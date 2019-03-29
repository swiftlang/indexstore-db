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

#include "IndexStoreDB/Core/Symbol.h"
#include "IndexStoreDB/Index/IndexSystem.h"
#include "IndexStoreDB/Index/IndexStoreLibraryProvider.h"
#include "IndexStoreDB/Index/IndexSystemDelegate.h"
#include "IndexStoreDB/Index/FilePathIndex.h"
#include "IndexStoreDB/Index/SymbolIndex.h"
#include "IndexStoreDB/Database/Database.h"
#include "FileVisibilityChecker.h"
#include "IndexDatastore.h"

#include "IndexStoreDB/Support/Path.h"
#include "IndexStoreDB/Support/Concurrency.h"
#include "indexstore/IndexStoreCXX.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

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
  std::shared_ptr<IndexSystemDelegate> Other;
  WorkQueue Queue{WorkQueue::Dequeuing::Serial, "indexstoredb.AsyncIndexDelegate"};

public:
  AsyncIndexDelegate(std::shared_ptr<IndexSystemDelegate> Other)
    : Other(std::move(Other)) {}

private:
  virtual void processingAddedPending(unsigned NumActions) override {
    if (!Other)
      return;
    auto LocalOther = this->Other;
    Queue.dispatch([LocalOther, NumActions]{
      LocalOther->processingAddedPending(NumActions);
    });
  }

  virtual void processingCompleted(unsigned NumActions) override {
    if (!Other)
      return;
    auto LocalOther = this->Other;
    Queue.dispatch([LocalOther, NumActions]{
      LocalOther->processingCompleted(NumActions);
    });
  }

  virtual void processedStoreUnit(StoreUnitInfo unitInfo) override {
    if (!Other)
      return;
    auto LocalOther = this->Other;
    Queue.dispatch([LocalOther, unitInfo]{
      LocalOther->processedStoreUnit(unitInfo);
    });
  }

  virtual void unitIsOutOfDate(StoreUnitInfo unitInfo,
                               llvm::sys::TimePoint<> outOfDateModTime,
                               OutOfDateTriggerHintRef hint,
                               bool synchronous) override {
    if (!Other)
      return;

    if (synchronous) {
      Other->unitIsOutOfDate(std::move(unitInfo), outOfDateModTime, hint, true);
      return;
    }

    auto LocalOther = this->Other;
    Queue.dispatch([=]{
      LocalOther->unitIsOutOfDate(std::move(unitInfo), outOfDateModTime, hint, false);
    });
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
            bool readonly, Optional<size_t> initialDBSize,
            std::string &Error);

  void waitUntilDoneInitializing();

  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime);
  void checkUnitContainingFileIsOutOfDate(StringRef file);

  void registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);
  void unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);

  void purgeStaleData();

  void printStats(raw_ostream &OS);

  void dumpProviderFileAssociations(raw_ostream &OS);

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
};

} // anonymous namespace

bool IndexSystemImpl::init(StringRef StorePath,
                           StringRef dbasePath,
                           std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
                           std::shared_ptr<IndexSystemDelegate> Delegate,
                           bool readonly, Optional<size_t> initialDBSize,
                           std::string &Error) {
  this->StorePath = StorePath;
  this->DBasePath = dbasePath;
  this->DelegateWrap = std::make_shared<AsyncIndexDelegate>(Delegate);

  auto dbase = db::Database::create(dbasePath, readonly, initialDBSize, Error);
  if (!dbase)
    return true;

  IndexStoreLibraryRef idxStoreLib = storeLibProvider->getLibraryForStorePath(StorePath);
  if (!idxStoreLib) {
    Error = "could not determine indexstore library";
    return true;
  }

  auto idxStore = indexstore::IndexStore::create(StorePath, idxStoreLib, Error);
  if (!idxStore)
    return true;

  auto canonPathCache = std::make_shared<CanonicalPathCache>();

  this->VisibilityChecker = std::make_shared<FileVisibilityChecker>(dbase, canonPathCache);
  this->SymIndex = std::make_shared<SymbolIndex>(dbase, idxStore, this->VisibilityChecker);
  this->PathIndex = std::make_shared<FilePathIndex>(dbase, idxStore, this->VisibilityChecker,
                                                    canonPathCache);
  this->IndexStore = IndexDatastore::create(idxStore,
                                            this->SymIndex,
                                            this->DelegateWrap,
                                            canonPathCache,
                                            readonly,
                                            Error);

  if (!this->IndexStore)
    return true;
  return false;
}

void IndexSystemImpl::waitUntilDoneInitializing() {
  IndexStore->waitUntilDoneInitializing();
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

void IndexSystemImpl::purgeStaleData() {
  IndexStore->purgeStaleData();
}

void IndexSystemImpl::printStats(raw_ostream &OS) {
  SymIndex->printStats(OS);
}

void IndexSystemImpl::dumpProviderFileAssociations(raw_ostream &OS) {
  return SymIndex->dumpProviderFileAssociations(OS);
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

//===----------------------------------------------------------------------===//
// IndexSystem
//===----------------------------------------------------------------------===//

void OutOfDateTriggerHint::_anchor() {}

std::string DependentFileOutOfDateTriggerHint::originalFileTrigger() {
  return FilePath;
}

std::string DependentFileOutOfDateTriggerHint::description() {
  return FilePath;
}

std::string DependentUnitOutOfDateTriggerHint::originalFileTrigger() {
  return DepHint->originalFileTrigger();
}

std::string DependentUnitOutOfDateTriggerHint::description() {
  std::string desc;
  llvm::raw_string_ostream OS(desc);
  OS << "unit(" << UnitName << ") -> " << DepHint->description();
  return desc;
}

void IndexSystemDelegate::anchor() {}

std::shared_ptr<IndexSystem>
IndexSystem::create(StringRef StorePath,
                    StringRef dbasePath,
                    std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
                    std::shared_ptr<IndexSystemDelegate> Delegate,
                    bool readonly, Optional<size_t> initialDBSize,
                    std::string &Error) {
  std::unique_ptr<IndexSystemImpl> Impl(new IndexSystemImpl());
  bool Err = Impl->init(StorePath, dbasePath, std::move(storeLibProvider), std::move(Delegate), readonly, initialDBSize, Error);
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

void IndexSystem::waitUntilDoneInitializing() {
  return IMPL->waitUntilDoneInitializing();
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

void IndexSystem::purgeStaleData() {
  return IMPL->purgeStaleData();
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
