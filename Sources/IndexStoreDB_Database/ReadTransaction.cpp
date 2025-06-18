//===--- ReadTransaction.cpp ----------------------------------------------===//
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

#include "ReadTransactionImpl.h"
#include <IndexStoreDB_Support/Path.h>
#include <IndexStoreDB_Support/PatternMatching.h>
#include <IndexStoreDB_Support/Logging.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_ArrayRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Path.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_raw_ostream.h>

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;

ReadTransactionGuard::ReadTransactionGuard(DatabaseRef dbase) : DBase(dbase) {
  DBase->impl().enterReadTransaction();
}
ReadTransactionGuard::~ReadTransactionGuard() {
  DBase->impl().exitReadTransaction();
}

ReadTransaction::Implementation::Implementation(DatabaseRef dbase)
  : DBase(dbase), TxnGuard(dbase) {
  Txn = lmdb::txn::begin(DBase->impl().getDBEnv(), /*parent=*/nullptr, MDB_RDONLY);
}

bool ReadTransaction::Implementation::lookupProvidersForUSR(StringRef USR, SymbolRoleSet rolesToLookup, SymbolRoleSet relatedRolesToLookup,
                                                            llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver) {
  return lookupProvidersForUSR(makeIDCodeFromString(USR), rolesToLookup, relatedRolesToLookup, std::move(receiver));
}

bool ReadTransaction::Implementation::lookupProvidersForUSR(IDCode usrCode, SymbolRoleSet rolesToLookup, SymbolRoleSet relatedRolesToLookup,
                                                            llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver) {
  auto &db = DBase->impl();
  auto &dbiProvidersByUSR = db.getDBISymbolProvidersByUSR();
  auto cursorUSR = lmdb::cursor::open(Txn, dbiProvidersByUSR);

  auto handleEntry = [&](const ProviderForUSRData &entry) -> bool {
    if ((!rolesToLookup || (entry.Roles & rolesToLookup.toRaw())) &&
        (!relatedRolesToLookup || (entry.RelatedRoles & relatedRolesToLookup.toRaw()))) {
      return receiver(entry.ProviderCode, SymbolRoleSet(entry.Roles), SymbolRoleSet(entry.RelatedRoles));
    }
    return true;
  };

  lmdb::val key{&usrCode, sizeof(usrCode)};
  lmdb::val value{};
  bool found = cursorUSR.get(key, value, MDB_SET_KEY);
  if (!found)
    return true;

  size_t numItems = cursorUSR.count();
  if (numItems == 1) {
    const ProviderForUSRData &entry = *(ProviderForUSRData*)value.data();
    return handleEntry(entry);
  } else {
    // The first one is returned again with MDB_NEXT_MULTIPLE.
    while (cursorUSR.get(key, value, MDB_NEXT_MULTIPLE)) {
      assert(value.size() % sizeof(ProviderForUSRData) == 0);
      ProviderForUSRData *entryPtr = (ProviderForUSRData*)value.data();
      size_t entryCount = value.size() / sizeof(ProviderForUSRData);
      auto entries = llvm::makeArrayRef(entryPtr, entryCount);
      for (auto &entry : entries) {
        bool cont = handleEntry(entry);
        if (!cont)
          return false;
      }
    }
  }

  return true;
}

StringRef ReadTransaction::Implementation::getProviderName(IDCode providerCode) {
  lmdb::val key{&providerCode, sizeof(providerCode)};
  lmdb::val data{};
  if (!DBase->impl().getDBISymbolProviderNameByCode().get(Txn, key, data)) {
    LOG_WARN_FUNC("provider code not found");
    return StringRef();
  }
  return StringRef(data.data(), data.size());
}

StringRef ReadTransaction::Implementation::getTargetName(IDCode targetCode) {
  if (targetCode == IDCode()) {
    return StringRef();
  }

  lmdb::val key{&targetCode, sizeof(targetCode)};
  lmdb::val data{};
  if (!DBase->impl().getDBITargetNameByCode().get(Txn, key, data)) {
    LOG_WARN_FUNC("target code not found");
    return StringRef();
  }
  return StringRef(data.data(), data.size());
}

StringRef ReadTransaction::Implementation::getModuleName(IDCode moduleNameCode) {
  if (moduleNameCode == IDCode()) {
    return StringRef();
  }

  lmdb::val key{&moduleNameCode, sizeof(moduleNameCode)};
  lmdb::val data{};
  if (!DBase->impl().getDBIModuleNameByCode().get(Txn, key, data)) {
    LOG_WARN_FUNC("module name code not found");
    return StringRef();
  }
  return StringRef(data.data(), data.size());
}

bool ReadTransaction::Implementation::getProviderFileReferences(IDCode provider,
    llvm::function_ref<bool(TimestampedPath path)> receiver) {
  auto unitFilter = [](IDCode unitCode)->bool { return true; };
  return getProviderFileCodeReferences(provider, unitFilter, [&](IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem) -> bool {
    std::string pathString;
    llvm::raw_string_ostream OS(pathString);
    if (!getFullFilePathFromCode(pathCode, OS)) {
      LOG_WARN_FUNC("path of provider file not found");
      return true;
    }
    auto unitInfo = getUnitInfo(unitCode);
    CanonicalFilePathRef sysroot;
    if (unitInfo.HasSysroot) {
      sysroot = getDirectoryFromCode(unitInfo.SysrootCode);
    }
    StringRef moduleName = getModuleName(moduleNameCode);
    return receiver(TimestampedPath{std::move(OS.str()), modTime, moduleName, isSystem, sysroot});
  });
}

/// `unitFilter` returns `true` if the unit should be included, `false` if it should be ignored.
static bool passFileReferencesForProviderCursor(lmdb::val &key,
                                                lmdb::val &value,
                                                lmdb::cursor &cursor,
                                                function_ref<bool(IDCode unitCode)> unitFilter,
                                                llvm::function_ref<bool(IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver) {
  // Entries are sorted by file code and there can be multiple same file entries
  // from different units. We want to pass each file only once with its most recent
  // timestamp. Visit the entries and keep track of current file and recent timestamp;
  // when file changes pass it to the receiver.
  Optional<IDCode> currFileCode;
  Optional<IDCode> currUnitCode;
  llvm::sys::TimePoint<> currModTime;
  IDCode currModuleNameCode;
  bool currIsSystem = false;
  auto passCurrFile = [&]() -> bool {
    return receiver(*currFileCode, *currUnitCode, currModTime, currModuleNameCode, currIsSystem);
  };

  do {
    const auto &entry = *(TimestampedFileForProviderData*)value.data();
    llvm::sys::TimePoint<> modTime = llvm::sys::TimePoint<>(std::chrono::nanoseconds(entry.NanoTime));
    if (!currFileCode) {
      if (unitFilter(entry.UnitCode)) {
        currFileCode = entry.FileCode;
        currUnitCode = entry.UnitCode;
        currModTime = modTime;
        currModuleNameCode = entry.ModuleNameCode;
        currIsSystem = entry.IsSystem;
      }
    } else if (currFileCode.getValue() == entry.FileCode) {
      if (currModTime < modTime) {
        if (unitFilter(entry.UnitCode)) {
          currModTime = modTime;
          currUnitCode = entry.UnitCode;
        }
      }
    } else {
      if (!passCurrFile())
        return false;
      if (unitFilter(entry.UnitCode)) {
        currFileCode = entry.FileCode;
        currUnitCode = entry.UnitCode;
        currModTime = modTime;
        currModuleNameCode = entry.ModuleNameCode;
        currIsSystem = entry.IsSystem;
      } else {
        currFileCode.reset();
        currUnitCode.reset();
      }
    }
  } while (cursor.get(key, value, MDB_NEXT_DUP));

  if (currFileCode) {
    return passCurrFile();
  } else {
    return true;
  }
}

bool ReadTransaction::Implementation::getProviderFileCodeReferences(IDCode provider,
    function_ref<bool(IDCode unitCode)> unitFilter,
    function_ref<bool(IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver) {
  auto &db = DBase->impl();
  auto &dbiFilesByProvider = db.getDBITimestampedFilesByProvider();
  auto cursor = lmdb::cursor::open(Txn, dbiFilesByProvider);

  lmdb::val key{&provider, sizeof(provider)};
  lmdb::val value{};
  bool found = cursor.get(key, value, MDB_SET_KEY);
  if (!found)
    return true;

  return passFileReferencesForProviderCursor(key, value, cursor, std::move(unitFilter), std::move(receiver));
}

bool ReadTransaction::Implementation::foreachProviderAndFileCodeReference(
    function_ref<bool(IDCode unitCode)> unitFilter,
    function_ref<bool(IDCode provider, IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver) {
  auto &db = DBase->impl();
  auto &dbiFilesByProvider = db.getDBITimestampedFilesByProvider();
  auto cursor = lmdb::cursor::open(Txn, dbiFilesByProvider);

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT_NODUP)) {
    IDCode providerCode = *(IDCode*)key.data();
    bool cont = passFileReferencesForProviderCursor(key, value, cursor, unitFilter, [&](IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem) -> bool {
      return receiver(providerCode, pathCode, unitCode, modTime, moduleNameCode, isSystem);
    });
    if (!cont)
      return false;
  }
  return true;
}

static bool passMultipleIDCodes(lmdb::cursor &cursor, lmdb::val &key, lmdb::val &value,
                                llvm::function_ref<bool(ArrayRef<IDCode> codes)> receiver) {
  size_t numItems = cursor.count();
  if (numItems == 1) {
    IDCode usrCode;
    memcpy(&usrCode, value.data(), sizeof(usrCode));
    if (!receiver(usrCode))
      return false;
  } else {
    // The first one is returned again with MDB_NEXT_MULTIPLE.
    while (cursor.get(key, value, MDB_NEXT_MULTIPLE)) {
      assert(value.size() % sizeof(IDCode) == 0);
      size_t entryCount = value.size() / sizeof(IDCode);
      SmallVector<IDCode, 16> entries(entryCount);
      // Note: the IDCodes in lmdb may be misaligned, so memcpy them to a
      // temporary buffer.
      memcpy(entries.data(), value.data(), entries.size() * sizeof(entries[0]));
      if (!receiver(entries))
        return false;
    }
  }
  return true;
}

bool ReadTransaction::Implementation::foreachProviderContainingTestSymbols(function_ref<bool(IDCode provider)> receiver) {
  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBISymbolProvidersWithTestSymbols());

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT)) {
    IDCode providerCode = *(IDCode*)key.data();
    if (!receiver(providerCode))
      return false;
  }
  return true;
}

bool ReadTransaction::Implementation::foreachUSROfGlobalSymbolKind(SymbolKind symKind,
                                                             llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  auto globalKindOpt = getGlobalSymbolKind(symKind);
  if (!globalKindOpt.hasValue())
    return true;
  return foreachUSROfGlobalSymbolKind(globalKindOpt.getValue(), receiver);
}

bool ReadTransaction::Implementation::foreachUSROfGlobalUnitTestSymbol(function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  bool cont = foreachUSROfGlobalSymbolKind(GlobalSymbolKind::TestClassOrExtension, receiver);
  if (cont) {
    cont = foreachUSROfGlobalSymbolKind(GlobalSymbolKind::TestMethod, receiver);
  }
  return cont;
}

bool ReadTransaction::Implementation::foreachUSROfGlobalSymbolKind(GlobalSymbolKind globalKind,
                                                                   function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBIUSRsByGlobalSymbolKind());
  lmdb::val key{&globalKind, sizeof(globalKind)};
  lmdb::val value{};
  bool found = cursor.get(key, value, MDB_SET_KEY);
  if (!found)
    return true;

  return passMultipleIDCodes(cursor, key, value, receiver);
}

bool ReadTransaction::Implementation::findUSRsWithNameContaining(StringRef pattern,
                                                                 bool anchorStart, bool anchorEnd,
                                                                 bool subsequence, bool ignoreCase,
                                                                 llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  auto &db = DBase->impl();
  auto &dbiNames = db.getDBIUSRsBySymbolName();
  auto cursor = lmdb::cursor::open(Txn, dbiNames);

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT_NODUP)) {
    StringRef name{key.data(), key.size()};
    if (!matchesPattern(name, pattern, anchorStart, anchorEnd, subsequence, ignoreCase))
      continue;

    if (!passMultipleIDCodes(cursor, key, value, receiver))
      return false;
  }
  return true;
}

bool ReadTransaction::Implementation::foreachUSRBySymbolName(StringRef name,
                                                     llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  auto &db = DBase->impl();
  auto &dbiNames = db.getDBIUSRsBySymbolName();
  auto cursor = lmdb::cursor::open(Txn, dbiNames);

  lmdb::val key{name};
  lmdb::val value{};
  bool found = cursor.get(key, value, MDB_SET_KEY);
  if (!found)
    return true;

  return passMultipleIDCodes(cursor, key, value, receiver);
}

bool ReadTransaction::Implementation::foreachSymbolName(function_ref<bool(StringRef name)> receiver) {
  auto &db = DBase->impl();
  auto &dbiNames = db.getDBIUSRsBySymbolName();
  auto cursor = lmdb::cursor::open(Txn, dbiNames);

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT_NODUP)) {
    StringRef name{key.data(), key.size()};
    if (!receiver(name))
      return false;
  }
  return true;
}

bool ReadTransaction::Implementation::findFilenamesContaining(StringRef pattern,
                                                              bool anchorStart, bool anchorEnd,
                                                              bool subsequence, bool ignoreCase,
                                                              llvm::function_ref<bool(CanonicalFilePathRef filePath)> receiver) {
  auto &db = DBase->impl();
  auto &dbiFilenames = db.getDBIFilenameByCode();
  auto cursor = lmdb::cursor::open(Txn, dbiFilenames);

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT)) {
    IDCode dirCode;
    StringRef fileName;
    std::tie(dirCode, fileName) = decomposeFilePathValue(value);

    // FIXME: When adding a path in the database mark it explicitly whether it
    // should be searchable or not. For now workaround the issue by excluding
    // output filenames.
    StringRef ext = llvm::sys::path::extension(fileName);
    if (ext == ".o" || ext == ".pcm")
      continue;

    if (!matchesPattern(fileName, pattern, anchorStart, anchorEnd, subsequence, ignoreCase))
      continue;

    CanonicalFilePath canonPath = getFilePathFromValue(value);
    if (!canonPath.empty()) {
      if (!receiver(canonPath))
        return false;
    }
  }
  return true;
}

bool ReadTransaction::Implementation::getFullFilePathFromCode(IDCode filePathCode, raw_ostream &OS) {
  auto &db = DBase->impl();
  auto &dbiFilenames = db.getDBIFilenameByCode();

  lmdb::val key{&filePathCode, sizeof(filePathCode)};
  lmdb::val value{};
  bool found = dbiFilenames.get(Txn, key, value);
  if (!found)
    return false;

  return getFilePathFromValue(value, OS);
}

CanonicalFilePath ReadTransaction::Implementation::getFullFilePathFromCode(IDCode filePathCode) {
  SmallString<128> path;
  {
    llvm::raw_svector_ostream OS(path);
    getFullFilePathFromCode(filePathCode, OS);
  }
  return CanonicalFilePathRef::getAsCanonicalPath(path);
}

std::string ReadTransaction::Implementation::getUnitFileIdentifierFromCode(IDCode filePathCode) {
  std::string path;
  {
    llvm::raw_string_ostream OS(path);
    getFullFilePathFromCode(filePathCode, OS);
  }
  return path;
}

CanonicalFilePathRef ReadTransaction::Implementation::getDirectoryFromCode(IDCode dirCode) {
  lmdb::val key{&dirCode, sizeof(dirCode)};
  lmdb::val value{};
  if (!DBase->impl().getDBIDirNameByCode().get(Txn, key, value)) {
    LOG_WARN_FUNC("directory code not found");
    return CanonicalFilePathRef();
  }
  return CanonicalFilePathRef::getAsCanonicalPath({value.data(), value.size()});
}

bool ReadTransaction::Implementation::foreachDirPath(llvm::function_ref<bool(CanonicalFilePathRef dirPath)> receiver) {
  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBIDirNameByCode());

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT)) {
    StringRef dirPath(value.data(), value.size());
    if (!receiver(CanonicalFilePathRef::getAsCanonicalPath(dirPath)))
      return false;
  }
  return true;
}

bool ReadTransaction::Implementation::findFilePathsWithParentPaths(ArrayRef<CanonicalFilePathRef> origParentPaths,
                   llvm::function_ref<bool(IDCode pathCode, CanonicalFilePathRef filePath)> receiver) {
  // Do cleanup of the path if it ends with '/'.
  SmallVector<StringRef, 8> parentPaths;
  parentPaths.reserve(origParentPaths.size());
  for (CanonicalFilePathRef canonPath : origParentPaths) {
    StringRef path = canonPath.getPath();
    while (!path.empty() && path.back() == '/')
      path = path.drop_back();
    if (!path.empty())
      parentPaths.push_back(path);
  }
  if (parentPaths.empty())
    return true;

  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBIFilePathCodesByDir());

  auto filePathCodesReceiver = [&](ArrayRef<IDCode> codes) -> bool {
    SmallString<256> pathBuf;
    for (IDCode pathCode : codes) {
      pathBuf.clear();
      {
        llvm::raw_svector_ostream OS(pathBuf);
        if (!getFullFilePathFromCode(pathCode, OS))
          continue;
      }
      bool cont = receiver(pathCode, CanonicalFilePathRef::getAsCanonicalPath(pathBuf.str()));
      if (!cont)
        return false;
    }
    return true;
  };

  for (StringRef parentPath : parentPaths) {
    IDCode dirCode = getFilePathCode(CanonicalFilePathRef::getAsCanonicalPath(parentPath));
    lmdb::val key{&dirCode, sizeof(dirCode)};
    lmdb::val value{};
    bool found = cursor.get(key, value, MDB_SET_KEY);
    if (!found)
      continue;
    bool cont = passMultipleIDCodes(cursor, key, value, filePathCodesReceiver);
    if (!cont)
      return false;
  }

  return true;
}

IDCode ReadTransaction::Implementation::getFilePathCode(CanonicalFilePathRef filePath) {
  return makeIDCodeFromString(filePath.getPath());
}

IDCode ReadTransaction::Implementation::getUnitPathCode(StringRef filePath) {
  return makeIDCodeFromString(filePath);
}

bool ReadTransaction::Implementation::getFilePathFromValue(lmdb::val &filePathValue, raw_ostream &OS) {
  auto &db = DBase->impl();
  auto &dbiDirNames = db.getDBIDirNameByCode();

  IDCode dirCode;
  StringRef fileName;
  std::tie(dirCode, fileName) = decomposeFilePathValue(filePathValue);

  lmdb::val key{&dirCode, sizeof(dirCode)};
  lmdb::val value{};
  bool found = dbiDirNames.get(Txn, key, value);
  if (found)
    OS << StringRef(value.data(), value.size());
  OS << llvm::sys::path::get_separator();
  OS << fileName;
  return true;
}

CanonicalFilePath ReadTransaction::Implementation::getFilePathFromValue(lmdb::val &filePathValue) {
  SmallString<128> path;
  {
    llvm::raw_svector_ostream OS(path);
    getFilePathFromValue(filePathValue, OS);
  }
  return CanonicalFilePathRef::getAsCanonicalPath(path);
}

std::pair<IDCode, StringRef> ReadTransaction::Implementation::decomposeFilePathValue(lmdb::val &filePathValue) {
  IDCode dirCode;
  memcpy(&dirCode, filePathValue.data(), sizeof(dirCode));
  StringRef fileName = StringRef(filePathValue.data()+sizeof(dirCode), filePathValue.size()-sizeof(dirCode));
  return std::make_pair(dirCode, fileName);
}

UnitInfo ReadTransaction::Implementation::getUnitInfo(IDCode unitCode) {
  auto &db = DBase->impl();
  return db.getUnitInfo(unitCode, Txn);
}

UnitInfo ReadTransaction::Implementation::getUnitInfo(StringRef unitName) {
  return getUnitInfo(makeIDCodeFromString(unitName));
}

bool ReadTransaction::Implementation::foreachUnitContainingFile(IDCode filePathCode,
                                                                llvm::function_ref<bool(ArrayRef<IDCode> unitCodes)> receiver) {
  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBIUnitByFileDependency());
  lmdb::val key{&filePathCode, sizeof(filePathCode)};
  lmdb::val value{};
  bool found = cursor.get(key, value, MDB_SET_KEY);
  if (!found)
    return true;

  return passMultipleIDCodes(cursor, key, value, receiver);
}

LLVM_DUMP_METHOD void ReadTransaction::Implementation::dumpUnitByFilePair() {

  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBIUnitByFileDependency());

  lmdb::val key{};
  lmdb::val value{};
  while (cursor.get(key, value, MDB_NEXT)) {
    IDCode filePathCode = *(IDCode*)key.data();
    IDCode unitCode = *(IDCode*)value.data();

    CanonicalFilePath filePath = getFullFilePathFromCode(filePathCode);
    UnitInfo unitInfo = getUnitInfo(unitCode);
    llvm::errs() << filePath.getPath() << " -> " << unitInfo.UnitName << '\n';
  }
}

bool ReadTransaction::Implementation::foreachUnitContainingUnit(IDCode unitCode,
                                                                llvm::function_ref<bool(ArrayRef<IDCode> unitCodes)> receiver) {
  auto &db = DBase->impl();
  auto cursor = lmdb::cursor::open(Txn, db.getDBIUnitByUnitDependency());
  lmdb::val key{&unitCode, sizeof(unitCode)};
  lmdb::val value{};
  bool found = cursor.get(key, value, MDB_SET_KEY);
  if (!found)
    return true;

  return passMultipleIDCodes(cursor, key, value, receiver);
}

void ReadTransaction::Implementation::collectRootUnits(
                             IDCode unitCode,
                             SmallVectorImpl<UnitInfo> &rootUnits,
                             std::unordered_set<IDCode> &visited) {
  if (visited.count(unitCode))
    return;
  visited.insert(unitCode);

  auto unitInfo = getUnitInfo(unitCode);
  if (unitInfo.isInvalid())
    return;

  if (unitInfo.HasMainFile) {
    rootUnits.push_back(std::move(unitInfo));
    return;
  }

  foreachUnitContainingUnit(unitCode, [&](ArrayRef<IDCode> contUnits) -> bool {
    for (IDCode contUnit : contUnits)
      collectRootUnits(contUnit, rootUnits, visited);
    return true;
  });
}

bool ReadTransaction::Implementation::foreachRootUnitOfFile(IDCode pathCode,
    function_ref<bool(const UnitInfo &unitInfo)> receiver) {
  SmallVector<UnitInfo, 32> rootUnits;
  std::unordered_set<IDCode> visited;

  foreachUnitContainingFile(pathCode, [&](ArrayRef<IDCode> unitCodes) -> bool {
    for (IDCode unitCode : unitCodes) {
      collectRootUnits(unitCode, rootUnits, visited);
    }
    return true;
  });

  for (auto &root : rootUnits) {
    if (!receiver(root))
      return false;
  }
  return true;
}

bool ReadTransaction::Implementation::foreachRootUnitOfUnit(IDCode unitCode,
    function_ref<bool(const UnitInfo &unitInfo)> receiver) {
  SmallVector<UnitInfo, 32> rootUnits;
  std::unordered_set<IDCode> visited;
  collectRootUnits(unitCode, rootUnits, visited);

  for (auto &root : rootUnits) {
    if (!receiver(root))
      return false;
  }
  return true;
}

void ReadTransaction::Implementation::getDirectDependentUnits(IDCode unitCode, SmallVectorImpl<IDCode> &units) {
  foreachUnitContainingUnit(unitCode, [&](ArrayRef<IDCode> contUnits) -> bool {
    units.append(contUnits.begin(), contUnits.end());
    return true;
  });
}

ReadTransaction::ReadTransaction(DatabaseRef dbase)
  : Impl(new Implementation(std::move(dbase))) {}

ReadTransaction::~ReadTransaction() {}

bool ReadTransaction::lookupProvidersForUSR(StringRef USR, SymbolRoleSet roles, SymbolRoleSet relatedRoles,
                                            llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver) {
  return Impl->lookupProvidersForUSR(USR, roles, relatedRoles, std::move(receiver));
}

bool ReadTransaction::lookupProvidersForUSR(IDCode usrCode, SymbolRoleSet roles, SymbolRoleSet relatedRoles,
                                            llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver) {
  return Impl->lookupProvidersForUSR(usrCode, roles, relatedRoles, std::move(receiver));
}

StringRef ReadTransaction::getProviderName(IDCode provider) {
  return Impl->getProviderName(provider);
}

StringRef ReadTransaction::getTargetName(IDCode target) {
  return Impl->getTargetName(target);
}

StringRef ReadTransaction::getModuleName(IDCode moduleName) {
  return Impl->getModuleName(moduleName);
}

bool ReadTransaction::getProviderFileReferences(IDCode provider,
    llvm::function_ref<bool(TimestampedPath path)> receiver) {
  return Impl->getProviderFileReferences(provider, std::move(receiver));
}

bool ReadTransaction::getProviderFileCodeReferences(IDCode provider,
    function_ref<bool(IDCode unitCode)> unitFilter,
    function_ref<bool(IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver) {
  return Impl->getProviderFileCodeReferences(provider, std::move(unitFilter), std::move(receiver));
}

bool ReadTransaction::foreachProviderAndFileCodeReference(
    function_ref<bool(IDCode unitCode)> unitFilter,
    function_ref<bool(IDCode provider, IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver) {
  return Impl->foreachProviderAndFileCodeReference(std::move(unitFilter), std::move(receiver));
}

bool ReadTransaction::foreachProviderContainingTestSymbols(function_ref<bool(IDCode provider)> receiver) {
  return Impl->foreachProviderContainingTestSymbols(std::move(receiver));
}

bool ReadTransaction::foreachUSROfGlobalSymbolKind(SymbolKind symKind, llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  return Impl->foreachUSROfGlobalSymbolKind(symKind, std::move(receiver));
}

bool ReadTransaction::foreachUSROfGlobalUnitTestSymbol(llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  return Impl->foreachUSROfGlobalUnitTestSymbol(std::move(receiver));
}

bool ReadTransaction::findUSRsWithNameContaining(StringRef pattern,
                                                 bool anchorStart, bool anchorEnd,
                                                 bool subsequence, bool ignoreCase,
                                                 llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  return Impl->findUSRsWithNameContaining(pattern, anchorStart, anchorEnd, subsequence, ignoreCase, std::move(receiver));
}

bool ReadTransaction::foreachUSRBySymbolName(StringRef name, llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver) {
  return Impl->foreachUSRBySymbolName(name, std::move(receiver));
}

bool ReadTransaction::findFilenamesContaining(StringRef pattern,
                                              bool anchorStart, bool anchorEnd,
                                              bool subsequence, bool ignoreCase,
                                              llvm::function_ref<bool(CanonicalFilePathRef filePath)> receiver) {
  return Impl->findFilenamesContaining(pattern, anchorStart, anchorEnd, subsequence, ignoreCase, std::move(receiver));
}

bool ReadTransaction::foreachSymbolName(function_ref<bool(StringRef name)> receiver) {
  return Impl->foreachSymbolName(std::move(receiver));
}

bool ReadTransaction::getFullFilePathFromCode(IDCode filePathCode, raw_ostream &OS) {
  return Impl->getFullFilePathFromCode(filePathCode, OS);
}

CanonicalFilePath ReadTransaction::getFullFilePathFromCode(IDCode filePathCode) {
  return Impl->getFullFilePathFromCode(filePathCode);
}

std::string ReadTransaction::getUnitFileIdentifierFromCode(IDCode filePathCode) {
  return Impl->getUnitFileIdentifierFromCode(filePathCode);
}

CanonicalFilePathRef ReadTransaction::getDirectoryFromCode(IDCode dirCode) {
  return Impl->getDirectoryFromCode(dirCode);
}

bool ReadTransaction::foreachDirPath(llvm::function_ref<bool(CanonicalFilePathRef dirPath)> receiver) {
  return Impl->foreachDirPath(std::move(receiver));
}

bool ReadTransaction::findFilePathsWithParentPaths(ArrayRef<CanonicalFilePathRef> parentPaths,
                                                   llvm::function_ref<bool(IDCode pathCode, CanonicalFilePathRef filePath)> receiver) {
  return Impl->findFilePathsWithParentPaths(parentPaths, std::move(receiver));
}

IDCode ReadTransaction::getFilePathCode(CanonicalFilePathRef filePath) {
  return Impl->getFilePathCode(filePath);
}

IDCode ReadTransaction::getUnitFileIdentifierCode(StringRef filePath) {
  return Impl->getUnitPathCode(filePath);
}

UnitInfo ReadTransaction::getUnitInfo(IDCode unitCode) {
  return Impl->getUnitInfo(unitCode);
}

UnitInfo ReadTransaction::getUnitInfo(StringRef unitName) {
  return Impl->getUnitInfo(unitName);
}

bool ReadTransaction::foreachUnitContainingFile(IDCode filePathCode,
                                                llvm::function_ref<bool(ArrayRef<IDCode> unitCodes)> receiver) {
  return Impl->foreachUnitContainingFile(filePathCode, std::move(receiver));
}

bool ReadTransaction::foreachUnitContainingUnit(IDCode unitCode,
                                                llvm::function_ref<bool(ArrayRef<IDCode> unitCodes)> receiver) {
  return Impl->foreachUnitContainingUnit(unitCode, std::move(receiver));
}

bool ReadTransaction::foreachRootUnitOfFile(IDCode filePathCode,
                                            function_ref<bool(const UnitInfo &unitInfo)> receiver) {
  return Impl->foreachRootUnitOfFile(filePathCode, std::move(receiver));
}

bool ReadTransaction::foreachRootUnitOfUnit(IDCode unitCode,
                                            function_ref<bool(const UnitInfo &unitInfo)> receiver) {
  return Impl->foreachRootUnitOfUnit(unitCode, std::move(receiver));
}

void ReadTransaction::getDirectDependentUnits(IDCode unitCode, SmallVectorImpl<IDCode> &units) {
  return Impl->getDirectDependentUnits(unitCode, units);
}
