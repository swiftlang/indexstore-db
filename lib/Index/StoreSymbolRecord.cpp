//===--- StoreSymbolRecord.cpp --------------------------------------------===//
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

#include "StoreSymbolRecord.h"
#include "IndexDatastore.h"
#include "IndexStoreDB/Database/Database.h"
#include "IndexStoreDB/Support/Logging.h"
#include "IndexStoreDB/Support/Path.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;
using namespace IndexStoreDB::index;
using namespace indexstore;
using namespace llvm;

static SymbolLanguage convertStoreLanguage(indexstore_symbol_language_t storeLang) {
  switch(storeLang) {
  case INDEXSTORE_SYMBOL_LANG_C:
    return SymbolLanguage::C;
  case INDEXSTORE_SYMBOL_LANG_OBJC:
    return SymbolLanguage::ObjC;
  case INDEXSTORE_SYMBOL_LANG_CXX:
    return SymbolLanguage::CXX;
  case INDEXSTORE_SYMBOL_LANG_SWIFT:
    return SymbolLanguage::Swift;
  default:
    return SymbolLanguage::C; // fallback.
  }
}

static SymbolKind convertStoreSymbolKind(indexstore_symbol_kind_t storeKind) {
  switch(storeKind) {
  case INDEXSTORE_SYMBOL_KIND_MODULE:
    return SymbolKind::Module;
  case INDEXSTORE_SYMBOL_KIND_NAMESPACE:
    return SymbolKind::Namespace;
  case INDEXSTORE_SYMBOL_KIND_NAMESPACEALIAS:
    return SymbolKind::NamespaceAlias;
  case INDEXSTORE_SYMBOL_KIND_MACRO:
    return SymbolKind::Macro;
  case INDEXSTORE_SYMBOL_KIND_ENUM:
    return SymbolKind::Enum;
  case INDEXSTORE_SYMBOL_KIND_STRUCT:
    return SymbolKind::Struct;
  case INDEXSTORE_SYMBOL_KIND_CLASS:
    return SymbolKind::Class;
  case INDEXSTORE_SYMBOL_KIND_PROTOCOL:
    return SymbolKind::Protocol;
  case INDEXSTORE_SYMBOL_KIND_EXTENSION:
    return SymbolKind::Extension;
  case INDEXSTORE_SYMBOL_KIND_UNION:
    return SymbolKind::Union;
  case INDEXSTORE_SYMBOL_KIND_TYPEALIAS:
    return SymbolKind::TypeAlias;
  case INDEXSTORE_SYMBOL_KIND_FUNCTION:
    return SymbolKind::Function;
  case INDEXSTORE_SYMBOL_KIND_VARIABLE:
    return SymbolKind::Variable;
  case INDEXSTORE_SYMBOL_KIND_FIELD:
    return SymbolKind::Field;
  case INDEXSTORE_SYMBOL_KIND_ENUMCONSTANT:
    return SymbolKind::EnumConstant;
  case INDEXSTORE_SYMBOL_KIND_INSTANCEMETHOD:
    return SymbolKind::InstanceMethod;
  case INDEXSTORE_SYMBOL_KIND_CLASSMETHOD:
    return SymbolKind::ClassMethod;
  case INDEXSTORE_SYMBOL_KIND_STATICMETHOD:
    return SymbolKind::StaticMethod;
  case INDEXSTORE_SYMBOL_KIND_INSTANCEPROPERTY:
    return SymbolKind::InstanceProperty;
  case INDEXSTORE_SYMBOL_KIND_CLASSPROPERTY:
    return SymbolKind::ClassProperty;
  case INDEXSTORE_SYMBOL_KIND_STATICPROPERTY:
    return SymbolKind::StaticProperty;
  case INDEXSTORE_SYMBOL_KIND_CONSTRUCTOR:
    return SymbolKind::Constructor;
  case INDEXSTORE_SYMBOL_KIND_DESTRUCTOR:
    return SymbolKind::Destructor;
  case INDEXSTORE_SYMBOL_KIND_CONVERSIONFUNCTION:
    return SymbolKind::ConversionFunction;
  case INDEXSTORE_SYMBOL_KIND_PARAMETER:
    return SymbolKind::Parameter;
#if INDEXSTORE_VERSION >= 9
  case INDEXSTORE_SYMBOL_KIND_COMMENTTAG:
    return SymbolKind::CommentTag;
#endif
  default:
    return SymbolKind::Unknown;
  }
}

static SymbolSubKind convertStoreSymbolSubKind(indexstore_symbol_subkind_t storeKind) {
  switch (storeKind) {
  case INDEXSTORE_SYMBOL_SUBKIND_CXXCOPYCONSTRUCTOR:
    return SymbolSubKind::CXXCopyConstructor;
  case INDEXSTORE_SYMBOL_SUBKIND_CXXMOVECONSTRUCTOR:
    return SymbolSubKind::CXXMoveConstructor;
  case INDEXSTORE_SYMBOL_SUBKIND_ACCESSORGETTER:
    return SymbolSubKind::AccessorGetter;
  case INDEXSTORE_SYMBOL_SUBKIND_ACCESSORSETTER:
    return SymbolSubKind::AccessorSetter;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORWILLSET:
    return SymbolSubKind::SwiftAccessorWillSet;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORDIDSET:
    return SymbolSubKind::SwiftAccessorDidSet;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORADDRESSOR:
    return SymbolSubKind::SwiftAccessorAddressor;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORMUTABLEADDRESSOR:
    return SymbolSubKind::SwiftAccessorMutableAddressor;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFSTRUCT:
    return SymbolSubKind::SwiftExtensionOfStruct;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFCLASS:
    return SymbolSubKind::SwiftExtensionOfClass;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFENUM:
    return SymbolSubKind::SwiftExtensionOfEnum;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFPROTOCOL:
    return SymbolSubKind::SwiftExtensionOfProtocol;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTPREFIXOPERATOR:
    return SymbolSubKind::SwiftPrefixOperator;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTPOSTFIXOPERATOR:
    return SymbolSubKind::SwiftPostfixOperator;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTINFIXOPERATOR:
    return SymbolSubKind::SwiftInfixOperator;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTSUBSCRIPT:
    return SymbolSubKind::SwiftSubscript;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTASSOCIATEDTYPE:
    return SymbolSubKind::SwiftAssociatedType;
  case INDEXSTORE_SYMBOL_SUBKIND_SWIFTGENERICTYPEPARAM:
    return SymbolSubKind::SwiftGenericTypeParam;
  default:
    return SymbolSubKind::None;
  }
}

static SymbolInfo getSymbolInfo(IndexRecordSymbol sym) {
  return SymbolInfo{ convertStoreSymbolKind(sym.getKind()),
                convertStoreSymbolSubKind(sym.getSubKind()),
                SymbolPropertySet(sym.getProperties()),
                convertStoreLanguage(sym.getLanguage()) };
}

static SymbolRef convertSymbol(IndexRecordSymbol sym) {
  return std::make_shared<Symbol>(getSymbolInfo(sym),
    sym.getName(), sym.getUSR());
}

static SymbolRoleSet convertFromIndexStoreRoles(uint64_t Roles, bool isCanonical) {
  SymbolRoleSet newRoles = SymbolRoleSet(Roles);
  if (isCanonical)
    newRoles |= SymbolRole::Canonical;
  return newRoles;
}

static SymbolRoleSet convertFromIndexStoreRoles(uint64_t Roles, const SymbolInfo &sym) {
  bool isCanonical;
  if (sym.preferDeclarationAsCanonical())
    isCanonical = Roles & INDEXSTORE_SYMBOL_ROLE_DECLARATION;
  else
    isCanonical = Roles & INDEXSTORE_SYMBOL_ROLE_DEFINITION;
  return convertFromIndexStoreRoles(Roles, isCanonical);
}


StoreSymbolRecord::~StoreSymbolRecord() {
  // llvm::errs() << "Destructing record: " << RecordName << '\n';
}

std::shared_ptr<StoreSymbolRecord>
StoreSymbolRecord::create(IndexStoreRef store,
                          StringRef recordName, IDCode providerCode,
                          SymbolProviderKind symProviderKind,
                          ArrayRef<FileAndTarget> fileReferences) {
  auto Rec = std::make_shared<StoreSymbolRecord>();
  Rec->Store = std::move(store);
  Rec->RecordName = recordName;
  Rec->ProviderCode = providerCode;
  Rec->SymProviderKind = symProviderKind;
  Rec->FileAndTargetRefs = fileReferences;
  return Rec;
}

bool StoreSymbolRecord::doForData(function_ref<void(IndexRecordReader &)> Action) {
  // FIXME: Cache this using libcache ? We may need to repeat searches.
  std::string Error;
  auto Reader = IndexRecordReader(*Store, RecordName, Error);
  if (!Reader) {
    LOG_WARN_FUNC("error reading record '"<< RecordName <<"': " << Error);
    return true;
  }

  Action(Reader);
  return false;
}

namespace {
class OccurrenceConverter {
  function_ref<bool(SymbolOccurrenceRef Occur)> Receiver;
  std::vector<FileAndTarget> FileAndTargetRefs;
  SymbolProviderKind SymProviderKind;

public:
  OccurrenceConverter(StoreSymbolRecord &SymRecord,
    function_ref<bool(SymbolOccurrenceRef Occur)> Receiver)
    : Receiver(std::move(Receiver)) {
      FileAndTargetRefs = SymRecord.getSourceFileReferencesAndTargets();
      SymProviderKind = SymRecord.getProviderKind();
    }

  bool operator()(IndexRecordOccurrence RecSym) {
    auto Sym = convertSymbol(RecSym.getSymbol());
    SymbolRoleSet OccurRoles = convertFromIndexStoreRoles(RecSym.getRoles(), Sym->getSymbolInfo());
    SmallVector<SymbolRelation, 4> Relations;
    RecSym.foreachRelation([&](IndexSymbolRelation Rel) -> bool {
      SymbolRoleSet Roles = convertFromIndexStoreRoles(Rel.getRoles(), /*isCanonical=*/false);
      SymbolRef RelSym = convertSymbol(Rel.getSymbol());
      Relations.emplace_back(Roles, std::move(RelSym));
      return true;
    });
    for (auto &FileRef : FileAndTargetRefs) {
      auto LineCol = RecSym.getLineCol();
      SymbolLocation SymLoc(FileRef.Path, LineCol.first, LineCol.second);
      auto Occur = std::make_shared<SymbolOccurrence>(Sym, OccurRoles,
                                                      std::move(SymLoc),
                                                      SymProviderKind,
                                                      FileRef.Target,
                                                      Relations);
      bool Continue = Receiver(std::move(Occur));
      if (!Continue)
        return false;
    }
    return true;
  }
};

class PredOccurrenceConverter {
  function_ref<bool(IndexRecordOccurrence)> Predicate;
  OccurrenceConverter Converter;

public:
  PredOccurrenceConverter(StoreSymbolRecord &SymRecord,
    function_ref<bool(IndexRecordOccurrence)> Predicate,
    function_ref<bool(SymbolOccurrenceRef Occur)> Receiver)
    : Predicate(std::move(Predicate)),
      Converter(SymRecord, Receiver) { }

  bool operator()(IndexRecordOccurrence RecSym) {
    if (Predicate(RecSym) == false)
      return true;

    return Converter(RecSym);
  }
};
}

bool StoreSymbolRecord::isSystem() const {
  if (FileAndTargetRefs.empty())
    return false;
  return FileAndTargetRefs.front().Path.isSystem();
}

bool StoreSymbolRecord::foreachCoreSymbolData(function_ref<bool(StringRef USR,
                                                                StringRef Name,
                                                                SymbolInfo Info,
                                                                SymbolRoleSet Roles,
                                                                SymbolRoleSet RelatedRoles)> Receiver) {
  bool Finished;
  bool Err = doForData([&](IndexRecordReader &Reader) {
    Finished = Reader.foreachSymbol(/*noCache=*/true, [&](IndexRecordSymbol sym)->bool {
      auto symInfo = getSymbolInfo(sym);
      return Receiver(sym.getUSR(), sym.getName(), symInfo,
                      convertFromIndexStoreRoles(sym.getRoles(), symInfo),
                      convertFromIndexStoreRoles(sym.getRelatedRoles(), /*isCanonical=*/false));
    });
  });

  return !Err && Finished;
}

static void searchDeclsByUSR(IndexRecordReader &Reader,
                             ArrayRef<db::IDCode> USRs,
           SmallVectorImpl<IndexRecordSymbol> &FoundDecls) {

  SmallVector<db::IDCode, 8> NotFoundUSRs(USRs.begin(), USRs.end());
  auto filter = [&](IndexRecordSymbol RecSym, bool &stop) -> bool {
    db::IDCode RecSymUSRCode = db::makeIDCodeFromString(RecSym.getUSR());
    auto It = std::find(NotFoundUSRs.begin(), NotFoundUSRs.end(), RecSymUSRCode);
    if (It == NotFoundUSRs.end())
      return false;
    // FIXME: Ideally we would stop looking for a USR once we found it, but
    // we are having records where symbols can show up multiple times (with different roles).
    // NotFoundUSRs.erase(It);
    // stop = NotFoundUSRs.empty();
    return true;
  };
  auto receiver = [&](IndexRecordSymbol sym) {
    FoundDecls.push_back(sym);
  };

  Reader.searchSymbols(filter, receiver);
}

namespace {
class CheckIndexStoreRolesPredicate {
  SymbolRoleSet Roles;
public:
  CheckIndexStoreRolesPredicate(SymbolRoleSet roles) : Roles(roles) {}
  bool operator()(IndexRecordOccurrence recOccur) {
    SymbolRoleSet occurRoles;
    // Only retrieve the symbol if we need to (there is a 'canonical' check).
    if (Roles.contains(SymbolRole::Canonical)) {
      auto symInfo = getSymbolInfo(recOccur.getSymbol());
      occurRoles = convertFromIndexStoreRoles(recOccur.getRoles(), symInfo);
    } else {
      occurRoles = convertFromIndexStoreRoles(recOccur.getRoles(), /*isCanonical=*/false);
    }
    return occurRoles.containsAny(Roles);
  }
};
}

bool StoreSymbolRecord::foreachSymbolOccurrenceByUSR(ArrayRef<db::IDCode> USRs,
                                                     SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  assert(!USRs.empty() && "did not set any USR!");
  assert(RoleSet && "did not set any role!");

  bool Finished = true;
  bool Err = doForData([&](IndexRecordReader &Reader) {
    SmallVector<IndexRecordSymbol, 8> FoundDecls;
    searchDeclsByUSR(Reader, USRs, FoundDecls);
    if (FoundDecls.empty())
      return;

    CheckIndexStoreRolesPredicate Pred(RoleSet);
    PredOccurrenceConverter Converter(*this, Pred, Receiver);
    Finished = Reader.foreachOccurrence(/*DeclsFilter=*/FoundDecls,
                                        /*RelatedDeclsFilter=*/None,
                                        Converter);
  });

  return !Err && Finished;
}

bool StoreSymbolRecord::foreachRelatedSymbolOccurrenceByUSR(ArrayRef<db::IDCode> USRs,
                                                     SymbolRoleSet RoleSet,
                       function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  assert(!USRs.empty() && "did not set any USR!");
  assert(RoleSet && "did not set any role!");

  bool Finished = true;
  bool Err = doForData([&](IndexRecordReader &Reader) {
    SmallVector<IndexRecordSymbol, 8> FoundDecls;
    searchDeclsByUSR(Reader, USRs, FoundDecls);
    if (FoundDecls.empty())
      return;

    CheckIndexStoreRolesPredicate Pred(RoleSet);
    PredOccurrenceConverter Converter(*this, Pred, Receiver);
    Finished = Reader.foreachOccurrence(/*DeclsFilter=*/None,
                                        /*RelatedDeclsFilter=*/FoundDecls,
                                        Converter);
  });

  return !Err && Finished;
}

bool StoreSymbolRecord::foreachUnitTestSymbolOccurrence(function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) {
  bool Finished = true;
  bool Err = doForData([&](IndexRecordReader &Reader) {
    SmallVector<IndexRecordSymbol, 8> FoundDecls;
    auto filter = [&](IndexRecordSymbol recSym, bool &stop) -> bool {
      auto symInfo = getSymbolInfo(recSym);
      return symInfo.Properties.contains(SymbolProperty::UnitTest);
    };
    auto receiver = [&](IndexRecordSymbol sym) {
      FoundDecls.push_back(sym);
    };
    Reader.searchSymbols(filter, receiver);
    if (FoundDecls.empty())
      return;

    // Return all occurrences.
    auto Pred = [](IndexRecordOccurrence) -> bool { return true; };
    PredOccurrenceConverter Converter(*this, Pred, Receiver);
    Finished = Reader.foreachOccurrence(/*symbolsFilter=*/FoundDecls,
                                        /*relatedSymbolsFilter=*/None,
                                        Converter);
  });

  return !Err && Finished;
}
