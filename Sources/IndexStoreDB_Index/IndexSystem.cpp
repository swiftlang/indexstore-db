//===--- IndexSystem.cpp --------------------------------------------------===//
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

#include <IndexStoreDB_Core/Symbol.h>
#include <IndexStoreDB_Index/IndexSystem.h>
#include <IndexStoreDB_Index/IndexStoreLibraryProvider.h>
#include <IndexStoreDB_Index/IndexSystemDelegate.h>
#include <IndexStoreDB_Index/FilePathIndex.h>
#include <IndexStoreDB_Index/SymbolIndex.h>
#include <IndexStoreDB_Database/Database.h>
#include "FileVisibilityChecker.h"
#include "IndexDatastore.h"

#include <IndexStoreDB_Support/Path.h>
#include <IndexStoreDB_Support/Concurrency.h>
#include <IndexStoreDB_Index/IndexStoreCXX.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_ArrayRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_STLExtras.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_FileSystem.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_raw_ostream.h>

#include <unordered_map>

using namespace IndexStoreDB;
using namespace IndexStoreDB::index;

namespace {

/// Delegates invocations for the provided \c IndexSystemDelegate serially and
/// asynchronously using a dedicated \c WorkQueue.
///
/// This allows the index system to invoke \c IndexSystemDelegate methods
/// without blocking on their implementations.
class AsyncIndexDelegate : public IndexSystemDelegate {
  std::vector<std::shared_ptr<IndexSystemDelegate>> Others;
  unsigned PendingActions = 0;
  WorkQueue Queue{WorkQueue::Dequeuing::Serial, "indexstoredb.AsyncIndexDelegate"};

public:
  AsyncIndexDelegate(std::shared_ptr<IndexSystemDelegate> Other)
    : Others(1, std::move(Other)) {}

  ~AsyncIndexDelegate() {
    _wait(); // Ensure the queue is drained, since we capture `this`.
  }

  void addDelegate(std::shared_ptr<IndexSystemDelegate> Other) {
    Queue.dispatchSync([&] {
      if (PendingActions)
        Other->processingAddedPending(PendingActions);
      Others.push_back(std::move(Other));
    });
  }

private:
  virtual void initialPendingUnits(unsigned numUnits) override {
    Queue.dispatch([this, numUnits]{
      for (auto &other : Others)
        other->initialPendingUnits(numUnits);
    });
  }

  virtual void processingAddedPending(unsigned NumActions) override {
    Queue.dispatch([this, NumActions]{
      PendingActions += NumActions;
      for (auto &other : Others)
        other->processingAddedPending(NumActions);
    });
  }

  virtual void processingCompleted(unsigned NumActions) override {
    Queue.dispatch([this, NumActions]{
      assert(NumActions <= PendingActions);
      PendingActions -= NumActions;
      for (auto &other : Others)
        other->processingCompleted(NumActions);
    });
  }

  virtual void processedStoreUnit(StoreUnitInfo unitInfo) override {
    Queue.dispatch([this, unitInfo]{
      for (auto &other : Others)
        other->processedStoreUnit(unitInfo);
    });
  }

  virtual void unitIsOutOfDate(StoreUnitInfo unitInfo,
                               OutOfDateFileTriggerRef trigger,
                               bool synchronous) override {
    if (synchronous) {
      Queue.dispatchSync(
          [this, unitInfo = std::move(unitInfo), trigger = std::move(trigger)] {
            for (auto &other : Others)
              other->unitIsOutOfDate(unitInfo, trigger, /*synchronous*/ true);
          });
      return;
    }

    Queue.dispatch(
        [this, unitInfo = std::move(unitInfo), trigger = std::move(trigger)] {
          for (auto &other : Others)
            other->unitIsOutOfDate(unitInfo, trigger, /*synchronous*/ false);
        });
  }

public:
  /// Public for Testing. Wait for any outstanding async work to finish.
  void _wait() {
    Queue.dispatchSync([]{});
  }
};

class IndexSystemImpl {
  std::string StorePath;
  std::string DBasePath;
  std::shared_ptr<AsyncIndexDelegate> DelegateWrap;
  SymbolIndexRef SymIndex;
  FilePathIndexRef PathIndex;
  std::shared_ptr<FileVisibilityChecker> VisibilityChecker;

  std::unique_ptr<IndexDatastore> IndexStore;

public:
  bool init(StringRef StorePath,
            StringRef dbasePath,
            std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
            std::shared_ptr<IndexSystemDelegate> Delegate,
            const CreationOptions &options,
            Optional<size_t> initialDBSize,
            std::string &Error);

  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime);
  void checkUnitContainingFileIsOutOfDate(StringRef file);

  void registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);
  void unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);

  void addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing);
  void removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing);

  void purgeStaleData();

  /// *For Testing* Poll for any changes to units and wait until they have been registered.
  void pollForUnitChangesAndWait(bool isInitialScan);

  void printStats(raw_ostream &OS);

  void dumpProviderFileAssociations(raw_ostream &OS);

  void addDelegate(std::shared_ptr<IndexSystemDelegate> Delegate);

  bool foreachSymbolInFilePath(StringRef filePath,
                               function_ref<bool(const SymbolRef &symbol)> receiver);

  bool foreachSymbolOccurrenceInFilePath(StringRef filePath,
                                         function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                                    function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                bool AnchorStart,
                                                bool AnchorEnd,
                                                bool Subsequence,
                                                bool IgnoreCase,
                              function_ref<bool(SymbolOccurrenceRef)> Receiver);

  bool foreachCanonicalSymbolOccurrenceByName(StringRef name,
                        function_ref<bool(SymbolOccurrenceRef Occur)> receiver);

  bool foreachSymbolName(function_ref<bool(StringRef name)> receiver);

  bool foreachRelatedSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                                    function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                        function_ref<bool(SymbolOccurrenceRef occur)> receiver);

  bool foreachSymbolCallOccurrence(SymbolOccurrenceRef Callee,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  size_t countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly);
  bool foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                              function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  std::vector<SymbolRef> getBaseMethodsOrClasses(SymbolRef Sym);

  bool isKnownFile(StringRef filePath);

  bool foreachMainUnitContainingFile(StringRef filePath,
                                 function_ref<bool(const StoreUnitInfo &unitInfo)> receiver);

  bool foreachFileOfUnit(StringRef unitName,
                         bool followDependencies,
                         function_ref<bool(CanonicalFilePathRef filePath)> receiver);

  bool foreachFilenameContainingPattern(StringRef Pattern,
                                        bool AnchorStart,
                                        bool AnchorEnd,
                                        bool Subsequence,
                                        bool IgnoreCase,
                               function_ref<bool(CanonicalFilePathRef FilePath)> Receiver);

  bool foreachFileIncludingFile(StringRef TargetPath,
                                function_ref<bool(CanonicalFilePathRef SourcePath, unsigned Line)> Receiver);

  bool foreachFileIncludedByFile(StringRef SourcePath,
                                 function_ref<bool(CanonicalFilePathRef TargetPath, unsigned Line)> Receiver);

  bool foreachIncludeOfUnit(StringRef unitName,
                            function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver);

  bool foreachUnitTestSymbolReferencedByOutputPaths(ArrayRef<CanonicalFilePathRef> FilePaths,
      function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  /// Calls `receiver` for every unit test symbol in unit files that reference
  /// one of the main files in `mainFilePaths`.
  ///
  ///  \returns `false` if the receiver returned `false` to stop receiving symbols, `true` otherwise.
  bool foreachUnitTestSymbolReferencedByMainFiles(
      ArrayRef<StringRef> mainFilePaths,
      function_ref<bool(SymbolOccurrenceRef Occur)> receiver
  );

  /// Calls `receiver` for every unit test symbol in the index.
  ///
  ///  \returns `false` if the receiver returned `false` to stop receiving symbols, `true` otherwise.
  bool foreachUnitTestSymbol(function_ref<bool(SymbolOccurrenceRef Occur)> receiver);

  /// Returns the latest modification date of a unit that contains the given source file.
  ///
  /// If no unit containing the given source file exists, returns `None`.
  llvm::Optional<llvm::sys::TimePoint<>> timestampOfLatestUnitForFile(StringRef filePath);
};

} // anonymous namespace

bool IndexSystemImpl::init(StringRef StorePath,
                           StringRef dbasePath,
                           std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
                           std::shared_ptr<IndexSystemDelegate> Delegate,
                           const CreationOptions &options,
                           Optional<size_t> initialDBSize,
                           std::string &Error) {
  this->StorePath = StorePath;
  this->DBasePath = dbasePath;
  this->DelegateWrap = std::make_shared<AsyncIndexDelegate>(Delegate);

  auto dbase = db::Database::create(dbasePath, options.readonly, initialDBSize, Error);
  if (!dbase)
    return true;

  IndexStoreLibraryRef idxStoreLib = storeLibProvider->getLibraryForStorePath(StorePath);
  if (!idxStoreLib) {
    Error = "could not determine indexstore library";
    return true;
  }

  if (!options.readonly) {
    // Create the index store path, if it does not already exist.
    if (std::error_code EC = llvm::sys::fs::create_directories(StorePath)) {
      Error = "could not create directories for data store path ";
      Error += StorePath.str() + ": " + EC.message();
    }
  }

  auto idxStore = indexstore::IndexStore::create(StorePath, idxStoreLib, options.indexStoreOptions, Error);
  if (!idxStore)
    return true;

  auto canonPathCache = std::make_shared<CanonicalPathCache>();

  this->VisibilityChecker = std::make_shared<FileVisibilityChecker>(dbase, canonPathCache, options.useExplicitOutputUnits);
  this->SymIndex = std::make_shared<SymbolIndex>(dbase, idxStore, this->VisibilityChecker);
  this->PathIndex = std::make_shared<FilePathIndex>(dbase, idxStore, this->VisibilityChecker,
                                                    canonPathCache);
  this->IndexStore = IndexDatastore::create(idxStore,
                                            this->SymIndex,
                                            this->DelegateWrap,
                                            canonPathCache,
                                            options,
                                            Error);

  if (!this->IndexStore)
    return true;
  return false;
}

bool IndexSystemImpl::isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles) {
  return IndexStore->isUnitOutOfDate(unitOutputPath, dirtyFiles);
}

bool IndexSystemImpl::isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime) {
  return IndexStore->isUnitOutOfDate(unitOutputPath, outOfDateModTime);
}

void IndexSystemImpl::checkUnitContainingFileIsOutOfDate(StringRef file) {
  return IndexStore->checkUnitContainingFileIsOutOfDate(file);
}

void IndexSystemImpl::registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName) {
  return VisibilityChecker->registerMainFiles(filePaths, productName);
}

void IndexSystemImpl::unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName) {
  return VisibilityChecker->unregisterMainFiles(filePaths, productName);
}

void IndexSystemImpl::addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  VisibilityChecker->addUnitOutFilePaths(filePaths);
  IndexStore->addUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexSystemImpl::removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  VisibilityChecker->removeUnitOutFilePaths(filePaths);
  IndexStore->removeUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexSystemImpl::purgeStaleData() {
  IndexStore->purgeStaleData();
}

void IndexSystemImpl::pollForUnitChangesAndWait(bool isInitialScan) {
  IndexStore->pollForUnitChangesAndWait(isInitialScan);
  DelegateWrap->_wait();
}

void IndexSystemImpl::printStats(raw_ostream &OS) {
  SymIndex->printStats(OS);
}

void IndexSystemImpl::dumpProviderFileAssociations(raw_ostream &OS) {
  return SymIndex->dumpProviderFileAssociations(OS);
}

void IndexSystemImpl::addDelegate(std::shared_ptr<IndexSystemDelegate> Delegate) {
  DelegateWrap->addDelegate(std::move(Delegate));
}

bool IndexSystemImpl::foreachSymbolOccurrenceByUSR(StringRef USR,
                                                    SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return SymIndex->foreachSymbolOccurrenceByUSR(USR, RoleSet, std::move(Receiver));
}

bool IndexSystemImpl::foreachRelatedSymbolOccurrenceByUSR(StringRef USR,
                                                    SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return SymIndex->foreachRelatedSymbolOccurrenceByUSR(USR, RoleSet, std::move(Receiver));
}

bool IndexSystemImpl::foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                               bool AnchorStart,
                                                               bool AnchorEnd,
                                                               bool Subsequence,
                                                               bool IgnoreCase,
                             function_ref<bool(SymbolOccurrenceRef)> Receiver) {
  return SymIndex->foreachCanonicalSymbolOccurrenceContainingPattern(Pattern, AnchorStart, AnchorEnd,
                                                            Subsequence, IgnoreCase,
                                                            std::move(Receiver));
}

bool IndexSystemImpl::foreachCanonicalSymbolOccurrenceByName(StringRef name,
                       function_ref<bool(SymbolOccurrenceRef Occur)> receiver) {
  return SymIndex->foreachCanonicalSymbolOccurrenceByName(name, std::move(receiver));
}

bool IndexSystemImpl::foreachSymbolName(function_ref<bool(StringRef name)> receiver) {
  return SymIndex->foreachSymbolName(std::move(receiver));
}

bool IndexSystemImpl::foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                       function_ref<bool(SymbolOccurrenceRef occur)> receiver) {
  return SymIndex->foreachCanonicalSymbolOccurrenceByUSR(USR, std::move(receiver));
}

static bool containsSymWithUSR(const SymbolRef &Sym,
                               const std::vector<SymbolRef> &Syms) {
  auto It = std::find_if(Syms.begin(), Syms.end(),
    [&](const SymbolRef &FoundSym) -> bool {
      return FoundSym->getUSR() == Sym->getUSR();
    });
  return It != Syms.end();
}
static bool containsSymWithUSR(const SymbolRef &Sym,
                               const std::vector<SymbolOccurrenceRef> &Syms) {
  auto It = std::find_if(Syms.begin(), Syms.end(),
    [&](const SymbolOccurrenceRef &FoundSym) -> bool {
      return FoundSym->getSymbol()->getUSR() == Sym->getUSR();
    });
  return It != Syms.end();
}

static void getBaseMethodsOrClassesImpl(IndexSystemImpl &Index,
                                        SymbolRef Sym,
                                     std::vector<SymbolRef> &BaseSyms) {
  auto addEntry = [&](SymbolRef NewSym) {
   if (!containsSymWithUSR(NewSym, BaseSyms)) {
     BaseSyms.push_back(NewSym);
     getBaseMethodsOrClassesImpl(Index, std::move(NewSym), BaseSyms);
   }
  };

  if (Sym->getSymbolKind() == SymbolKind::InstanceMethod) {
    Index.foreachSymbolOccurrenceByUSR(Sym->getUSR(),
                                       SymbolRole::RelationOverrideOf,
     [&](SymbolOccurrenceRef Occur) -> bool {
       Occur->foreachRelatedSymbol(SymbolRole::RelationOverrideOf,
         [&](SymbolRef RelSym){
           addEntry(std::move(RelSym));
         });
       return true;
     });
  } else {
    Index.foreachRelatedSymbolOccurrenceByUSR(Sym->getUSR(),
                                       SymbolRole::RelationBaseOf,
     [&](SymbolOccurrenceRef Occur) -> bool {
       addEntry(Occur->getSymbol());
       return true;
     });
  }
}

static void getAllRelatedOccursImpl(IndexSystemImpl &Index, SymbolRef Sym,
                                    SymbolRoleSet RoleSet,
                                    std::vector<SymbolOccurrenceRef> &RelSyms) {
  auto addEntry = [&](SymbolOccurrenceRef NewSym) {
   if (!containsSymWithUSR(NewSym->getSymbol(), RelSyms)) {
     RelSyms.push_back(NewSym);
     getAllRelatedOccursImpl(Index, NewSym->getSymbol(), RoleSet, RelSyms);
   }
  };

  Index.foreachRelatedSymbolOccurrenceByUSR(Sym->getUSR(), RoleSet,
   [&](SymbolOccurrenceRef Occur) -> bool {
     addEntry(std::move(Occur));
     return true;
   });
}

bool IndexSystemImpl::foreachSymbolCallOccurrence(SymbolOccurrenceRef Callee,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  auto Sym = Callee->getSymbol();
  if (!Sym->isCallable())
    return false;

  // Find direct call references.
  bool Continue = foreachSymbolOccurrenceByUSR(Sym->getUSR(), SymbolRole::Call,
                                               Receiver);
  if (!Continue)
    return false;

  if (!Callee->getRoles().containsAny(SymbolRole::Dynamic)) {
    // We don't need to search for 'dynamic' callers.
    return true;
  }

  // Take into account virtual methods and dynamic dispatch.
  // Search for dynamic calls where the receiver is a class in the method's base
  // class hierarchy.

  // Collect the classes in the base hierarchy. If any of these are receivers
  // in a dynamic dispatch call then we will include it as potential caller.
  SymbolRole RelationToUse;
  if (Callee->getRoles().containsAny(SymbolRole::Call))
    RelationToUse = SymbolRole::RelationReceivedBy;
  else
    RelationToUse = SymbolRole::RelationChildOf;
  std::vector<SymbolRef> clsSyms;
  Callee->foreachRelatedSymbol(RelationToUse,
    [&](SymbolRef RelSym){
      clsSyms.push_back(RelSym);
    });
  // Replace extensions with the types they extend.
  for (auto &clsSym : clsSyms) {
    if (clsSym->getSymbolKind() == SymbolKind::Extension) {
      foreachRelatedSymbolOccurrenceByUSR(clsSym->getUSR(), SymbolRole::RelationExtendedBy, [&](SymbolOccurrenceRef Occur) -> bool {
        clsSym = Occur->getSymbol();
        return false;
      });
    }
  }

  if (clsSyms.empty())
    return true;

  if (clsSyms[0]->getSymbolKind() == SymbolKind::Protocol) {
    // Find direct call references of all the conforming methods.
    std::vector<SymbolOccurrenceRef> overrideSyms;
    getAllRelatedOccursImpl(*this, Sym, SymbolRole::RelationOverrideOf, overrideSyms);
    for (const auto &occur : overrideSyms) {
      bool Continue = foreachSymbolOccurrenceByUSR(occur->getSymbol()->getUSR(), SymbolRole::Call, Receiver);
      if (!Continue)
        return false;
    }
    return true;
  }

  std::vector<SymbolRef> ClassSyms;
  for (const auto &clsSym : clsSyms) {
    getBaseMethodsOrClassesImpl(*this, clsSym, ClassSyms);
    ClassSyms.push_back(clsSym);
  }

  // Get all override methods walking the base hierarchy.
  std::vector<SymbolRef> BaseMethodSyms = getBaseMethodsOrClasses(Sym);

  for (auto &MethodSym : BaseMethodSyms) {
    bool Continue = foreachSymbolOccurrenceByUSR(MethodSym->getUSR(),
                                                 SymbolRole::Call,
     [&](SymbolOccurrenceRef Occur) -> bool {
       bool IsDynamic = Occur->getRoles().containsAny(SymbolRole::Dynamic);
       if (!IsDynamic)
         return true;

       bool PossiblyCalledViaDispatch = false;
       if (!Occur->getRoles().contains(SymbolRole::RelationReceivedBy)) {
         // Receiver is 'id' so the class that the method belongs to is a
         // candidate.
         PossiblyCalledViaDispatch = true;
       } else {
         Occur->foreachRelatedSymbol(SymbolRole::RelationReceivedBy,
           [&](SymbolRef RelSym) {
             if (containsSymWithUSR(RelSym, ClassSyms))
               PossiblyCalledViaDispatch = true;
           });
       }
       if (PossiblyCalledViaDispatch)
         return Receiver(Occur);

       return true;
     });

    if (!Continue)
      return false;
  }

  return true;
}

size_t IndexSystemImpl::countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly) {
  return SymIndex->countOfCanonicalSymbolsWithKind(symKind, workspaceOnly);
}

bool IndexSystemImpl::foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                                             function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return SymIndex->foreachCanonicalSymbolOccurrenceByKind(symKind, workspaceOnly, std::move(Receiver));
}

std::vector<SymbolRef>
IndexSystemImpl::getBaseMethodsOrClasses(SymbolRef Sym) {
  std::vector<SymbolRef> Syms;
  getBaseMethodsOrClassesImpl(*this, std::move(Sym), Syms);
  return Syms;
}

bool IndexSystemImpl::isKnownFile(StringRef filePath) {
  auto canonPath = PathIndex->getCanonicalPath(filePath);
  return PathIndex->isKnownFile(canonPath);
}

bool IndexSystemImpl::foreachMainUnitContainingFile(StringRef filePath,
                                                    function_ref<bool(const StoreUnitInfo &unitInfo)> receiver) {
    auto canonPath = PathIndex->getCanonicalPath(filePath);
    return PathIndex->foreachMainUnitContainingFile(canonPath, std::move(receiver));
}

bool IndexSystemImpl::foreachSymbolInFilePath(StringRef filePath,
                                              function_ref<bool(const SymbolRef &symbol)> receiver) {
    auto canonPath = PathIndex->getCanonicalPath(filePath);
    return SymIndex->foreachSymbolInFilePath(canonPath, std::move(receiver));
}

bool IndexSystemImpl::foreachSymbolOccurrenceInFilePath(StringRef filePath,
                                                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  auto canonPath = PathIndex->getCanonicalPath(filePath);
  return SymIndex->foreachSymbolOccurrenceInFilePath(canonPath, std::move(Receiver));
}

bool IndexSystemImpl::foreachFileOfUnit(StringRef unitName,
                                        bool followDependencies,
                                        function_ref<bool(CanonicalFilePathRef filePath)> receiver) {
  return PathIndex->foreachFileOfUnit(unitName, followDependencies, std::move(receiver));
}

bool IndexSystemImpl::foreachFilenameContainingPattern(StringRef Pattern,
                                                       bool AnchorStart,
                                                       bool AnchorEnd,
                                                       bool Subsequence,
                                                       bool IgnoreCase,
                              function_ref<bool(CanonicalFilePathRef FilePath)> Receiver) {
  return PathIndex->foreachFilenameContainingPattern(Pattern, AnchorStart,
                                                     AnchorEnd,
                                                     Subsequence, IgnoreCase,
                                                     std::move(Receiver));
}

bool IndexSystemImpl::foreachFileIncludingFile(StringRef TargetPath,
                                               function_ref<bool(CanonicalFilePathRef SourcePath, unsigned Line)> Receiver) {
  auto canonTargetPath = PathIndex->getCanonicalPath(TargetPath);
  return PathIndex->foreachFileIncludingFile(canonTargetPath, Receiver);
}

bool IndexSystemImpl::foreachFileIncludedByFile(StringRef SourcePath,
                                                function_ref<bool(CanonicalFilePathRef TargetPath, unsigned Line)> Receiver) {
  auto canonSourcePath = PathIndex->getCanonicalPath(SourcePath);
  return PathIndex->foreachFileIncludedByFile(canonSourcePath, Receiver);
}

bool IndexSystemImpl::foreachIncludeOfUnit(StringRef unitName,
                                           function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver) {
  return PathIndex->foreachIncludeOfUnit(unitName, receiver);
}

bool IndexSystemImpl::foreachUnitTestSymbolReferencedByOutputPaths(ArrayRef<CanonicalFilePathRef> FilePaths, function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return SymIndex->foreachUnitTestSymbolReferencedByOutputPaths(FilePaths, std::move(Receiver));
}

bool IndexSystemImpl::foreachUnitTestSymbolReferencedByMainFiles(
    ArrayRef<StringRef> mainFilePaths,
    function_ref<bool(SymbolOccurrenceRef Occur)> receiver
) {
  std::vector<CanonicalFilePath> canonicalMainFilesPaths;
  for (StringRef mainFilePath : mainFilePaths) {
    canonicalMainFilesPaths.push_back(PathIndex->getCanonicalPath(mainFilePath));
  }
  return SymIndex->foreachUnitTestSymbolReferencedByMainFiles(canonicalMainFilesPaths, std::move(receiver));
}

bool IndexSystemImpl::foreachUnitTestSymbol(function_ref<bool(SymbolOccurrenceRef Occur)> receiver) {
  return SymIndex->foreachUnitTestSymbol(std::move(receiver));
}

llvm::Optional<llvm::sys::TimePoint<>> IndexSystemImpl::timestampOfLatestUnitForFile(StringRef filePath) {
  auto canonFilePath = PathIndex->getCanonicalPath(filePath);
  return SymIndex->timestampOfLatestUnitForFile(canonFilePath);
}

//===----------------------------------------------------------------------===//
// IndexSystem
//===----------------------------------------------------------------------===//

void IndexSystemDelegate::anchor() {}

std::shared_ptr<IndexSystem>
IndexSystem::create(StringRef StorePath,
                    StringRef dbasePath,
                    std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
                    std::shared_ptr<IndexSystemDelegate> Delegate,
                    const CreationOptions &options,
                    Optional<size_t> initialDBSize,
                    std::string &Error) {
  std::unique_ptr<IndexSystemImpl> Impl(new IndexSystemImpl());
  bool Err = Impl->init(StorePath, dbasePath, std::move(storeLibProvider), std::move(Delegate),
                        options, initialDBSize, Error);
  if (Err)
    return nullptr;

  std::shared_ptr<IndexSystem> Index;
  Index.reset(new IndexSystem(Impl.release()));
  return Index;
}

#define IMPL static_cast<IndexSystemImpl*>(Impl)

IndexSystem::~IndexSystem() {
  delete IMPL;
}

bool IndexSystem::isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles) {
  return IMPL->isUnitOutOfDate(unitOutputPath, dirtyFiles);
}

bool IndexSystem::isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime) {
  return IMPL->isUnitOutOfDate(unitOutputPath, outOfDateModTime);
}

void IndexSystem::checkUnitContainingFileIsOutOfDate(StringRef file) {
  return IMPL->checkUnitContainingFileIsOutOfDate(file);
}

void IndexSystem::registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName) {
  return IMPL->registerMainFiles(filePaths, productName);
}

void IndexSystem::unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName) {
  return IMPL->unregisterMainFiles(filePaths, productName);
}

void IndexSystem::addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  return IMPL->addUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexSystem::removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing) {
  return IMPL->removeUnitOutFilePaths(filePaths, waitForProcessing);
}

void IndexSystem::purgeStaleData() {
  return IMPL->purgeStaleData();
}

void IndexSystem::pollForUnitChangesAndWait(bool isInitialScan) {
  IMPL->pollForUnitChangesAndWait(isInitialScan);
}

void IndexSystem::printStats(raw_ostream &OS) {
  return IMPL->printStats(OS);
}

void IndexSystem::dumpProviderFileAssociations(raw_ostream &OS) {
  return IMPL->dumpProviderFileAssociations(OS);
}

void IndexSystem::dumpProviderFileAssociations() {
  return dumpProviderFileAssociations(llvm::errs());
}

void IndexSystem::addDelegate(std::shared_ptr<IndexSystemDelegate> Delegate) {
  IMPL->addDelegate(std::move(Delegate));
}

bool IndexSystem::foreachSymbolOccurrenceByUSR(StringRef USR,
                                                SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachSymbolOccurrenceByUSR(USR, RoleSet, std::move(Receiver));
}

bool IndexSystem::foreachRelatedSymbolOccurrenceByUSR(StringRef USR,
                                                      SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachRelatedSymbolOccurrenceByUSR(USR, RoleSet, std::move(Receiver));
}

bool IndexSystem::foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                           bool AnchorStart,
                                                           bool AnchorEnd,
                                                           bool Subsequence,
                                                           bool IgnoreCase,
                             function_ref<bool(SymbolOccurrenceRef)> Receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceContainingPattern(Pattern, AnchorStart, AnchorEnd,
                                                        Subsequence, IgnoreCase,
                                                        std::move(Receiver));
}

bool IndexSystem::foreachCanonicalSymbolOccurrenceByName(StringRef name,
                       function_ref<bool(SymbolOccurrenceRef Occur)> receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceByName(name, std::move(receiver));
}

bool IndexSystem::foreachSymbolName(function_ref<bool(StringRef name)> receiver) {
  return IMPL->foreachSymbolName(std::move(receiver));
}

bool IndexSystem::foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                       function_ref<bool(SymbolOccurrenceRef occur)> receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceByUSR(USR, std::move(receiver));
}

bool IndexSystem::foreachSymbolCallOccurrence(SymbolOccurrenceRef Callee,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachSymbolCallOccurrence(std::move(Callee),
                                           std::move(Receiver));
}

size_t IndexSystem::countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly) {
  return IMPL->countOfCanonicalSymbolsWithKind(symKind, workspaceOnly);
}

bool IndexSystem::foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                                         function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceByKind(symKind, workspaceOnly, std::move(Receiver));
}

bool IndexSystem::foreachSymbolInFilePath(StringRef FilePath,
                                          function_ref<bool(SymbolRef Symbol)> Receiver) {
    return IMPL->foreachSymbolInFilePath(FilePath, std::move(Receiver));
}

bool IndexSystem::foreachSymbolOccurrenceInFilePath(StringRef FilePath,
                                                    function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachSymbolOccurrenceInFilePath(FilePath, std::move(Receiver));
}

bool IndexSystem::isKnownFile(StringRef filePath) {
  return IMPL->isKnownFile(filePath);
}

bool IndexSystem::foreachMainUnitContainingFile(StringRef filePath,
                                            function_ref<bool(const StoreUnitInfo &unitInfo)> receiver) {
  return IMPL->foreachMainUnitContainingFile(filePath, std::move(receiver));
}

bool IndexSystem::foreachFileOfUnit(StringRef unitName,
                                    bool followDependencies,
                                    function_ref<bool(CanonicalFilePathRef filePath)> receiver) {
  return IMPL->foreachFileOfUnit(unitName, followDependencies, std::move(receiver));
}

bool IndexSystem::foreachFilenameContainingPattern(StringRef Pattern,
                                                   bool AnchorStart,
                                                   bool AnchorEnd,
                                                   bool Subsequence,
                                                   bool IgnoreCase,
                              function_ref<bool(CanonicalFilePathRef FilePath)> Receiver) {
  return IMPL->foreachFilenameContainingPattern(Pattern, AnchorStart, AnchorEnd,
                                                Subsequence, IgnoreCase,
                                                std::move(Receiver));
}

bool IndexSystem::foreachFileIncludingFile(StringRef TargetPath,
                                               function_ref<bool(CanonicalFilePathRef SourcePath, unsigned Line)> Receiver) {
  return IMPL->foreachFileIncludingFile(TargetPath, Receiver);
}

bool IndexSystem::foreachFileIncludedByFile(StringRef SourcePath,
                                                function_ref<bool(CanonicalFilePathRef TargetPath, unsigned Line)> Receiver) {
  return IMPL->foreachFileIncludedByFile(SourcePath, Receiver);
}

bool IndexSystem::foreachIncludeOfUnit(StringRef unitName,
                                       function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver) {
  return IMPL->foreachIncludeOfUnit(unitName, receiver);
}

bool IndexSystem::foreachUnitTestSymbolReferencedByOutputPaths(ArrayRef<CanonicalFilePathRef> FilePaths,
    function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachUnitTestSymbolReferencedByOutputPaths(FilePaths, std::move(Receiver));
}

bool IndexSystem::foreachUnitTestSymbolReferencedByMainFiles(
   ArrayRef<StringRef> mainFilePaths,
   function_ref<bool(SymbolOccurrenceRef Occur)> receiver
) {
  return IMPL->foreachUnitTestSymbolReferencedByMainFiles(mainFilePaths, std::move(receiver));
}

bool IndexSystem::foreachUnitTestSymbol(function_ref<bool(SymbolOccurrenceRef Occur)> receiver) {
  return IMPL->foreachUnitTestSymbol(std::move(receiver));
}

llvm::Optional<llvm::sys::TimePoint<>> IndexSystem::timestampOfLatestUnitForFile(StringRef filePath) {
  return IMPL->timestampOfLatestUnitForFile(filePath);
}
