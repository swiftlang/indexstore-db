//===--- IndexDatastore.cpp -----------------------------------------------===//
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

#include "IndexDatastore.h"
#include "StoreSymbolRecord.h"
#include "IndexStoreDB/Core/Symbol.h"
#include "IndexStoreDB/Index/FilePathIndex.h"
#include "IndexStoreDB/Index/SymbolIndex.h"
#include "IndexStoreDB/Index/IndexSystemDelegate.h"
#include "IndexStoreDB/Database/Database.h"
#include "IndexStoreDB/Database/DatabaseError.h"
#include "IndexStoreDB/Database/ImportTransaction.h"
#include "IndexStoreDB/Database/ReadTransaction.h"
#include "IndexStoreDB/Support/FilePathWatcher.h"
#include "IndexStoreDB/Support/Path.h"
#include "IndexStoreDB/Support/Concurrency.h"
#include "IndexStoreDB/Support/Logging.h"
#include "indexstore/IndexStoreCXX.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <dispatch/dispatch.h>
#include <Block.h>
#include <unordered_map>
#include <unordered_set>

// Dispatch on Linux doesn't have QOS_* macros.
#if !__has_include(<sys/qos.h>)
#define QOS_CLASS_UTILITY DISPATCH_QUEUE_PRIORITY_LOW
#endif

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;
using namespace IndexStoreDB::index;
using namespace indexstore;
using namespace llvm;
using namespace std::chrono;

static sys::TimePoint<> toTimePoint(timespec ts) {
  auto time = time_point_cast<nanoseconds>(sys::toTimePoint(ts.tv_sec));
  time += nanoseconds(ts.tv_nsec);
  return time;
}

const static dispatch_qos_class_t unitChangesQOS = QOS_CLASS_UTILITY;

/// Returns a global serial queue for unit processing.
/// This is useful to avoid doing a lot of parallel CPU and I/O work when opening multiple workspaces.
static dispatch_queue_t getGlobalQueueForUnitChanges() {
  static dispatch_queue_t queueForUnitChanges;
  static dispatch_once_t onceToken = 0;
  dispatch_once(&onceToken, ^{
    dispatch_queue_attr_t qosAttribute = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, unitChangesQOS, 0);
    queueForUnitChanges = dispatch_queue_create("IndexStoreDB.store.unit.processing", qosAttribute);
  });
  return queueForUnitChanges;
}

namespace {

  class UnitMonitor;

struct UnitEventInfo {
  IndexStore::UnitEvent::Kind kind;
  std::string name;
};

struct DoneInitState {
  std::atomic<bool> DoneInit{false};
};

class StoreUnitRepo : public std::enable_shared_from_this<StoreUnitRepo> {
  IndexStoreRef IdxStore;
  SymbolIndexRef SymIndex;
  std::shared_ptr<IndexSystemDelegate> Delegate;
  std::shared_ptr<CanonicalPathCache> CanonPathCache;

  std::shared_ptr<FilePathWatcher> PathWatcher;

  // This is shared so that it can be safely passed to an asynchronous block.
  std::shared_ptr<DoneInitState> DoneInitializingPtr;
  dispatch_semaphore_t InitSemaphore;

  mutable llvm::sys::Mutex StateMtx;
  std::unordered_map<IDCode, std::shared_ptr<UnitMonitor>> UnitMonitorsByCode;

public:
  StoreUnitRepo(IndexStoreRef IdxStore, SymbolIndexRef SymIndex,
                std::shared_ptr<IndexSystemDelegate> Delegate,
                std::shared_ptr<CanonicalPathCache> canonPathCache)
  : IdxStore(IdxStore),
    SymIndex(std::move(SymIndex)),
    Delegate(std::move(Delegate)),
    CanonPathCache(std::move(canonPathCache)) {

    DoneInitializingPtr = std::make_shared<DoneInitState>();
    InitSemaphore = dispatch_semaphore_create(0);
  }
  ~StoreUnitRepo() {
    dispatch_release(InitSemaphore);
  }

  void onFilesChange(std::vector<UnitEventInfo> evts,
                     function_ref<void(unsigned)> ReportCompleted,
                     function_ref<void()> DirectoryDeleted);

  void waitUntilDoneInitializing();
  void purgeStaleData();

  std::shared_ptr<UnitMonitor> getUnitMonitor(IDCode unitCode) const;
  void addUnitMonitor(IDCode unitCode, std::shared_ptr<UnitMonitor> monitor);
  void removeUnitMonitor(IDCode unitCode);

  void onUnitOutOfDate(IDCode unitCode, StringRef unitName,
                       sys::TimePoint<> outOfDateModTime,
                       OutOfDateTriggerHintRef hint,
                       bool synchronous = false);
  void onFSEvent(std::vector<std::string> parentPaths);
  void checkUnitContainingFileIsOutOfDate(StringRef file);

private:
  void registerUnit(StringRef UnitName);
  void removeUnit(StringRef UnitName);
};

class IndexDatastoreImpl {
  IndexStoreRef IdxStore;
  std::shared_ptr<StoreUnitRepo> UnitRepo;

public:
  bool init(IndexStoreRef idxStore,
            SymbolIndexRef SymIndex,
            std::shared_ptr<IndexSystemDelegate> Delegate,
            std::shared_ptr<CanonicalPathCache> CanonPathCache,
            bool readonly,
            std::string &Error);

  void waitUntilDoneInitializing();
  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime);
  void checkUnitContainingFileIsOutOfDate(StringRef file);
  void purgeStaleData();
};

class UnitMonitor {
  struct OutOfDateTrigger {
    OutOfDateTriggerHintRef hint;
    sys::TimePoint<> outOfDateModTime;

    std::string getTriggerFilePath() const {
      return hint->originalFileTrigger();
    }
  };

  std::weak_ptr<StoreUnitRepo> UnitRepo;
  IDCode UnitCode;
  std::string UnitName;
  sys::TimePoint<> ModTime;

  mutable llvm::sys::Mutex StateMtx;
  /// Map of out-of-date file path to its associated info.
  StringMap<OutOfDateTrigger> OutOfDateTriggers;

public:
  UnitMonitor(std::shared_ptr<StoreUnitRepo> unitRepo);

  void initialize(IDCode unitCode,
                  StringRef UnitName,
                  sys::TimePoint<> modTime,
                  ArrayRef<CanonicalFilePath> userFileDepends,
                  ArrayRef<IDCode> userUnitDepends);

  ~UnitMonitor();

  StringRef getUnitName() const { return UnitName; }
  sys::TimePoint<> getModTime() const { return ModTime; }

  std::vector<OutOfDateTrigger> getOutOfDateTriggers() const;

  void checkForOutOfDate(sys::TimePoint<> outOfDateModTime, StringRef filePath, bool synchronous=false);
  void markOutOfDate(sys::TimePoint<> outOfDateModTime, OutOfDateTriggerHintRef hint, bool synchronous=false);

  static std::pair<StringRef, sys::TimePoint<>> getMostRecentModTime(ArrayRef<StringRef> filePaths);
  static sys::TimePoint<> getModTimeForOutOfDateCheck(StringRef filePath);
};

} // anonymous namespace

//===----------------------------------------------------------------------===//
// StoreUnitRepo
//===----------------------------------------------------------------------===//

void StoreUnitRepo::onFilesChange(std::vector<UnitEventInfo> evts,
                                  function_ref<void(unsigned)> ReportCompleted,
                                  function_ref<void()> DirectoryDeleted) {

  auto guardForMapFullError = [&](function_ref<void()> block) {
    unsigned tries = 0;
    while (true) {
      try {
        ++tries;
        block();
        break;
      } catch (db::MapFullError err) {
        // If we hit the map size limit try again but only for a limited number of times.
        if (tries > 4) {
          // If it still fails after doubling the map size 4 times then something is going
          // wrong so give up.
          LOG_WARN("guardForMapFullError", "Still MDB_MAP_FULL error after increasing map size, tries: " << tries);
          throw;
        }
        SymIndex->getDBase()->increaseMapSize();
        // Try again.
      }
    }
  };

  for (const auto &evt : evts) {
    guardForMapFullError([&]{
      switch (evt.kind) {
      case IndexStore::UnitEvent::Kind::Added:
        registerUnit(evt.name);
        break;
      case IndexStore::UnitEvent::Kind::Removed:
        removeUnit(evt.name);
        break;
      case IndexStore::UnitEvent::Kind::Modified:
        registerUnit(evt.name);
        break;
      case IndexStore::UnitEvent::Kind::DirectoryDeleted:
        DirectoryDeleted();
        break;
      }
    });

    ReportCompleted(1);
  }

  // Can't just initialize this in the constructor because 'shared_from_this()'
  // cannot be called from a constructor.
  if (!PathWatcher) {
    std::weak_ptr<StoreUnitRepo> weakUnitRepo = shared_from_this();
    auto pathEventsReceiver = [weakUnitRepo](std::vector<std::string> paths) {
      if (auto unitRepo = weakUnitRepo.lock()) {
        unitRepo->onFSEvent(std::move(paths));
      }
    };
    PathWatcher = std::make_shared<FilePathWatcher>(std::move(pathEventsReceiver));
  }

  if (!DoneInitializingPtr->DoneInit) {
    dispatch_semaphore_signal(InitSemaphore);
    DoneInitializingPtr->DoneInit = true;
  }
}

void StoreUnitRepo::registerUnit(StringRef unitName) {
  std::string Error;
  auto optModTime = IdxStore->getUnitModificationTime(unitName, Error);
  if (!optModTime) {
    LOG_WARN_FUNC("error getting mod time for unit '" << unitName << "':" << Error);
    return;
  }
  auto unitModTime = toTimePoint(optModTime.getValue());

  std::unique_ptr<IndexUnitReader> readerPtr;
  auto getUnitReader = [&]() -> IndexUnitReader& {
    if (!readerPtr) {
      readerPtr.reset(new IndexUnitReader(*IdxStore, unitName, Error));
      if (readerPtr->isInvalid()) {
        LOG_WARN_FUNC("error loading unit  '" << unitName << "':" << Error);
      }
    }
    return *readerPtr;
  };

  IDCode unitCode;
  bool needDatabaseUpdate;
  Optional<bool> optIsSystem;
  IDCode PrevMainFileCode;
  IDCode PrevOutFileCode;
  Optional<StoreUnitInfo> StoreUnitInfoOpt;
  std::vector<CanonicalFilePath> UserFileDepends;
  std::vector<IDCode> UserUnitDepends;

  // Returns true if an error occurred.
  auto importUnit = [&]() -> bool {
    ImportTransaction import(SymIndex->getDBase());
    UnitDataImport unitImport(import, unitName, unitModTime);
    unitCode = unitImport.getUnitCode();
    needDatabaseUpdate = !unitImport.isUpToDate();
    optIsSystem = unitImport.getIsSystem();
    if (!needDatabaseUpdate) {
      PrevMainFileCode = unitImport.getPrevMainFileCode();
      PrevOutFileCode = unitImport.getPrevOutFileCode();
      return false;
    }

    auto &Reader = getUnitReader();
    if (Reader.isInvalid())
      return true;

    SymbolProviderKind symProviderKind = getSymbolProviderKindFromIdentifer(Reader.getProviderIdentifier()).getValue();
    optIsSystem = Reader.isSystemUnit();
    unitImport.setIsSystemUnit(optIsSystem.getValue());
    unitImport.setSymbolProviderKind(symProviderKind);
    unitImport.setTarget(Reader.getTarget());
    StringRef WorkDir = Reader.getWorkingDirectory();
    CanonicalFilePath CanonMainFile;
    bool hasMainFile = Reader.hasMainFile();
    if (hasMainFile) {
      CanonMainFile = CanonPathCache->getCanonicalPath(Reader.getMainFilePath(), WorkDir);
      unitImport.setMainFile(CanonMainFile);
    }
    CanonicalFilePath CanonOutFile = CanonPathCache->getCanonicalPath(Reader.getOutputFile(), WorkDir);
    unitImport.setOutFile(CanonOutFile);
    StoreUnitInfoOpt = StoreUnitInfo{unitName, CanonMainFile, CanonOutFile, unitModTime};

    CanonicalFilePath CanonSysroot = CanonPathCache->getCanonicalPath(Reader.getSysrootPath(), WorkDir);
    unitImport.setSysroot(CanonSysroot);

    Reader.foreachDependency([&](IndexUnitDependency Dep)->bool {
      switch (Dep.getKind()) {
        case IndexUnitDependency::DependencyKind::Record: {
          CanonicalFilePath CanonPath = CanonPathCache->getCanonicalPath(Dep.getFilePath(), WorkDir);
          if (CanonPath.empty())
            break;

          if (!Dep.isSystem())
            UserFileDepends.push_back(CanonPath);
          StringRef recordName = Dep.getName();
          bool isNewProvider;
          IDCode providerCode = unitImport.addProviderDependency(recordName, CanonPath, Dep.getModuleName(), Dep.isSystem(), &isNewProvider);
          if (!isNewProvider)
            break;

          std::string Error;
          auto Rec = StoreSymbolRecord::create(IdxStore, recordName, providerCode, symProviderKind, /*fileRefs=*/None);
          if (!Rec) {
            LOG_WARN_FUNC("error creating store symbol record: " << Error);
            break;
          }

          SymIndex->importSymbols(import, Rec);
          break;
        }

        case IndexUnitDependency::DependencyKind::Unit: {
          IDCode unitDepCode = unitImport.addUnitDependency(Dep.getName());
          if (!Dep.isSystem())
            UserUnitDepends.push_back(unitDepCode);
          break;
        }

        case IndexUnitDependency::DependencyKind::File: {
          CanonicalFilePath CanonPath = CanonPathCache->getCanonicalPath(Dep.getFilePath(), WorkDir);
          if (CanonPath.empty())
            break;

          if (!Dep.isSystem())
            UserFileDepends.push_back(CanonPath);
          unitImport.addFileDependency(CanonPath);
        }
      }
      return true;
    });

    unitImport.commit();
    import.commit();
    return false;
  };

  if (importUnit())
    return; // error occurred;

  if (Delegate) {
    if (!StoreUnitInfoOpt.hasValue()) {
      ReadTransaction reader(SymIndex->getDBase());
      CanonicalFilePath mainFile = reader.getFullFilePathFromCode(PrevMainFileCode);
      CanonicalFilePath outFile = reader.getFullFilePathFromCode(PrevOutFileCode);
      StoreUnitInfoOpt = StoreUnitInfo{unitName, mainFile, outFile, unitModTime};
    }
    Delegate->processedStoreUnit(StoreUnitInfoOpt.getValue());
  }

  if (*optIsSystem)
    return;

  // Monitor user files of the unit.

  // Get the user files if we didn't already go through them earlier.
  if (!needDatabaseUpdate) {
    auto &Reader = getUnitReader();
    if (Reader.isInvalid())
      return;

    StringRef WorkDir = Reader.getWorkingDirectory();
    Reader.foreachDependency([&](IndexUnitDependency Dep)->bool {
      switch (Dep.getKind()) {
        case IndexUnitDependency::DependencyKind::Unit:
          if (!Dep.isSystem())
            UserUnitDepends.push_back(makeIDCodeFromString(Dep.getName()));
          break;
        case IndexUnitDependency::DependencyKind::Record:
        case IndexUnitDependency::DependencyKind::File: {
          CanonicalFilePath CanonPath = CanonPathCache->getCanonicalPath(Dep.getFilePath(), WorkDir);
          if (CanonPath.empty())
            break;

          if (!Dep.isSystem())
            UserFileDepends.push_back(CanonPath);
        }
      }
      return true;
    });
  }

  auto localThis = shared_from_this();
  auto unitMonitor = std::make_shared<UnitMonitor>(localThis);
  unitMonitor->initialize(unitCode, unitName, unitModTime, UserFileDepends, UserUnitDepends);
  addUnitMonitor(unitCode, unitMonitor);
}

void StoreUnitRepo::removeUnit(StringRef unitName) {
  removeUnitMonitor(makeIDCodeFromString(unitName));

  ImportTransaction import(SymIndex->getDBase());
  import.removeUnitData(unitName);
  import.commit();
}

void StoreUnitRepo::purgeStaleData() {
  // FIXME: Get referenced records from the database.
  // IdxStore->purgeStaleRecords(ActiveRecNames);
}

void StoreUnitRepo::waitUntilDoneInitializing() {
  if (DoneInitializingPtr->DoneInit)
    return;
  dispatch_semaphore_wait(InitSemaphore, DISPATCH_TIME_FOREVER);
}

std::shared_ptr<UnitMonitor> StoreUnitRepo::getUnitMonitor(IDCode unitCode) const {
  sys::ScopedLock L(StateMtx);
  auto It = UnitMonitorsByCode.find(unitCode);
  if (It != UnitMonitorsByCode.end())
    return It->second;
  return nullptr;
}

void StoreUnitRepo::addUnitMonitor(IDCode unitCode, std::shared_ptr<UnitMonitor> monitor) {
  sys::ScopedLock L(StateMtx);
  UnitMonitorsByCode[unitCode] = monitor;
}

void StoreUnitRepo::removeUnitMonitor(IDCode unitCode) {
  sys::ScopedLock L(StateMtx);
  UnitMonitorsByCode.erase(unitCode);
}

void StoreUnitRepo::onUnitOutOfDate(IDCode unitCode, StringRef unitName,
                                    sys::TimePoint<> outOfDateModTime,
                                    OutOfDateTriggerHintRef hint,
                                    bool synchronous) {
  CanonicalFilePath MainFilePath;
  CanonicalFilePath OutFilePath;
  llvm::sys::TimePoint<> CurrModTime;
  SmallVector<IDCode, 8> dependentUnits;
  {
    ReadTransaction reader(SymIndex->getDBase());
    auto unitInfo = reader.getUnitInfo(unitCode);
    if (!unitInfo.isInvalid()) {
      if (unitInfo.HasMainFile) {
        MainFilePath = reader.getFullFilePathFromCode(unitInfo.MainFileCode);
      }
      OutFilePath = reader.getFullFilePathFromCode(unitInfo.OutFileCode);
      CurrModTime = unitInfo.ModTime;
    }
    reader.getDirectDependentUnits(unitCode, dependentUnits);
  }

  if (!MainFilePath.empty() && Delegate) {
    StoreUnitInfo unitInfo{unitName, MainFilePath, OutFilePath, CurrModTime};
    Delegate->unitIsOutOfDate(unitInfo, outOfDateModTime, hint, synchronous);
  }

  for (IDCode depUnit : dependentUnits) {
    if (auto monitor = getUnitMonitor(depUnit)) {
      if (monitor->getModTime() < outOfDateModTime)
        monitor->markOutOfDate(outOfDateModTime,
                               DependentUnitOutOfDateTriggerHint::create(unitName, hint),
                               synchronous);
    }
  }
}

void StoreUnitRepo::onFSEvent(std::vector<std::string> changedParentPaths) {
  std::vector<CanonicalFilePathRef> parentPathStrRefs;
  parentPathStrRefs.reserve(changedParentPaths.size());
  for (auto &path : changedParentPaths)
    parentPathStrRefs.push_back(CanonicalFilePathRef::getAsCanonicalPath(path));

  struct OutOfDateCheck {
    std::shared_ptr<UnitMonitor> Monitor;
    sys::TimePoint<> ModTime;
    CanonicalFilePath FilePath;
  };

  std::vector<OutOfDateCheck> outOfDateChecks;
  {
    ReadTransaction reader(SymIndex->getDBase());
    reader.findFilePathsWithParentPaths(parentPathStrRefs, [&](IDCode pathCode, CanonicalFilePathRef filePath) -> bool {
      auto modTime = UnitMonitor::getModTimeForOutOfDateCheck(filePath.getPath());
      reader.foreachUnitContainingFile(pathCode, [&](ArrayRef<IDCode> unitCodes) -> bool {
        for (IDCode unitCode : unitCodes) {
          if (auto monitor = getUnitMonitor(unitCode)) {
            outOfDateChecks.push_back(OutOfDateCheck{monitor, modTime, filePath});
          }
        }
        return true;
      });
      return true;
    });
  }
  // We collect and call later to avoid nested read transactions.
  for (auto &check : outOfDateChecks) {
    check.Monitor->checkForOutOfDate(check.ModTime, check.FilePath.getPath());
  }
}

void StoreUnitRepo::checkUnitContainingFileIsOutOfDate(StringRef filePath) {
  auto realPath = CanonPathCache->getCanonicalPath(filePath);
  filePath = realPath.getPath();

  // The timestamp that the file system returns has second precision, so if the file
  // was touched in less than a second after it got indexed, it will look like it is not actually dirty.
  // FIXME: Use modification-time + file-size to check for updated files.
  auto modTime = UnitMonitor::getModTimeForOutOfDateCheck(filePath);

  std::vector<std::shared_ptr<UnitMonitor>> unitMonitors;
  {
    ReadTransaction reader(SymIndex->getDBase());
    IDCode pathCode = reader.getFilePathCode(realPath);
    reader.foreachUnitContainingFile(pathCode, [&](ArrayRef<IDCode> unitCodes) -> bool {
      for (IDCode unitCode : unitCodes) {
        if (auto monitor = getUnitMonitor(unitCode)) {
          unitMonitors.push_back(std::move(monitor));
        }
      }
      return true;
    });
  }
  // We collect and call later to avoid nested read transactions.
  for (auto &unitMonitor : unitMonitors) {
    unitMonitor->checkForOutOfDate(modTime, filePath, /*synchronous=*/true);
  }
}

//===----------------------------------------------------------------------===//
// UnitMonitor
//===----------------------------------------------------------------------===//

UnitMonitor::UnitMonitor(std::shared_ptr<StoreUnitRepo> unitRepo) {
  this->UnitRepo = unitRepo;
}

void UnitMonitor::initialize(IDCode unitCode,
                             StringRef unitName,
                             sys::TimePoint<> modTime,
                             ArrayRef<CanonicalFilePath> userFileDepends,
                             ArrayRef<IDCode> userUnitDepends) {
  auto unitRepo = this->UnitRepo.lock();
  if (!unitRepo)
    return;
  this->UnitCode = unitCode;
  this->UnitName = unitName;
  this->ModTime = modTime;
  for (IDCode unitDepCode : userUnitDepends) {
    if (auto depMonitor = unitRepo->getUnitMonitor(unitDepCode)) {
      for (const auto &trigger : depMonitor->getOutOfDateTriggers()) {
        if (trigger.outOfDateModTime > modTime) {
          markOutOfDate(trigger.outOfDateModTime,
                        DependentUnitOutOfDateTriggerHint::create(depMonitor->getUnitName(),
                                                                  trigger.hint));
        }
      }
    }
  }

  SmallVector<StringRef, 32> filePaths;
  filePaths.reserve(userFileDepends.size());
  for (const auto &canonPath : userFileDepends) {
    filePaths.push_back(canonPath.getPath());
  }
  auto mostRecentFileAndTime = getMostRecentModTime(filePaths);
  if (mostRecentFileAndTime.second > modTime) {
    markOutOfDate(mostRecentFileAndTime.second, DependentFileOutOfDateTriggerHint::create(mostRecentFileAndTime.first));
    return;
  }
}

UnitMonitor::~UnitMonitor() {}

std::vector<UnitMonitor::OutOfDateTrigger> UnitMonitor::getOutOfDateTriggers() const {
  sys::ScopedLock L(StateMtx);
  std::vector<OutOfDateTrigger> triggers;
  for (const auto &entry : OutOfDateTriggers) {
    triggers.push_back(entry.getValue());
  }
  return triggers;
}

void UnitMonitor::checkForOutOfDate(sys::TimePoint<> outOfDateModTime, StringRef filePath, bool synchronous) {
  sys::ScopedLock L(StateMtx);
  auto findIt = OutOfDateTriggers.find(filePath);
  if (findIt != OutOfDateTriggers.end() && findIt->getValue().outOfDateModTime >= outOfDateModTime) {
    return; // already marked as out-of-date related to this trigger file.
  }
  if (ModTime < outOfDateModTime)
    markOutOfDate(outOfDateModTime, DependentFileOutOfDateTriggerHint::create(filePath), synchronous);
}

void UnitMonitor::markOutOfDate(sys::TimePoint<> outOfDateModTime, OutOfDateTriggerHintRef hint, bool synchronous) {
  {
    sys::ScopedLock L(StateMtx);
    OutOfDateTrigger trigger{ hint, outOfDateModTime};
    auto &entry = OutOfDateTriggers[trigger.getTriggerFilePath()];
    if (entry.outOfDateModTime >= outOfDateModTime)
      return; // already marked as out-of-date related to this trigger file.
    entry = trigger;
  }
  if (auto localUnitRepo = UnitRepo.lock())
    localUnitRepo->onUnitOutOfDate(UnitCode, UnitName, outOfDateModTime, hint, synchronous);
}

std::pair<StringRef, sys::TimePoint<>> UnitMonitor::getMostRecentModTime(ArrayRef<StringRef> filePaths) {
  sys::TimePoint<> mostRecentTime = sys::TimePoint<>::min();
  StringRef mostRecentFile;
  auto checkModTime = [&](sys::TimePoint<> mod, StringRef filePath) {
    if (mod > mostRecentTime) {
      mostRecentTime = mod;
      mostRecentFile = filePath;
    }
  };

  for (StringRef filePath : filePaths) {
    sys::fs::file_status fileStat;
    std::error_code EC = sys::fs::status(filePath, fileStat);
    sys::TimePoint<> currModTime = sys::TimePoint<>::min();
    if (sys::fs::status_known(fileStat) && fileStat.type() == sys::fs::file_type::file_not_found) {
      // Make a recent time value so that we consider this out-of-date.
      currModTime = std::chrono::system_clock::now();
    } else if (!EC) {
      currModTime = fileStat.getLastModificationTime();
    }
    checkModTime(currModTime, filePath);
  }

  return std::make_pair(mostRecentFile, mostRecentTime);
}

sys::TimePoint<> UnitMonitor::getModTimeForOutOfDateCheck(StringRef filePath) {
  sys::fs::file_status fileStat;
  std::error_code EC = sys::fs::status(filePath, fileStat);
  sys::TimePoint<> modTime = sys::TimePoint<>::min();
  if (sys::fs::status_known(fileStat) && fileStat.type() == sys::fs::file_type::file_not_found) {
    // Make a recent time value so that we consider this out-of-date.
    modTime = std::chrono::system_clock::now();
  } else if (!EC) {
    modTime = fileStat.getLastModificationTime();
  }
  return modTime;
}

//===----------------------------------------------------------------------===//
// IndexDatastoreImpl
//===----------------------------------------------------------------------===//

static const unsigned MAX_STORE_EVENTS_TO_PROCESS_PER_WORK_UNIT = 10;

namespace {
/// A thread-safe deque object for UnitEventInfo objects.
class UnitEventInfoDeque {
  std::deque<UnitEventInfo> EventsDequeue;
  mutable llvm::sys::Mutex StateMtx;

public:
  void addEvents(ArrayRef<UnitEventInfo> evts) {
    sys::ScopedLock L(StateMtx);
    EventsDequeue.insert(EventsDequeue.end(), evts.begin(), evts.end());
  }

  std::vector<UnitEventInfo> popFront(unsigned N) {
    sys::ScopedLock L(StateMtx);
    std::vector<UnitEventInfo> evts;
    for (unsigned i = 0; i < N; ++i) {
      if (EventsDequeue.empty())
        break;
      UnitEventInfo evt = EventsDequeue.front();
      EventsDequeue.pop_front();
      evts.push_back(std::move(evt));
    }
    return evts;
  }
};
}

/// Enqueues asynchronous processing of the unit events in an incremental fashion.
/// Events are queued-up individually and the next event is enqueued only after
/// the current one has been processed.
static void processUnitEventsIncrementally(std::shared_ptr<UnitEventInfoDeque> evts,
                                           std::weak_ptr<StoreUnitRepo> weakUnitRepo,
                                           std::shared_ptr<IndexSystemDelegate> delegate,
                                           dispatch_queue_t queue) {
  std::vector<UnitEventInfo> poppedEvts = evts->popFront(MAX_STORE_EVENTS_TO_PROCESS_PER_WORK_UNIT);
  if (poppedEvts.empty())
    return;
  auto UnitRepo = weakUnitRepo.lock();
  if (!UnitRepo)
    return;

  UnitRepo->onFilesChange(poppedEvts, [&](unsigned NumCompleted){
    delegate->processingCompleted(NumCompleted);
  }, [&](){
    // FIXME: the database should recover.
  });

  // Enqueue processing the rest of the events.
  dispatch_async(queue, ^{
    processUnitEventsIncrementally(evts, weakUnitRepo, delegate, queue);
  });
}

bool IndexDatastoreImpl::init(IndexStoreRef idxStore,
                              SymbolIndexRef SymIndex,
                              std::shared_ptr<IndexSystemDelegate> Delegate,
                              std::shared_ptr<CanonicalPathCache> CanonPathCache,
                              bool readonly,
                              std::string &Error) {
  this->IdxStore = std::move(idxStore);
  if (!this->IdxStore)
    return true;

  if (readonly)
    return false;

  auto UnitRepo = std::make_shared<StoreUnitRepo>(this->IdxStore, SymIndex, Delegate, CanonPathCache);
  std::weak_ptr<StoreUnitRepo> WeakUnitRepo = UnitRepo;
  auto eventsDeque = std::make_shared<UnitEventInfoDeque>();
  auto OnUnitsChange = [WeakUnitRepo, Delegate, eventsDeque](IndexStore::UnitEventNotification EventNote) {
    std::vector<UnitEventInfo> evts;
    for (size_t i = 0, e = EventNote.getEventsCount(); i != e; ++i) {
      auto evt = EventNote.getEvent(i);
      evts.push_back(UnitEventInfo{evt.getKind(), evt.getUnitName()});
    }

    Delegate->processingAddedPending(evts.size());
    eventsDeque->addEvents(evts);

    // Create the block with QoS explicitly to ensure that the QoS from the indexstore callback can't affect the onFilesChange priority. This call may do a lot of I/O and we don't want to wedge the system by running at elevated priority.
    dispatch_block_t onUnitChangeBlock = dispatch_block_create_with_qos_class(DISPATCH_BLOCK_INHERIT_QOS_CLASS, unitChangesQOS, 0, ^{
      // Pass registration events to be processed incrementally by the global serial queue.
      // This allows intermixing processing of registration events from multiple workspaces.
      processUnitEventsIncrementally(eventsDeque, WeakUnitRepo, Delegate, getGlobalQueueForUnitChanges());
    });
    dispatch_async(getGlobalQueueForUnitChanges(), onUnitChangeBlock);
    Block_release(onUnitChangeBlock);
  };

  this->IdxStore->setUnitEventHandler(OnUnitsChange);
  bool err = this->IdxStore->startEventListening(/*waitInitialSync=*/false, Error);
  if (err)
    return true;

  this->UnitRepo = std::move(UnitRepo);
  return false;
}

void IndexDatastoreImpl::waitUntilDoneInitializing() {
  if (UnitRepo)
    UnitRepo->waitUntilDoneInitializing();
}

bool IndexDatastoreImpl::isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles) {
  auto mostRecentFileAndTime = UnitMonitor::getMostRecentModTime(dirtyFiles);
  return isUnitOutOfDate(unitOutputPath, mostRecentFileAndTime.second);
}

bool IndexDatastoreImpl::isUnitOutOfDate(StringRef unitOutputPath, sys::TimePoint<> outOfDateModTime) {
  SmallString<128> nameBuf;
  IdxStore->getUnitNameFromOutputPath(unitOutputPath, nameBuf);
  StringRef unitName = nameBuf.str();
  std::string error;
  auto optUnitModTime = IdxStore->getUnitModificationTime(unitName, error);
  if (!optUnitModTime)
    return true;

  auto unitModTime = toTimePoint(optUnitModTime.getValue());
  return outOfDateModTime > unitModTime;
}

void IndexDatastoreImpl::checkUnitContainingFileIsOutOfDate(StringRef file) {
  UnitRepo->checkUnitContainingFileIsOutOfDate(file);
}

void IndexDatastoreImpl::purgeStaleData() {
  UnitRepo->purgeStaleData();
}

//===----------------------------------------------------------------------===//
// IndexDatastore
//===----------------------------------------------------------------------===//

std::unique_ptr<IndexDatastore>
IndexDatastore::create(IndexStoreRef idxStore,
                       SymbolIndexRef SymIndex,
                       std::shared_ptr<IndexSystemDelegate> Delegate,
                       std::shared_ptr<CanonicalPathCache> CanonPathCache,
                       bool readonly,
                       std::string &Error) {
  std::unique_ptr<IndexDatastoreImpl> Impl(new IndexDatastoreImpl());
  bool Err = Impl->init(std::move(idxStore), std::move(SymIndex), std::move(Delegate), std::move(CanonPathCache),
                        readonly, Error);
  if (Err)
    return nullptr;

  std::unique_ptr<IndexDatastore> Store;
  Store.reset(new IndexDatastore(Impl.release()));
  return Store;
}

#define IMPL static_cast<IndexDatastoreImpl*>(Impl)

IndexDatastore::~IndexDatastore() {
  delete IMPL;
}

void IndexDatastore::waitUntilDoneInitializing() {
  return IMPL->waitUntilDoneInitializing();
}

bool IndexDatastore::isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles) {
  return IMPL->isUnitOutOfDate(unitOutputPath, dirtyFiles);
}

bool IndexDatastore::isUnitOutOfDate(StringRef unitOutputPath, sys::TimePoint<> outOfDateModTime) {
  return IMPL->isUnitOutOfDate(unitOutputPath, outOfDateModTime);
}

void IndexDatastore::checkUnitContainingFileIsOutOfDate(StringRef file) {
  return IMPL->checkUnitContainingFileIsOutOfDate(file);
}

void IndexDatastore::purgeStaleData() {
  return IMPL->purgeStaleData();
}
