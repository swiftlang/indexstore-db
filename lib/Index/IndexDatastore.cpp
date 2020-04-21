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
#include "llvm/ADT/StringSet.h"
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
  class UnitProcessingSession;

struct UnitEventInfo {
  IndexStore::UnitEvent::Kind kind;
  std::string name;
  /// Whether this is an explicit enqueue of a dependency unit for processing, while `UseExplicitOutputUnits` is enabled.
  bool isDependency;

  UnitEventInfo(IndexStore::UnitEvent::Kind kind, std::string name, bool isDependency = false)
  : kind(kind), name(std::move(name)), isDependency(isDependency) {}
};

struct DoneInitState {
  std::atomic<bool> DoneInit{false};
  unsigned RemainingInitUnits = 0; // this is not accessed concurrently.
};

struct PollUnitsState {
  llvm::sys::Mutex pollMtx;
  llvm::StringMap<sys::TimePoint<>> knownUnits;
};

class StoreUnitRepo : public std::enable_shared_from_this<StoreUnitRepo> {
  IndexStoreRef IdxStore;
  SymbolIndexRef SymIndex;
  const bool UseExplicitOutputUnits;
  std::shared_ptr<IndexSystemDelegate> Delegate;
  std::shared_ptr<CanonicalPathCache> CanonPathCache;

  std::shared_ptr<FilePathWatcher> PathWatcher;

  DoneInitState InitializingState;
  dispatch_semaphore_t InitSemaphore;

  PollUnitsState pollUnitsState;

  mutable llvm::sys::Mutex StateMtx;
  std::unordered_map<IDCode, std::shared_ptr<UnitMonitor>> UnitMonitorsByCode;

  std::unordered_set<db::IDCode> ExplicitOutputUnitsSet;

public:
  StoreUnitRepo(IndexStoreRef IdxStore, SymbolIndexRef SymIndex,
                bool useExplicitOutputUnits,
                std::shared_ptr<IndexSystemDelegate> Delegate,
                std::shared_ptr<CanonicalPathCache> canonPathCache)
  : IdxStore(IdxStore),
    SymIndex(std::move(SymIndex)),
    UseExplicitOutputUnits(useExplicitOutputUnits),
    Delegate(std::move(Delegate)),
    CanonPathCache(std::move(canonPathCache)) {
    InitSemaphore = dispatch_semaphore_create(0);
  }
  ~StoreUnitRepo() {
    dispatch_release(InitSemaphore);
  }

  void onFilesChange(std::vector<UnitEventInfo> evts,
                     std::shared_ptr<UnitProcessingSession> processSession,
                     function_ref<void(unsigned)> ReportCompleted,
                     function_ref<void()> DirectoryDeleted);

  void setInitialUnitCount(unsigned count);
  void processedInitialUnitCount(unsigned count);
  void finishedUnitInitialization();
  void waitUntilDoneInitializing();

  /// *For Testing* Poll for any changes to units and wait until they have been registered.
  void pollForUnitChangesAndWait();

  std::shared_ptr<UnitProcessingSession> makeUnitProcessingSession();

  void addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing);
  void removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing);
  bool isUnitNameInKnownOutFilePaths(StringRef unitName) const;

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
  void registerUnit(StringRef UnitName, std::shared_ptr<UnitProcessingSession> processSession);
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
            bool useExplicitOutputUnits,
            bool readonly,
            bool listenToUnitEvents,
            std::string &Error);

  void waitUntilDoneInitializing();
  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime);
  void checkUnitContainingFileIsOutOfDate(StringRef file);

  void addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing);
  void removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing);

  void purgeStaleData();

  /// *For Testing* Poll for any changes to units and wait until they have been registered.
  void pollForUnitChangesAndWait();
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

  bool hasEnqueuedUnitDependency(StringRef unitName) const {
    sys::ScopedLock L(StateMtx);
    for (const auto &evt : EventsDequeue) {
      if (evt.isDependency && evt.name == unitName)
        return true;
    }
    return false;
  }
};

/// Encapsulates state for processing a number of units and handles asynchronous (or synchronous for testing) scheduling.
class UnitProcessingSession : public std::enable_shared_from_this<UnitProcessingSession> {
  std::shared_ptr<UnitEventInfoDeque> Deque;
  std::weak_ptr<StoreUnitRepo> WeakUnitRepo;
  std::shared_ptr<IndexSystemDelegate> Delegate;

  static const unsigned MAX_STORE_EVENTS_TO_PROCESS_PER_WORK_UNIT = 10;

public:
  UnitProcessingSession(std::shared_ptr<UnitEventInfoDeque> eventsDeque,
                        std::weak_ptr<StoreUnitRepo> unitRepo,
                        std::shared_ptr<IndexSystemDelegate> delegate)
  : Deque(std::move(eventsDeque)), WeakUnitRepo(std::move(unitRepo)),
    Delegate(std::move(delegate)) {
  }

  void process(std::vector<UnitEventInfo> evts, bool waitForProcessing) {
    if (evts.empty()) {
      return; // bail out early if there's no work.
    }

    enqueue(std::move(evts));

    if (waitForProcessing) {
      processUnitsAndWait();
    } else {
      processUnitsAsync();
    }
  }

  /// Enqueue units for processing and return.
  /// This should be used when `process()` has already be called on this session object.
  void enqueue(std::vector<UnitEventInfo> evts) {
    if (evts.empty())
      return;
    Delegate->processingAddedPending(evts.size());
    Deque->addEvents(std::move(evts));
  }

  bool hasEnqueuedUnitDependency(StringRef unitName) const {
    return Deque->hasEnqueuedUnitDependency(unitName);
  }

private:
  void processUnitsAsync() {
    auto session = shared_from_this();
    auto onUnitChangeBlockImpl = ^{
      // Pass registration events to be processed incrementally by the global serial queue.
      // This allows intermixing processing of registration events from multiple workspaces.
      session->processUnitEventsIncrementally(getGlobalQueueForUnitChanges());
    };

#if defined(__APPLE__)
    // Create the block with QoS explicitly to ensure that the QoS from the indexstore callback can't affect the onFilesChange priority. This call may do a lot of I/O and we don't want to wedge the system by running at elevated priority.
    dispatch_block_t onUnitChangeBlock = dispatch_block_create_with_qos_class(DISPATCH_BLOCK_INHERIT_QOS_CLASS, unitChangesQOS, 0, onUnitChangeBlockImpl);
#else
    // FIXME: https://bugs.swift.org/browse/SR-10319
    auto onUnitChangeBlock = Block_copy(onUnitChangeBlockImpl);
#endif
    dispatch_async(getGlobalQueueForUnitChanges(), onUnitChangeBlock);
    Block_release(onUnitChangeBlock);
  }

  /// Primarily used for testing.
  void processUnitsAndWait() {
    auto unitRepo = WeakUnitRepo.lock();
    if (!unitRepo)
      return;

    while (true) {
      std::vector<UnitEventInfo> evts = Deque->popFront(MAX_STORE_EVENTS_TO_PROCESS_PER_WORK_UNIT);
      if (evts.empty()) {
        break;
      }

      dispatch_sync(getGlobalQueueForUnitChanges(), ^{
        unitRepo->onFilesChange(std::move(evts), shared_from_this(), [&](unsigned numCompleted){
          Delegate->processingCompleted(numCompleted);
        }, []{
          // FIXME: the database should recover.
        });
      });
    }
  }

  /// Enqueues asynchronous processing of the unit events in an incremental fashion.
  /// Events are queued-up individually and the next event is enqueued only after
  /// the current one has been processed.
  void processUnitEventsIncrementally(dispatch_queue_t queue) {
    std::vector<UnitEventInfo> poppedEvts = Deque->popFront(MAX_STORE_EVENTS_TO_PROCESS_PER_WORK_UNIT);
    if (poppedEvts.empty())
      return;
    auto unitRepo = WeakUnitRepo.lock();
    if (!unitRepo)
      return;

    auto session = shared_from_this();

    unitRepo->onFilesChange(poppedEvts, session, [&](unsigned NumCompleted){
      Delegate->processingCompleted(NumCompleted);
    }, [&](){
      // FIXME: the database should recover.
    });

    // Enqueue processing the rest of the events.
    dispatch_async(queue, ^{
      session->processUnitEventsIncrementally(queue);
    });
  }
};

} // end anonymous namespace

void StoreUnitRepo::onFilesChange(std::vector<UnitEventInfo> evts,
                                  std::shared_ptr<UnitProcessingSession> processSession,
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

  auto shouldIgnore = [&](const UnitEventInfo &evt) -> bool {
    if (!UseExplicitOutputUnits)
      return false;
    if (evt.isDependency)
      return false;
    return !isUnitNameInKnownOutFilePaths(evt.name);
  };

  for (const auto &evt : evts) {
    guardForMapFullError([&]{
      switch (evt.kind) {
      case IndexStore::UnitEvent::Kind::Added:
      case IndexStore::UnitEvent::Kind::Modified:
        if (!shouldIgnore(evt)) {
          registerUnit(evt.name, processSession);
        }
        break;
      case IndexStore::UnitEvent::Kind::Removed:
        removeUnit(evt.name);
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

  if (!InitializingState.DoneInit) {
    processedInitialUnitCount(evts.size());
  }
}

void StoreUnitRepo::registerUnit(StringRef unitName, std::shared_ptr<UnitProcessingSession> processSession) {
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
  Optional<bool> PrevHasTestSymbols;
  IDCode PrevMainFileCode;
  IDCode PrevOutFileCode;
  Optional<StoreUnitInfo> StoreUnitInfoOpt;
  std::vector<CanonicalFilePath> UserFileDepends;
  std::vector<IDCode> UserUnitDepends;

  SmallVector<std::string, 16> unitDependencies;

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
      PrevHasTestSymbols = unitImport.getHasTestSymbols();
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

    CanonicalFilePath CanonSysroot = CanonPathCache->getCanonicalPath(Reader.getSysrootPath(), WorkDir);
    unitImport.setSysroot(CanonSysroot);

    // Collect the dependency info and process them outside of the libIndexStore callback.
    // This is because processing populates the database and C++ exceptions can be thrown; libIndexStore builds with -fno-exceptions so we cannot
    // be throwing C++ exceptions from inside its frames.
    struct UnitDependencyInfo {
      IndexUnitDependency::DependencyKind Kind;
      bool IsSystem;
      std::string FilePath;
      std::string Name;
      std::string ModuleName;
    };
    SmallVector<UnitDependencyInfo, 16> dependencies;
    Reader.foreachDependency([&](IndexUnitDependency Dep)->bool {
      dependencies.push_back(UnitDependencyInfo{Dep.getKind(), Dep.isSystem(), Dep.getFilePath(), Dep.getName(), Dep.getModuleName()});
      return true;
    });

    for (const UnitDependencyInfo &dep : dependencies) {
      switch (dep.Kind) {
        case IndexUnitDependency::DependencyKind::Record: {
          CanonicalFilePath CanonPath = CanonPathCache->getCanonicalPath(dep.FilePath, WorkDir);
          if (CanonPath.empty())
            break;

          if (!dep.IsSystem)
            UserFileDepends.push_back(CanonPath);
          StringRef recordName = dep.Name;
          StringRef moduleName = dep.ModuleName;
          if (moduleName.empty()) {
            // Workaround for swift compiler not associating the module name with records of swift files.
            // FIXME: Fix this on swift compiler and remove this.
            if (CanonPath.getPath().endswith(".swift")) {
              moduleName = Reader.getModuleName();
            }
          }
          bool isNewProvider;
          IDCode providerCode = unitImport.addProviderDependency(recordName, CanonPath, moduleName, dep.IsSystem, &isNewProvider);
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
          unitDependencies.push_back(dep.Name);
          IDCode unitDepCode = unitImport.addUnitDependency(dep.Name);
          if (!dep.IsSystem)
            UserUnitDepends.push_back(unitDepCode);
          break;
        }

        case IndexUnitDependency::DependencyKind::File: {
          CanonicalFilePath CanonPath = CanonPathCache->getCanonicalPath(dep.FilePath, WorkDir);
          if (CanonPath.empty())
            break;

          if (!dep.IsSystem)
            UserFileDepends.push_back(CanonPath);
          unitImport.addFileDependency(CanonPath);
        }
      }
    }

    unitImport.commit();
    StoreUnitInfoOpt = StoreUnitInfo{unitName, CanonMainFile, CanonOutFile, unitImport.getHasTestSymbols().getValue(), unitModTime};
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
      StoreUnitInfoOpt = StoreUnitInfo{unitName, mainFile, outFile, PrevHasTestSymbols.getValue(), unitModTime};
    }
    Delegate->processedStoreUnit(StoreUnitInfoOpt.getValue());
  }

  if (UseExplicitOutputUnits) {
    // Unit dependencies, like PCH/modules, are not included in the explicit list,
    // make sure to process them as we find them.
    // We do this after finishing processing the dependent unit to avoid nested write transactions.
    std::vector<UnitEventInfo> unitsNeedingUpdate;
    {
      ReadTransaction reader(SymIndex->getDBase());

      auto needsUpdate = [&](StringRef unitName) -> bool {
        if (processSession->hasEnqueuedUnitDependency(unitName)) {
          // Avoid enqueuing the same dependency from multiple dependents.
          return false;
        }
        UnitInfo info = reader.getUnitInfo(unitName);
        if (info.isInvalid()) {
          return true; // not registered yet.
        }
        std::string error;
        auto optModTime = IdxStore->getUnitModificationTime(unitName, error);
        if (!optModTime) {
          LOG_WARN_FUNC("error getting mod time for unit '" << unitName << "':" << error);
          return false;
        }
        auto unitModTime = toTimePoint(optModTime.getValue());
        return info.ModTime != unitModTime;
      };

      for (const auto &unitName : unitDependencies) {
        if (needsUpdate(unitName)) {
          unitsNeedingUpdate.push_back(UnitEventInfo(IndexStore::UnitEvent::Kind::Added, unitName, /*isDependency=*/true));
        }
      }
    }
    processSession->enqueue(std::move(unitsNeedingUpdate));
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

void StoreUnitRepo::addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  std::vector<UnitEventInfo> unitEvts;
  {
    sys::ScopedLock L(StateMtx);
    SmallString<128> nameBuf;
    for (StringRef filePath : filePaths) {
      nameBuf.clear();
      IdxStore->getUnitNameFromOutputPath(filePath, nameBuf);
      StringRef unitName = nameBuf.str();
      ExplicitOutputUnitsSet.insert(makeIDCodeFromString(unitName));
      // It makes no difference for unit registration whether the kind is `Added` or `Modified`.
      unitEvts.push_back(UnitEventInfo(IndexStore::UnitEvent::Kind::Added, unitName));
    }
  }
  auto session = makeUnitProcessingSession();
  session->process(std::move(unitEvts), waitForProcessing);
}

void StoreUnitRepo::removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  // FIXME: This doesn't remove unit dependencies. Probably a task for `purgeStaleData`.
  std::vector<UnitEventInfo> unitEvts;
  {
    sys::ScopedLock L(StateMtx);
    SmallString<128> nameBuf;
    for (StringRef filePath : filePaths) {
      nameBuf.clear();
      IdxStore->getUnitNameFromOutputPath(filePath, nameBuf);
      StringRef unitName = nameBuf.str();
      ExplicitOutputUnitsSet.erase(makeIDCodeFromString(unitName));
      unitEvts.push_back(UnitEventInfo(IndexStore::UnitEvent::Kind::Removed, unitName));
    }
  }
  auto session = makeUnitProcessingSession();
  session->process(std::move(unitEvts), waitForProcessing);
}

bool StoreUnitRepo::isUnitNameInKnownOutFilePaths(StringRef unitName) const {
  sys::ScopedLock L(StateMtx);
  return ExplicitOutputUnitsSet.count(makeIDCodeFromString(unitName));
}

void StoreUnitRepo::purgeStaleData() {
  // FIXME: Get referenced records from the database.
  // IdxStore->purgeStaleRecords(ActiveRecNames);
}

void StoreUnitRepo::setInitialUnitCount(unsigned count) {
  InitializingState.RemainingInitUnits = count;
}

void StoreUnitRepo::processedInitialUnitCount(unsigned count) {
  assert(!InitializingState.DoneInit);
  InitializingState.RemainingInitUnits -= std::min(count, InitializingState.RemainingInitUnits);
  if (InitializingState.RemainingInitUnits == 0) {
    finishedUnitInitialization();
  }
}

void StoreUnitRepo::finishedUnitInitialization() {
  assert(!InitializingState.DoneInit);
  dispatch_semaphore_signal(InitSemaphore);
  InitializingState.DoneInit = true;
}

void StoreUnitRepo::waitUntilDoneInitializing() {
  if (InitializingState.DoneInit)
    return;
  dispatch_semaphore_wait(InitSemaphore, DISPATCH_TIME_FOREVER);
}

void StoreUnitRepo::pollForUnitChangesAndWait() {
  sys::ScopedLock L(pollUnitsState.pollMtx);
  std::vector<UnitEventInfo> events;
  {
    llvm::StringMap<sys::TimePoint<>> knownUnits;
    llvm::StringMap<sys::TimePoint<>> foundUnits;

    std::swap(knownUnits, pollUnitsState.knownUnits);

    IdxStore->foreachUnit(/*sort=*/false, [&](StringRef unitName) {
      std::string error;
      auto optModTime = IdxStore->getUnitModificationTime(unitName, error);
      if (!optModTime) {
        LOG_WARN_FUNC("error getting mod time for unit '" << unitName << "':" << error);
        return true;
      }

      auto modTime = toTimePoint(optModTime.getValue());
      foundUnits[unitName] = modTime;

      auto I = knownUnits.find(unitName);
      if (I != knownUnits.end() && I->getValue() != modTime) {
        events.push_back({IndexStore::UnitEvent::Kind::Modified, unitName.str()});
      } else {
        events.push_back({IndexStore::UnitEvent::Kind::Added, unitName.str()});
      }

      return true;
    });

    for (const auto &known : knownUnits) {
      if (foundUnits.count(known.getKey()) == 0) {
        events.push_back({IndexStore::UnitEvent::Kind::Removed, known.getKey().str()});
      }
    }

    pollUnitsState.knownUnits = std::move(foundUnits);
  }

  auto session = makeUnitProcessingSession();
  session->process(std::move(events), /*waitForProcessing=*/true);
}

std::shared_ptr<UnitProcessingSession> StoreUnitRepo::makeUnitProcessingSession() {
  return std::make_shared<UnitProcessingSession>(std::make_shared<UnitEventInfoDeque>(),
                                                 shared_from_this(),
                                                 Delegate);
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
  bool hasTestSymbols = false;
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
      hasTestSymbols = unitInfo.HasTestSymbols;
      CurrModTime = unitInfo.ModTime;
    }
    reader.getDirectDependentUnits(unitCode, dependentUnits);
  }

  if (!MainFilePath.empty() && Delegate) {
    StoreUnitInfo unitInfo{unitName, MainFilePath, OutFilePath, hasTestSymbols, CurrModTime};
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
    std::string FilePath;
    sys::TimePoint<> ModTime;
    SmallVector<IDCode, 2> UnitCodes;
  };

  std::vector<OutOfDateCheck> outOfDateChecks;
  {
    ReadTransaction reader(SymIndex->getDBase());
    reader.findFilePathsWithParentPaths(parentPathStrRefs, [&](IDCode pathCode, CanonicalFilePathRef filePath) -> bool {
      auto modTime = UnitMonitor::getModTimeForOutOfDateCheck(filePath.getPath());
      outOfDateChecks.push_back(OutOfDateCheck{filePath.getPath().str(), modTime, {}});
      reader.foreachUnitContainingFile(pathCode, [&](ArrayRef<IDCode> unitCodes) -> bool {
        outOfDateChecks.back().UnitCodes.append(unitCodes.begin(), unitCodes.end());
        return true;
      });
      return true;
    });
  }
  // We collect and call later to avoid nested read transactions.
  for (auto &check : outOfDateChecks) {
    for (IDCode unitCode : check.UnitCodes) {
      if (auto monitor = getUnitMonitor(unitCode)) {
        monitor->checkForOutOfDate(check.ModTime, check.FilePath);
      }
    }
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

bool IndexDatastoreImpl::init(IndexStoreRef idxStore,
                              SymbolIndexRef SymIndex,
                              std::shared_ptr<IndexSystemDelegate> Delegate,
                              std::shared_ptr<CanonicalPathCache> CanonPathCache,
                              bool useExplicitOutputUnits,
                              bool readonly,
                              bool listenToUnitEvents,
                              std::string &Error) {
  this->IdxStore = std::move(idxStore);
  if (!this->IdxStore)
    return true;

  if (readonly)
    return false;

  auto UnitRepo = std::make_shared<StoreUnitRepo>(this->IdxStore, SymIndex, useExplicitOutputUnits, Delegate, CanonPathCache);
  std::weak_ptr<StoreUnitRepo> WeakUnitRepo = UnitRepo;
  auto eventsDeque = std::make_shared<UnitEventInfoDeque>();
  auto OnUnitsChange = [WeakUnitRepo, Delegate, eventsDeque](IndexStore::UnitEventNotification EventNote) {
    if (EventNote.isInitial()) {
      auto UnitRepo = WeakUnitRepo.lock();
      if (!UnitRepo)
        return;
      size_t evtCount = EventNote.getEventsCount();
      if (evtCount == 0) {
        UnitRepo->finishedUnitInitialization();
      } else {
        UnitRepo->setInitialUnitCount(evtCount);
      }
    }

    std::vector<UnitEventInfo> evts;
    for (size_t i = 0, e = EventNote.getEventsCount(); i != e; ++i) {
      auto evt = EventNote.getEvent(i);
      evts.push_back(UnitEventInfo{evt.getKind(), evt.getUnitName()});
    }

    auto session = std::make_shared<UnitProcessingSession>(eventsDeque, WeakUnitRepo, Delegate);
    session->process(std::move(evts), /*waitForProcessing=*/false);
  };

  if (listenToUnitEvents) {
    this->IdxStore->setUnitEventHandler(OnUnitsChange);
    bool err = this->IdxStore->startEventListening(/*waitInitialSync=*/false, Error);
    if (err)
      return true;
  }

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

void IndexDatastoreImpl::addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  return UnitRepo->addUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexDatastoreImpl::removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  return UnitRepo->removeUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexDatastoreImpl::purgeStaleData() {
  UnitRepo->purgeStaleData();
}

void IndexDatastoreImpl::pollForUnitChangesAndWait() {
  UnitRepo->pollForUnitChangesAndWait();
}

//===----------------------------------------------------------------------===//
// IndexDatastore
//===----------------------------------------------------------------------===//

std::unique_ptr<IndexDatastore>
IndexDatastore::create(IndexStoreRef idxStore,
                       SymbolIndexRef SymIndex,
                       std::shared_ptr<IndexSystemDelegate> Delegate,
                       std::shared_ptr<CanonicalPathCache> CanonPathCache,
                       bool useExplicitOutputUnits,
                       bool readonly,
                       bool listenToUnitEvents,
                       std::string &Error) {
  std::unique_ptr<IndexDatastoreImpl> Impl(new IndexDatastoreImpl());
  bool Err = Impl->init(std::move(idxStore), std::move(SymIndex), std::move(Delegate), std::move(CanonPathCache),
                        useExplicitOutputUnits, readonly, listenToUnitEvents, Error);
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

void IndexDatastore::addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  return IMPL->addUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexDatastore::removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  return IMPL->removeUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexDatastore::purgeStaleData() {
  return IMPL->purgeStaleData();
}

void IndexDatastore::pollForUnitChangesAndWait() {
  return IMPL->pollForUnitChangesAndWait();
}
