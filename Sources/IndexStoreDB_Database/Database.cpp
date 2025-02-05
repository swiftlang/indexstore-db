//===--- Database.cpp -----------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "DatabaseImpl.h"
#include <IndexStoreDB_Core/Symbol.h>
#include <IndexStoreDB_Database/UnitInfo.h>
#include <IndexStoreDB_Support/Logging.h>
#include <IndexStoreDB_Support/Path.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_Hashing.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringMap.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_STLExtras.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Errc.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_FileSystem.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Mutex.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Path.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_raw_ostream.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_WindowsError.h>
#if defined(_WIN32)
#define NOMINMAX
#include "Windows.h"
#include "winbase.h"
#endif

#if defined(_WIN32)
typedef DWORD indexstorePid_t;
#else
typedef pid_t indexstorePid_t;
#endif

// Dispatch on Linux doesn't have QOS_* macros.
#if !__has_include(<sys/qos.h>)
#define QOS_CLASS_BACKGROUND DISPATCH_QUEUE_PRIORITY_BACKGROUND
#endif

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;

const unsigned Database::DATABASE_FORMAT_VERSION = 13;

static const char *DeadProcessDBSuffix = "-dead";

static std::error_code renameDirectory(const Twine &from, const Twine &to) {
  // llvm::sys::fs::rename is not able to rename directories on Windows. Use `MoveFile` directly.
  #if defined(_WIN32)
  if (!llvm::sys::fs::exists(from)) {
    return make_error_code(llvm::errc::no_such_file_or_directory);
  }
  SmallVector<wchar_t, 128> wideFrom;
  if (std::error_code ec = llvm::sys::path::widenPath(from, wideFrom)) {
    return ec;
  }
  SmallVector<wchar_t, 128> wideTo;
  if (std::error_code ec = llvm::sys::path::widenPath(to, wideTo)) {
    return ec;
  }
  // MoveFileW does not override an existing directory. Remove the destination if it is an empty directory.
  ::RemoveDirectoryW(wideTo.begin());
  if (!::MoveFileW(wideFrom.begin(), wideTo.begin())) {
    return llvm::mapWindowsError(GetLastError());
  }
  return std::error_code();
  #else
  return llvm::sys::fs::rename(from, to);
  #endif
}

static int providersForUSR_compare(const MDB_val *a, const MDB_val *b) {
  assert(a->mv_size == sizeof(ProviderForUSRData));
  assert(b->mv_size == sizeof(ProviderForUSRData));
  ProviderForUSRData *lhs = (ProviderForUSRData*)a->mv_data;
  ProviderForUSRData *rhs = (ProviderForUSRData*)b->mv_data;
  // A provider will be associated with a USR only once, and the roles are auxiliary data.
  // So only compare using the provider and ignore roles. This will allow updating the roles
  // that a USR has for a particular provider.
  return IDCode::compare(lhs->ProviderCode, rhs->ProviderCode);
}

static int filesForProvider_compare(const MDB_val *a, const MDB_val *b) {
  assert(a->mv_size == sizeof(TimestampedFileForProviderData));
  assert(b->mv_size == sizeof(TimestampedFileForProviderData));
  TimestampedFileForProviderData *lhs = (TimestampedFileForProviderData*)a->mv_data;
  TimestampedFileForProviderData *rhs = (TimestampedFileForProviderData*)b->mv_data;
  // A file+unit will be associated with a provider only once, and the timestamp is auxiliary data.
  // So only compare using the file+unit and ignore timestamp. This will allow updating the timestamp only.
  // Compare using FileCode first, so we can go through the multiple FileCode entries to look for the
  // most recent mod-time from a FileCode/UnitCode pair.
  int comp = IDCode::compare(lhs->FileCode, rhs->FileCode);
  if (comp != 0)
    return comp;
  return IDCode::compare(lhs->UnitCode, rhs->UnitCode);
}

/// Returns a global serial queue for stale database removal.
static dispatch_queue_t getDiscardedDBsCleanupQueue() {
  static dispatch_queue_t queue;
  static dispatch_once_t onceToken = 0;
  dispatch_once(&onceToken, ^{
    dispatch_queue_attr_t qosAttribute = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_BACKGROUND, 0);
    queue = dispatch_queue_create("indexstoredb.db.discarded_dbs_cleanup", qosAttribute);
  });
  return queue;
}

Database::Implementation::Implementation() {
  ReadTxnGroup = dispatch_group_create();
  TxnSyncQueue = dispatch_queue_create("indexstoredb.db.txn_sync", DISPATCH_QUEUE_CONCURRENT);
}
Database::Implementation::~Implementation() {
  if (!IsReadOnly) {
    DBEnv.close();
    assert(!SavedPath.empty() && !UniquePath.empty());
    // In case some other process already created the 'saved' path, override it so
    // that the 'last one wins'.
    renameDirectory(SavedPath, llvm::Twine(UniquePath)+"-saved"+DeadProcessDBSuffix);
    if (std::error_code ec = renameDirectory(UniquePath, SavedPath)) {
      // If the database directory already got removed or some other process beat
      // us during the tiny window between the above 2 renames, then give-up,
      // and let the database to be discarded.
      LOG_INFO_FUNC(High, "failed moving " << llvm::sys::path::filename(UniquePath) << " directory to 'saved': " << ec.message());
    }
  }

  dispatch_release(ReadTxnGroup);
  dispatch_release(TxnSyncQueue);
}

std::shared_ptr<Database::Implementation>
Database::Implementation::create(StringRef path, bool readonly, Optional<size_t> initialDBSize, std::string &error) {
  SmallString<10> versionStr;
  llvm::raw_svector_ostream(versionStr) << 'v' << Database::DATABASE_FORMAT_VERSION;
  SmallString<128> versionPath = path;
  llvm::sys::path::append(versionPath, versionStr);

  SmallString<128> savedPathBuf = versionPath;
  llvm::sys::path::append(savedPathBuf, "saved");
  SmallString<128> prefixPathBuf = versionPath;
#if defined(WIN32)
  llvm::raw_svector_ostream(prefixPathBuf) << "\\p" << GetCurrentProcessId();
#else
  llvm::raw_svector_ostream(prefixPathBuf) << "/p" << getpid();
#endif
  llvm::raw_svector_ostream(prefixPathBuf) << "-";
  SmallString<128> uniqueDirPath;

  bool existingDB = true;

  auto createDirectoriesOrError = [&error](SmallVectorImpl<char> &path) {
    if (std::error_code ec = llvm::sys::fs::create_directories(path)) {
      llvm::raw_string_ostream(error) << "failed creating directory '" << path << "': " << ec.message();
      return true;
    }
    return false;
  };
  auto createUniqueDirOrError = [&error, &prefixPathBuf, &uniqueDirPath]() {
    uniqueDirPath.clear();
    if (std::error_code ec = llvm::sys::fs::createUniqueDirectory(prefixPathBuf, uniqueDirPath)) {
      llvm::raw_string_ostream(error) << "failed creating directory '" << uniqueDirPath << "': " << ec.message();
      return true;
    }
    return false;
  };

  StringRef dbPath;
  if (!readonly) {
    if (createDirectoriesOrError(versionPath))
      return nullptr;

    // Move the currently stored database to a unique directory to isolate it.
    // When the database closes it moves the unique directory back to
    // the '/saved' one. If we crash before closing, then we'll discard the database
    // that is left in the unique directory that includes the process pid number.
    if (createUniqueDirOrError())
      return nullptr;

    // This succeeds for moving to an empty directory, like the newly constructed `uniqueDirPath`.
    if (renameDirectory(savedPathBuf, uniqueDirPath)) {
      // No existing database, just use the new directory.
      existingDB = false;
    }
    dbPath = uniqueDirPath;
  } else {
    dbPath = savedPathBuf;
  }

retry:
  try {
    auto db = std::make_shared<Database::Implementation>();
    db->IsReadOnly = readonly;
    db->VersionedPath = versionPath.str();
    db->SavedPath = savedPathBuf.str();
    db->UniquePath = uniqueDirPath.str();
    db->DBEnv = lmdb::env::create();
    db->DBEnv.set_max_dbs(14);

    uint64_t dbFileSize = 0;
    if (existingDB) {
      if (std::error_code ec = llvm::sys::fs::file_size(dbPath + "/data.mdb", dbFileSize)) {
        LOG_WARN_FUNC("failed reading database file size " << dbPath << "/data.mdb: " << ec.message());
      }
    }
    // Start with 64MB.
    uint64_t initialSize = initialDBSize.getValueOr(64ULL*1024ULL*1024ULL);

    db->MapSize = std::max(dbFileSize, initialSize);
    db->DBEnv.set_mapsize(db->MapSize);

    unsigned openflags = MDB_NOMEMINIT|MDB_WRITEMAP|MDB_NOSYNC;
    if (readonly)
      openflags |= MDB_RDONLY;
    db->DBEnv.open(dbPath, openflags);
    db->MaxKeySize = lmdb::env_get_max_keysize(db->DBEnv);

    unsigned txnflags = lmdb::txn::default_flags;
    if (readonly)
      txnflags |= MDB_RDONLY;
    auto txn = lmdb::txn::begin(db->DBEnv, /*parent=*/nullptr, txnflags);
    db->DBISymbolProvidersByUSR = lmdb::dbi::open(txn, "usrs", MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED|MDB_CREATE);
    db->DBISymbolProvidersByUSR.set_dupsort(txn, providersForUSR_compare);
    db->DBISymbolProviderNameByCode = lmdb::dbi::open(txn, "providers", MDB_INTEGERKEY|MDB_CREATE);
    db->DBISymbolProvidersWithTestSymbols = lmdb::dbi::open(txn, "providers-with-test-symbols", MDB_INTEGERKEY|MDB_CREATE);
    db->DBIUSRsBySymbolName = lmdb::dbi::open(txn, "symbol-names", MDB_DUPSORT|MDB_DUPFIXED|MDB_INTEGERDUP|MDB_CREATE);
    db->DBIUSRsByGlobalSymbolKind = lmdb::dbi::open(txn, "symbol-kinds", MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED|MDB_INTEGERDUP|MDB_CREATE);
    db->DBIDirNameByCode = lmdb::dbi::open(txn, "directories", MDB_INTEGERKEY|MDB_CREATE);
    db->DBIFilenameByCode = lmdb::dbi::open(txn, "filenames", MDB_INTEGERKEY|MDB_CREATE);
    db->DBIFilePathCodesByDir = lmdb::dbi::open(txn, "filepaths-by-directory", MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED|MDB_INTEGERDUP|MDB_CREATE);
    db->DBITimestampedFilesByProvider = lmdb::dbi::open(txn, "provider-files", MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED|MDB_CREATE);
    db->DBITimestampedFilesByProvider.set_dupsort(txn, filesForProvider_compare);
    db->DBIUnitInfoByCode = lmdb::dbi::open(txn, "unit-info", MDB_INTEGERKEY|MDB_CREATE);
    db->DBIUnitByFileDependency = lmdb::dbi::open(txn, "unit-by-file", MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED|MDB_INTEGERDUP|MDB_CREATE);
    db->DBIUnitByUnitDependency = lmdb::dbi::open(txn, "unit-by-unit", MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED|MDB_INTEGERDUP|MDB_CREATE);
    db->DBITargetNameByCode = lmdb::dbi::open(txn, "target-names", MDB_INTEGERKEY|MDB_CREATE);
    db->DBIModuleNameByCode = lmdb::dbi::open(txn, "module-names", MDB_INTEGERKEY|MDB_CREATE);
    txn.commit();

    db->cleanupDiscardedDBs();

    return db;

  } catch (lmdb::error err) {
    if (existingDB) {
      // If opening an existing database fails, create a new database. This
      // prevents a corrupted database from preventing all progress.

      // Since db is destroyed, the database is now at the saved path; move it
      // aside to a 'corrupted' path we can find it for analysis.
      SmallString<128> corruptedPathBuf = versionPath;
      llvm::sys::path::append(corruptedPathBuf, "corrupted");
      renameDirectory(corruptedPathBuf, llvm::Twine(corruptedPathBuf)+DeadProcessDBSuffix);
      renameDirectory(savedPathBuf, corruptedPathBuf);
      LOG_WARN_FUNC("failed opening database: " << err.description() << "\n"
                    "corrupted database saved at '" << corruptedPathBuf << "'\n"
                    "creating new database...");

      // Recreate the unique database path for the next attempt.
      if (!readonly && createUniqueDirOrError())
        return nullptr;
      existingDB = false;
      goto retry;
    }
    llvm::raw_string_ostream(error) << "failed opening database: " << err.description();
    return nullptr;
  }
}

UnitInfo Database::Implementation::getUnitInfo(IDCode unitCode, lmdb::txn &Txn) {
  lmdb::val key{&unitCode, sizeof(unitCode)};
  lmdb::val value{};
  bool found = getDBIUnitInfoByCode().get(Txn, key, value);
  if (!found)
    return UnitInfo{ StringRef(), unitCode };

  UnitInfoData infoData;
  ArrayRef<IDCode> fileDepends;
  ArrayRef<IDCode> unitDepends;
  ArrayRef<UnitInfo::Provider> providerDepends;
  StringRef unitName;

  char *ptr = value.data();
  memcpy(&infoData, ptr, sizeof(infoData));
  ptr += sizeof(infoData);
  assert(llvm::alignmentAdjustment(ptr, alignof(IDCode)) == 0 && "misaligned IDCode");
  fileDepends = llvm::makeArrayRef((IDCode*)ptr, infoData.FileDependSize);
  ptr += sizeof(IDCode)*fileDepends.size();
  unitDepends = llvm::makeArrayRef((IDCode*)ptr, infoData.UnitDependSize);
  ptr += sizeof(IDCode)*unitDepends.size();
  providerDepends = llvm::makeArrayRef((UnitInfo::Provider*)ptr, infoData.ProviderDependSize);
  ptr += sizeof(UnitInfo::Provider)*providerDepends.size();
  unitName = StringRef(ptr, infoData.NameLength);

  llvm::sys::TimePoint<> modTime = llvm::sys::TimePoint<>(std::chrono::nanoseconds(infoData.NanoTime));
  return UnitInfo{ unitName, unitCode, modTime,
    infoData.OutFileCode, infoData.MainFileCode, infoData.SysrootCode, infoData.TargetCode,
    infoData.HasMainFile, infoData.HasSysroot, infoData.IsSystem, infoData.HasTestSymbols,
    SymbolProviderKind(infoData.SymProviderKind),
    fileDepends, unitDepends, providerDepends };
}

void Database::Implementation::enterReadTransaction() {
  // Prevent the read transaction from starting if increaseMapSize() is running.
  dispatch_sync(TxnSyncQueue, ^{
    dispatch_group_enter(ReadTxnGroup);
  });
}

void Database::Implementation::exitReadTransaction() {
  dispatch_group_leave(ReadTxnGroup);
}

void Database::Implementation::increaseMapSize() {
  // Prevent new read transactions from starting.
  dispatch_barrier_sync(TxnSyncQueue, ^{
    // Wait until all pending read transactions are finished.
    dispatch_group_wait(ReadTxnGroup, DISPATCH_TIME_FOREVER);
    // Double the map size;
    MapSize *= 2;
    DBEnv.set_mapsize(MapSize);
  });
  LOG_INFO_FUNC(High, "increased lmdb map size to: " << MapSize);
}

static bool isProcessStillExecuting(indexstorePid_t PID) {
#if defined(_WIN32)
  HANDLE hProcess;
  hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, PID);
  if (hProcess == NULL)
    return false;

  DWORD dwExitCode;
  bool result = GetExitCodeProcess(hProcess, &dwExitCode);
  return result && (dwExitCode == STILL_ACTIVE);
#else
  if (getsid(PID) == -1 && errno == ESRCH)
    return false;
  return true;
#endif
}

// This runs in a background priority queue.
static void cleanupDiscardedDBsImpl(StringRef versionedPath) {
  using namespace llvm::sys::fs;

  // Finds database subdirectories that are considered dead and removes them.
  // A directory is dead if it has been marked with the suffix "-dead" or if it
  // has the name "p<PID>-*" where process PID is no longer running.

#if defined(WIN32)
  indexstorePid_t currPID = GetCurrentProcessId();
#else
  indexstorePid_t currPID = getpid();
#endif

  std::error_code EC;
  directory_iterator Begin(versionedPath, EC);
  directory_iterator End;
  while (Begin != End) {
    auto &Item = *Begin;
    StringRef currPath = Item.path();

    auto shouldRemove = [](StringRef fullpath, indexstorePid_t currPID) -> bool {
      StringRef path = llvm::sys::path::filename(fullpath);
      if (path.endswith(DeadProcessDBSuffix))
        return true;
      if (!path.startswith("p"))
        return false;
      StringRef pidStr = path.substr(1);
      size_t dashIdx = pidStr.find('-');
      if (dashIdx == StringRef::npos)
        return false;
      pidStr = pidStr.substr(0, dashIdx);
      size_t pathPID;
      if (pidStr.getAsInteger(10, pathPID))
        return false;
      if ((indexstorePid_t)pathPID == currPID)
        return false;
      return !isProcessStillExecuting((indexstorePid_t)pathPID);
    };

    if (shouldRemove(currPath, currPID)) {
      // FIXME: With `IgnoreErrors` set to true it can hit an assertion if an error occurs.
      remove_directories(currPath, /*IgnoreErrors=*/false);
    }

    Begin.increment(EC);
  }
}

void Database::Implementation::cleanupDiscardedDBs() {
  std::string localVersionedPath = VersionedPath;
  dispatch_async(getDiscardedDBsCleanupQueue(), ^{
    cleanupDiscardedDBsImpl(localVersionedPath);
  });
}

void Database::Implementation::printStats(raw_ostream &OS) {
  OS << "\n*** Database Statistics\n";
  auto txn = lmdb::txn::begin(DBEnv, nullptr, MDB_RDONLY);
  auto printDBStats = [&](lmdb::dbi &db, StringRef name) {
    OS << "DB " << name << '\n';
    MDB_stat st = db.stat(txn);
    OS << "depth: " << st.ms_depth << '\n';
    OS << "branch pages: " << st.ms_branch_pages << '\n';
    OS << "leaf pages: " << st.ms_leaf_pages << '\n';
    OS << "overflow pages: " << st.ms_overflow_pages << '\n';
    OS << "entries: " << st.ms_entries << '\n';
    OS << "---\n";
  };
  printDBStats(DBISymbolProvidersByUSR, "SymbolProvidersByUSR");
  printDBStats(DBISymbolProviderNameByCode, "SymbolProviderNameByCode");
  printDBStats(DBIUSRsBySymbolName, "USRsBySymbolName");
  printDBStats(DBIUSRsByGlobalSymbolKind, "USRsBySymbolKind");
  printDBStats(DBIDirNameByCode, "DirNameByCode");
  printDBStats(DBIFilenameByCode, "FilenameByCode");
  printDBStats(DBIFilePathCodesByDir, "FilePathCodesByDir");
  printDBStats(DBITimestampedFilesByProvider, "TimestampedFilesByProvider");
  printDBStats(DBIUnitInfoByCode, "UnitInfoByCode");
  printDBStats(DBIUnitByFileDependency, "UnitByFileDependency");
  printDBStats(DBIUnitByUnitDependency, "UnitByUnitDependency");
  printDBStats(DBITargetNameByCode, "TargetNameByCode");
  printDBStats(DBIModuleNameByCode, "ModuleNameByCode");
}

// LMDB prohibits opening an LMDB database twice in the same process at the same time.
// To protect against this, use a global map based on the database filepath.
// This allows referring to the same database from multiple index clients and addresses
// racing issues where a new index client opens the same database before another client
// had the chance to close it.
static std::shared_ptr<Database::Implementation>
getLMDBDatabaseRefForPath(StringRef dbPath, bool readonly, Optional<size_t> initialDBSize, std::string &error) {
  static llvm::sys::Mutex processDatabasesMtx;
  static llvm::StringMap<std::weak_ptr<Database::Implementation>> databasesByPath;

  // Note that canonicalization of the path may result in different paths, if the
  // path doesn't exist yet vs the path exists. Use the path as given by the client.

  llvm::sys::ScopedLock L(processDatabasesMtx);
  std::weak_ptr<Database::Implementation> &dbWeakRef = databasesByPath[dbPath];
  if (auto dbRef = dbWeakRef.lock()) {
    return dbRef;
  }
  auto dbRef = Database::Implementation::create(dbPath, readonly, initialDBSize, error);
  if (!dbRef)
    return nullptr;
  dbWeakRef = dbRef;
  return dbRef;
}

DatabaseRef Database::create(StringRef dbPath, bool readonly, Optional<size_t> initialDBSize, std::string &error) {
  auto impl = getLMDBDatabaseRefForPath(dbPath, readonly, initialDBSize, error);
  if (!impl)
    return nullptr;

  auto db = std::make_shared<Database>();
  db->Impl = std::move(impl);
  return db;
}

Database::~Database() {}

void Database::increaseMapSize() {
  return Impl->increaseMapSize();
}

void Database::printStats(raw_ostream &OS) {
  return Impl->printStats(OS);
}

IDCode db::makeIDCodeFromString(StringRef name) {
  uint64_t code = hash_value(name);
  return IDCode::fromValue(code);
}

Optional<GlobalSymbolKind> db::getGlobalSymbolKind(SymbolKind K) {
  switch (K) {
    case SymbolKind::Unknown:
    case SymbolKind::Module:
    case SymbolKind::Namespace:
    case SymbolKind::NamespaceAlias:
    case SymbolKind::Macro:
    case SymbolKind::Extension:
    case SymbolKind::Field:
    case SymbolKind::Parameter:
    case SymbolKind::EnumConstant:
    case SymbolKind::InstanceMethod:
    case SymbolKind::ClassMethod:
    case SymbolKind::StaticMethod:
    case SymbolKind::InstanceProperty:
    case SymbolKind::ClassProperty:
    case SymbolKind::StaticProperty:
    case SymbolKind::Constructor:
    case SymbolKind::Destructor:
    case SymbolKind::ConversionFunction:
      return None;

    case SymbolKind::Enum:
      return GlobalSymbolKind::Enum;
    case SymbolKind::Struct:
      return GlobalSymbolKind::Struct;
    case SymbolKind::Class:
      return GlobalSymbolKind::Class;
    case SymbolKind::Protocol:
      return GlobalSymbolKind::Protocol;
    case SymbolKind::Union:
      return GlobalSymbolKind::Union;
    case SymbolKind::TypeAlias:
      return GlobalSymbolKind::Type;
    case SymbolKind::Function:
      return GlobalSymbolKind::Function;
    case SymbolKind::Variable:
      return GlobalSymbolKind::GlobalVar;
    case SymbolKind::Concept:
      return GlobalSymbolKind::Concept;
    case SymbolKind::CommentTag:
      return GlobalSymbolKind::CommentTag;
  }
}
