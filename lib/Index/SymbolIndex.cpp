//===--- SymbolIndex.cpp --------------------------------------------------===//
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
#include "IndexStoreDB/Index/SymbolIndex.h"
#include "IndexStoreDB/Index/SymbolDataProvider.h"
#include "StoreSymbolRecord.h"
#include "IndexStoreDB/Database/Database.h"
#include "IndexStoreDB/Database/ImportTransaction.h"
#include "IndexStoreDB/Database/ReadTransaction.h"
#include "FileVisibilityChecker.h"

#include "indexstore/IndexStoreCXX.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/raw_ostream.h"

#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <deque>

using namespace IndexStoreDB;
using namespace IndexStoreDB::index;
using namespace IndexStoreDB::db;
using namespace llvm;

namespace {

class SymbolIndexImpl {
  DatabaseRef DBase;
  indexstore::IndexStoreRef IdxStore;
  std::shared_ptr<FileVisibilityChecker> VisibilityChecker;

  // Statistics tracking.
  std::atomic<unsigned> NumProvidersAdded{0};
  std::atomic<unsigned> NumProvidersRemoved{0};
  std::atomic<unsigned> NumProviderForeachSymbolOccurrenceByUSR{0};
  std::atomic<unsigned> NumProviderForeachRelatedSymbolOccurrenceByUSR{0};
  std::atomic<unsigned> NumMissingProvidersLookedUp{0};

public:
  SymbolIndexImpl(DatabaseRef dbase, indexstore::IndexStoreRef indexStore,
                  std::shared_ptr<FileVisibilityChecker> visibilityChecker)
    : DBase(std::move(dbase)), IdxStore(std::move(indexStore)), VisibilityChecker(std::move(visibilityChecker)) {}

  DatabaseRef getDBase() const { return DBase; }

  void importSymbols(ImportTransaction &Import, SymbolDataProviderRef Provider);
  void printStats(raw_ostream &OS);

  void dumpProviderFileAssociations(raw_ostream &OS);

  bool foreachSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);
  bool foreachRelatedSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
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

  bool foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                        function_ref<bool(SymbolOccurrenceRef occur)> receiver);
  size_t countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly);
  bool foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                              function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

private:
  bool foreachCanonicalSymbolImpl(bool workspaceOnly,
                                  function_ref<bool(ReadTransaction &, function_ref<bool(ArrayRef<IDCode> usrCode)> usrConsumer)> usrProducer,
                                  function_ref<bool(SymbolDataProviderRef, std::vector<std::pair<IDCode, bool>> USRs)> receiver);
  bool foreachCanonicalSymbolOccurrenceImpl(bool workspaceOnly,
                                            function_ref<bool(ReadTransaction &, function_ref<bool(ArrayRef<IDCode> usrCode)> usrConsumer)> usrProducer,
                                            function_ref<bool(SymbolOccurrenceRef)> Receiver);
  std::vector<SymbolDataProviderRef> lookupProvidersForUSR(StringRef USR, SymbolRoleSet roles, SymbolRoleSet relatedRoles);
  std::vector<std::pair<SymbolDataProviderRef, bool>> findCanonicalProvidersForUSR(IDCode usrCode);
  SymbolDataProviderRef createProviderForCode(IDCode providerCode, ReadTransaction &reader);
};

} // anonymous namespace

void SymbolIndexImpl::importSymbols(ImportTransaction &import, SymbolDataProviderRef Provider) {
  ++NumProvidersAdded;

  // FIXME: The records may contain duplicate USRs at the symbol array, the following
  // compensates for that. Duplicate USRs is an indication that the USR is not unique
  // or we missed canonicalizing a decl reference. We should fix all such issues.
  struct CoreSymbolData {
    StringRef Name;
    SymbolInfo SymInfo;
    SymbolRoleSet Roles;
    SymbolRoleSet RelatedRoles;
  };
  BumpPtrAllocator Alloc;
  StringMap<CoreSymbolData, BumpPtrAllocator&> CoreSymbols(Alloc);
  Provider->foreachCoreSymbolData([&](StringRef USR, StringRef Name,
                                      SymbolInfo Info,
                                      SymbolRoleSet Roles,
                                      SymbolRoleSet RelatedRoles)->bool {
    char *copiedStr = Alloc.Allocate<char>(Name.size());
    std::uninitialized_copy(Name.begin(), Name.end(), copiedStr);
    StringRef copiedName = StringRef(copiedStr, Name.size());
    // FIXME: Make this part of the compiler indexing output. E.g. a C++-like 'struct' should be a 'class' kind.
    if (Info.isClassLike())
      Info.Kind = SymbolKind::Class;
    auto pair = CoreSymbols.insert(std::make_pair(USR, CoreSymbolData{copiedName, Info, Roles, RelatedRoles}));
    bool wasInserted = pair.second;
    if (!wasInserted) {
      // errs() << "Duplicate USR in record '" << Provider->getIdentifier() << "':" << USR << '\n';
      pair.first->second.Roles |= Roles;
      pair.first->second.RelatedRoles |= RelatedRoles;
    }
    return true;
  });

  IDCode providerCode = import.addProviderName(Provider->getIdentifier());
  for (auto &coreSym : CoreSymbols) {
    import.addSymbolInfo(providerCode, coreSym.first(), coreSym.second.Name,
                         coreSym.second.SymInfo, coreSym.second.Roles, coreSym.second.RelatedRoles);
  }
}

void SymbolIndexImpl::printStats(raw_ostream &OS) {
  DBase->printStats(OS);
  OS << "\n*** SymbolIndex Statistics\n";
  OS << "Providers added: " << NumProvidersAdded << '\n';
  OS << "Providers removed: " << NumProvidersRemoved << '\n';
  OS << "Provider->foreachSymbolOccurrenceByUSR calls: " << NumProviderForeachSymbolOccurrenceByUSR << '\n';
  OS << "Provider->foreachRelatedSymbolOccurrenceByUSR calls: " << NumProviderForeachRelatedSymbolOccurrenceByUSR << '\n';
  OS << "Missing providers looked up: " << NumMissingProvidersLookedUp << '\n';
  OS << "----------------------\n";
}

void SymbolIndexImpl::dumpProviderFileAssociations(raw_ostream &OS) {
  ReadTransaction reader(DBase);
  Optional<IDCode> prevProvCode;
  reader.foreachProviderAndFileCodeReference([&](IDCode providerCode, IDCode pathCode, IDCode unitCode, llvm::sys::TimeValue modTime, IDCode moduleNameCode, bool isSystem) -> bool {
    if (!prevProvCode.hasValue() || prevProvCode.getValue() != providerCode) {
      OS << reader.getProviderName(providerCode) << '\n';
      prevProvCode = providerCode;
    }
    auto path = reader.getFullFilePathFromCode(pathCode);
    auto unit = reader.getUnitInfo(unitCode);
    auto moduleName = reader.getModuleName(moduleNameCode);
    OS << "---- " << path.getPath() << ", " << unit.UnitName << ", module: " << moduleName << ", sys: " << isSystem << ", " << modTime.seconds() << '\n';
    return true;
  });
}

SymbolDataProviderRef SymbolIndexImpl::createProviderForCode(IDCode providerCode, ReadTransaction &reader) {
  StringRef recordName = reader.getProviderName(providerCode);
  if (recordName.empty()) {
    ++NumMissingProvidersLookedUp;
    return nullptr;
  }

  Optional<SymbolProviderKind> providerKind;
  SmallVector<FileAndTarget, 8> fileRefs;
  reader.getProviderFileCodeReferences(providerCode, [&](IDCode pathCode, IDCode unitCode, llvm::sys::TimeValue modTime, IDCode moduleNameCode, bool isSystem) -> bool {
    auto unitInfo = reader.getUnitInfo(unitCode);
    if (unitInfo.isInvalid())
      return true;
    if (!VisibilityChecker->isUnitVisible(unitInfo, reader))
      return true;

    if (!providerKind.hasValue()) {
      providerKind = unitInfo.SymProviderKind;
    }
    CanonicalFilePathRef sysroot;
    if (unitInfo.HasSysroot) {
      sysroot = reader.getDirectoryFromCode(unitInfo.SysrootCode);
    }
    std::string pathString;
    llvm::raw_string_ostream OS(pathString);
    if (reader.getFullFilePathFromCode(pathCode, OS)) {
      FileAndTarget value{
        TimestampedPath{std::move(OS.str()), modTime, reader.getModuleName(moduleNameCode), isSystem, sysroot},
        reader.getTargetName(unitInfo.TargetCode)};
      fileRefs.push_back(std::move(value));
    }
    return true;
  });
  if (fileRefs.empty())
    return nullptr;

  return StoreSymbolRecord::create(IdxStore, recordName, providerCode, providerKind.getValue(), fileRefs);
}

std::vector<SymbolDataProviderRef>
SymbolIndexImpl::lookupProvidersForUSR(StringRef USR, SymbolRoleSet roles, SymbolRoleSet relatedRoles) {
  std::vector<SymbolDataProviderRef> providers;
  ReadTransaction reader(DBase);
  reader.lookupProvidersForUSR(USR, roles, relatedRoles, [&](IDCode providerCode, SymbolRoleSet roles, SymbolRoleSet relatedRoles) -> bool {
    if (auto prov = createProviderForCode(providerCode, reader))
      providers.push_back(prov);
    return true;
  });
  return providers;
}

bool SymbolIndexImpl::foreachSymbolOccurrenceByUSR(StringRef USR,
                                                    SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  assert(RoleSet && "did not set any role!");
  auto providers = lookupProvidersForUSR(USR, RoleSet, None);
  for (auto &prov : providers) {
    bool Continue = prov->foreachSymbolOccurrenceByUSR(makeIDCodeFromString(USR), RoleSet,
      [&](SymbolOccurrenceRef Occur)->bool {
        return Receiver(std::move(Occur));
      });
    ++NumProviderForeachSymbolOccurrenceByUSR;
    if (!Continue)
      return false;
  }

  return true;
}

bool SymbolIndexImpl::foreachRelatedSymbolOccurrenceByUSR(StringRef USR,
                                                    SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  assert(RoleSet && "did not set any role!");
  auto providers = lookupProvidersForUSR(USR, None, RoleSet);
  for (auto &prov : providers) {
    bool Continue = prov->foreachRelatedSymbolOccurrenceByUSR(makeIDCodeFromString(USR), RoleSet,
      [&](SymbolOccurrenceRef Occur)->bool {
        return Receiver(std::move(Occur));
      });
    ++NumProviderForeachRelatedSymbolOccurrenceByUSR;
    if (!Continue)
      return false;
  }

  return true;
}

bool SymbolIndexImpl::foreachCanonicalSymbolImpl(bool workspaceOnly,
                                                 function_ref<bool(ReadTransaction &, function_ref<bool(ArrayRef<IDCode> usrCode)> usrConsumer)> usrProducer,
                                                 function_ref<bool(SymbolDataProviderRef, std::vector<std::pair<IDCode, bool>> USRs)> receiver) {
  SymbolRoleSet DeclOrCanon = SymbolRoleSet(SymbolRole::Declaration) | SymbolRole::Canonical;

  struct PerProviderInfo {
    SymbolDataProviderRef Provider;
    std::vector<std::pair<IDCode, bool>> USRs;
    bool IsInvisible = false;
  };
  std::unordered_map<IDCode, PerProviderInfo> InfoByProvider;
  {
    ReadTransaction reader(DBase);
    bool cont = usrProducer(reader, [&](ArrayRef<IDCode> usrCodes) -> bool {
      for (IDCode usrCode : usrCodes) {
        // Pairs of (IDCode, isCanon), canonicals go at the front.
        std::deque<std::pair<IDCode, bool>> providerCodes;
        reader.lookupProvidersForUSR(usrCode, DeclOrCanon, None, [&](IDCode providerCode, SymbolRoleSet roles, SymbolRoleSet relatedRoles) -> bool {
          if (roles.contains(SymbolRole::Canonical))
            providerCodes.push_front({providerCode, true});
          else
            providerCodes.push_back({providerCode, false});
          return true;
        });

        auto getProvInfo = [&](IDCode provCode) -> PerProviderInfo & {
          auto &provInfo = InfoByProvider[provCode];
          if (provInfo.IsInvisible)
            return provInfo;
          if (!provInfo.Provider) {
            provInfo.Provider = createProviderForCode(provCode, reader);
            if (!provInfo.Provider)
              provInfo.IsInvisible = true;
          }
          return provInfo;
        };

        bool foundCanon = false;
        for (auto &pair : providerCodes) {
          IDCode provCode = pair.first;
          bool isCanon = pair.second;
          if (!isCanon && foundCanon)
            break;
          auto &provInfo = getProvInfo(provCode);
          if (provInfo.IsInvisible)
            continue;
          provInfo.USRs.emplace_back(usrCode, isCanon);
          foundCanon = foundCanon || isCanon;
        }
      }
      return true;
    });
    if (!cont)
      return false;
  }

  for (auto &Pair : InfoByProvider) {
    auto &provInfo = Pair.second;
    if (provInfo.IsInvisible)
      continue;
    if (workspaceOnly && provInfo.Provider->isSystem())
      continue;
    if (!receiver(std::move(provInfo.Provider), std::move(provInfo.USRs)))
      return false;
  }

  return true;
}

bool SymbolIndexImpl::foreachCanonicalSymbolOccurrenceImpl(bool workspaceOnly,
                                                           function_ref<bool(ReadTransaction &, function_ref<bool(ArrayRef<IDCode> usrCode)> usrConsumer)> usrProducer,
                                                           function_ref<bool(SymbolOccurrenceRef)> Receiver) {
  SymbolRoleSet DeclOrCanon = SymbolRoleSet(SymbolRole::Declaration) | SymbolRole::Canonical;
  return foreachCanonicalSymbolImpl(workspaceOnly, usrProducer, [&](SymbolDataProviderRef Prov, std::vector<std::pair<IDCode, bool>> USRsInfo) -> bool {
    SmallVector<IDCode, 16> USRs;
    USRs.reserve(USRsInfo.size());
    for (auto &Entry : USRsInfo) {
      USRs.push_back(Entry.first);
    }

    bool ReceiverStopped = false;
    Prov->foreachSymbolOccurrenceByUSR(USRs, DeclOrCanon, [&](SymbolOccurrenceRef Occur)->bool{
      IDCode OccurUSRCode = makeIDCodeFromString(Occur->getSymbol()->getUSR());
      auto It = std::find_if(USRsInfo.begin(), USRsInfo.end(), [&](const std::pair<IDCode, bool> &Info)->bool {
        return Info.first == OccurUSRCode;
      });
      if (It == USRsInfo.end())
        return true;

      bool HasCanonical = It->second;
      SymbolRoleSet OccurRoles = Occur->getRoles();
      if (!HasCanonical || OccurRoles.containsAny(SymbolRole::Canonical)) {
        bool Continue = Receiver(std::move(Occur));
        if (!Continue) {
          ReceiverStopped = true;
          return false;
        }
      }
      return true;
    });
    ++NumProviderForeachSymbolOccurrenceByUSR;

    return !ReceiverStopped;
  });
}

bool SymbolIndexImpl::foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                               bool AnchorStart,
                                                               bool AnchorEnd,
                                                               bool Subsequence,
                                                               bool IgnoreCase,
                             function_ref<bool(SymbolOccurrenceRef)> Receiver) {
  return foreachCanonicalSymbolOccurrenceImpl(/*workspaceOnly=*/false,
                                              [=](ReadTransaction &reader,
                                                 function_ref<bool (ArrayRef<IDCode>)> usrConsumer) -> bool {
    return reader.findUSRsWithNameContaining(Pattern, AnchorStart, AnchorEnd, Subsequence, IgnoreCase, usrConsumer);
  }, Receiver);
}

bool SymbolIndexImpl::foreachCanonicalSymbolOccurrenceByName(StringRef name,
                             function_ref<bool(SymbolOccurrenceRef)> receiver) {
  return foreachCanonicalSymbolOccurrenceImpl(/*workspaceOnly=*/false,
                                              [=](ReadTransaction &reader,
                                                 function_ref<bool (ArrayRef<IDCode>)> usrConsumer) -> bool {
    return reader.foreachUSRBySymbolName(name, usrConsumer);
  }, receiver);
}

bool SymbolIndexImpl::foreachSymbolName(function_ref<bool(StringRef name)> receiver) {
  ReadTransaction reader(DBase);
  return reader.foreachSymbolName(std::move(receiver));
}

bool SymbolIndexImpl::foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                       function_ref<bool(SymbolOccurrenceRef occur)> receiver) {
  IDCode usrCode = makeIDCodeFromString(USR);
  for (auto ProvInfo : findCanonicalProvidersForUSR(usrCode)) {
    bool HasCanonical = ProvInfo.second;
    SymbolRole RoleToSearch = HasCanonical ? SymbolRole::Canonical : SymbolRole::Declaration;
    bool Continue = ProvInfo.first->foreachSymbolOccurrenceByUSR(usrCode, RoleToSearch, [&](SymbolOccurrenceRef Occur)->bool {
      return receiver(std::move(Occur));
    });
    if (!Continue)
      return false;
  }
  return true;
}

size_t SymbolIndexImpl::countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly) {
  size_t totalCount = 0;
  foreachCanonicalSymbolImpl(workspaceOnly,
                                    [=](ReadTransaction &reader, function_ref<bool (ArrayRef<IDCode>)> usrConsumer) -> bool {
    return reader.foreachUSROfGlobalSymbolKind(symKind, usrConsumer);
  }, [&totalCount](SymbolDataProviderRef Prov, std::vector<std::pair<IDCode, bool>> USRsInfo) -> bool {
    totalCount += USRsInfo.size();
    return true;
  });

  return totalCount;
}

bool SymbolIndexImpl::foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                                             function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return foreachCanonicalSymbolOccurrenceImpl(workspaceOnly,
                                              [=](ReadTransaction &reader, function_ref<bool (ArrayRef<IDCode>)> usrConsumer) -> bool {
    return reader.foreachUSROfGlobalSymbolKind(symKind, usrConsumer);
  }, Receiver);
}

std::vector<std::pair<SymbolDataProviderRef, bool>>
SymbolIndexImpl::findCanonicalProvidersForUSR(IDCode usrCode) {
  std::vector<std::pair<SymbolDataProviderRef, bool>> foundProvs;

  SymbolRoleSet DeclOrCanon = SymbolRoleSet(SymbolRole::Declaration) | SymbolRole::Canonical;
  ReadTransaction reader(DBase);
  // Pairs of (IDCode, isCanon), definitions go at the front.
  std::deque<std::pair<IDCode, bool>> provCodes;
  reader.lookupProvidersForUSR(usrCode, DeclOrCanon, None, [&](IDCode providerCode, SymbolRoleSet roles, SymbolRoleSet relatedRoles) -> bool {
    if (roles.contains(SymbolRole::Canonical)) {
      provCodes.push_front(std::make_pair(providerCode, true));
    } else {
      provCodes.push_back(std::make_pair(providerCode, false));
    }
    return true;
  });

  // Providers containing definitions are in the front of the queue so they have higher priority.
  bool foundCanon = false;
  for (auto provCodeAndHasDef : provCodes) {
    IDCode provCode = provCodeAndHasDef.first;
    bool isCanon = provCodeAndHasDef.second;
    if (!isCanon && foundCanon)
      break;
    if (auto prov = createProviderForCode(provCode, reader))
      foundProvs.emplace_back(std::move(prov), isCanon);
    foundCanon |= isCanon;
  }
  return foundProvs;
}

//===----------------------------------------------------------------------===//
// SymbolIndex
//===----------------------------------------------------------------------===//

void SymbolDataProvider::anchor() {}

SymbolIndex::SymbolIndex(DatabaseRef dbase, indexstore::IndexStoreRef indexStore,
                         std::shared_ptr<FileVisibilityChecker> visibilityChecker) {
  Impl = new SymbolIndexImpl(std::move(dbase), std::move(indexStore), std::move(visibilityChecker));
}

#define IMPL static_cast<SymbolIndexImpl*>(Impl)

SymbolIndex::~SymbolIndex() {
  delete IMPL;
}

DatabaseRef SymbolIndex::getDBase() const {
  return IMPL->getDBase();
}

void SymbolIndex::importSymbols(ImportTransaction &Import, SymbolDataProviderRef Provider) {
  return IMPL->importSymbols(Import, std::move(Provider));
}

void SymbolIndex::printStats(raw_ostream &OS) {
  return IMPL->printStats(OS);
}

void SymbolIndex::dumpProviderFileAssociations(raw_ostream &OS) {
  return IMPL->dumpProviderFileAssociations(OS);
}

bool SymbolIndex::foreachSymbolOccurrenceByUSR(StringRef USR,
                                                SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachSymbolOccurrenceByUSR(USR, RoleSet, std::move(Receiver));
}

bool SymbolIndex::foreachRelatedSymbolOccurrenceByUSR(StringRef USR,
                                                SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachRelatedSymbolOccurrenceByUSR(USR, RoleSet, std::move(Receiver));
}

bool SymbolIndex::foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                           bool AnchorStart,
                                                           bool AnchorEnd,
                                                           bool Subsequence,
                                                           bool IgnoreCase,
                             function_ref<bool(SymbolOccurrenceRef)> Receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceContainingPattern(Pattern, AnchorStart, AnchorEnd,
                                                        Subsequence, IgnoreCase,
                                                        std::move(Receiver));
}

bool SymbolIndex::foreachCanonicalSymbolOccurrenceByName(StringRef name,
                       function_ref<bool(SymbolOccurrenceRef Occur)> receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceByName(name, std::move(receiver));
}

bool SymbolIndex::foreachSymbolName(function_ref<bool(StringRef name)> receiver) {
  return IMPL->foreachSymbolName(std::move(receiver));
}

bool SymbolIndex::foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                       function_ref<bool(SymbolOccurrenceRef occur)> receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceByUSR(USR, std::move(receiver));
}

size_t SymbolIndex::countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly) {
  return IMPL->countOfCanonicalSymbolsWithKind(symKind, workspaceOnly);
}

bool SymbolIndex::foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                                         function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  return IMPL->foreachCanonicalSymbolOccurrenceByKind(symKind, workspaceOnly, std::move(Receiver));
}
