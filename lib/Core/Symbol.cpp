//===--- Symbol.cpp -------------------------------------------------------===//
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
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace IndexStoreDB;

bool SymbolInfo::isCallable() const {
  switch (Kind) {
  case SymbolKind::Function:
  case SymbolKind::InstanceMethod:
  case SymbolKind::ClassMethod:
  case SymbolKind::StaticMethod:
  case SymbolKind::Constructor:
  case SymbolKind::Destructor:
  case SymbolKind::ConversionFunction:
    return true;
  default:
    return false;
  }
}

bool SymbolInfo::isClassLike() const {
  switch (Kind) {
    case SymbolKind::Class:
    case SymbolKind::Struct:
      return Lang != SymbolLanguage::C;
    default:
      return false;
  }
}

bool SymbolInfo::isClassLikeOrExtension() const {
  return isClassLike() || Kind == SymbolKind::Extension;
}

bool SymbolInfo::preferDeclarationAsCanonical() const {
  if (Lang == SymbolLanguage::ObjC) {
    return Kind == SymbolKind::Class ||
      Kind == SymbolKind::Extension ||
      Kind == SymbolKind::InstanceProperty ||
      Kind == SymbolKind::ClassProperty;
  }
  return false;
}

bool SymbolInfo::includeInGlobalNameSearch() const {
  // Swift extensions don't have their own name, exclude them from global name search.
  // You can always lookup the class name and then find the class symbol extensions.
  if (Kind == SymbolKind::Extension && Lang == SymbolLanguage::Swift)
    return false;
  return true;
}

const char *IndexStoreDB::getSymbolKindString(SymbolKind kind) {
  switch (kind) {
  case SymbolKind::Unknown: return "unknown";
  case SymbolKind::Module: return "module";
  case SymbolKind::Macro: return "macro";
  case SymbolKind::Enum: return "enum";
  case SymbolKind::Struct: return "struct";
  case SymbolKind::Class: return "class";
  case SymbolKind::Protocol: return "protocol";
  case SymbolKind::Extension: return "extension";
  case SymbolKind::Union: return "union";
  case SymbolKind::TypeAlias: return "typealias";
  case SymbolKind::Function: return "function";
  case SymbolKind::Variable: return "variable";
  case SymbolKind::Field: return "field";
  case SymbolKind::Parameter: return "parameter";
  case SymbolKind::EnumConstant: return "enumerator";
  case SymbolKind::InstanceMethod: return "instance-method";
  case SymbolKind::ClassMethod: return "class-method";
  case SymbolKind::StaticMethod: return "static-method";
  case SymbolKind::InstanceProperty: return "instance-property";
  case SymbolKind::ClassProperty: return "class-property";
  case SymbolKind::StaticProperty: return "static-property";
  case SymbolKind::Constructor: return "constructor";
  case SymbolKind::Destructor: return "destructor";
  case SymbolKind::ConversionFunction: return "conversion-func";
  case SymbolKind::Namespace: return "namespace";
  case SymbolKind::NamespaceAlias: return "namespace-alias";
  case SymbolKind::CommentTag: return "comment-tag";
  }
  llvm_unreachable("Garbage symbol kind");
}

void Symbol::print(raw_ostream &OS) const {
  OS << getName() << " | ";
  OS << getSymbolKindString(getSymbolKind()) << " | ";
  OS << getUSR();
}

void SymbolLocation::print(raw_ostream &OS) const {
  OS << getPath().getPathString() << ':' << getLine() << ':' << getColumn();
}

void SymbolOccurrence::foreachRelatedSymbol(SymbolRoleSet Roles,
                                            function_ref<void(SymbolRef)> Receiver) {
  for (auto &Rel : getRelations()) {
    if (Rel.getRoles().containsAny(Roles))
      Receiver(Rel.getSymbol());
  }
}

void SymbolOccurrence::print(raw_ostream &OS) const {
  getLocation().print(OS);
  OS << " | ";
  getSymbol()->print(OS);
  OS << " | ";
  printSymbolRoles(getRoles(), OS);
}


void IndexStoreDB::applyForEachSymbolRole(SymbolRoleSet Roles,
                                   llvm::function_ref<void(SymbolRole)> Fn) {
#define APPLY_FOR_ROLE(Role) \
  if (Roles & SymbolRole::Role) \
    Fn(SymbolRole::Role)

  APPLY_FOR_ROLE(Declaration);
  APPLY_FOR_ROLE(Definition);
  APPLY_FOR_ROLE(Reference);
  APPLY_FOR_ROLE(Read);
  APPLY_FOR_ROLE(Write);
  APPLY_FOR_ROLE(Call);
  APPLY_FOR_ROLE(Dynamic);
  APPLY_FOR_ROLE(AddressOf);
  APPLY_FOR_ROLE(Implicit);
  APPLY_FOR_ROLE(RelationChildOf);
  APPLY_FOR_ROLE(RelationBaseOf);
  APPLY_FOR_ROLE(RelationOverrideOf);
  APPLY_FOR_ROLE(RelationReceivedBy);
  APPLY_FOR_ROLE(RelationCalledBy);
  APPLY_FOR_ROLE(RelationExtendedBy);
  APPLY_FOR_ROLE(RelationAccessorOf);
  APPLY_FOR_ROLE(RelationContainedBy);
  APPLY_FOR_ROLE(RelationIBTypeOf);
  APPLY_FOR_ROLE(RelationSpecializationOf);

#undef APPLY_FOR_ROLE
}

void IndexStoreDB::printSymbolRoles(SymbolRoleSet Roles, raw_ostream &OS) {
  bool VisitedOnce = false;
  applyForEachSymbolRole(Roles, [&](SymbolRole Role) {
    if (VisitedOnce)
      OS << ',';
    else
      VisitedOnce = true;
    switch (Role) {
    case SymbolRole::Declaration: OS << "Decl"; break;
    case SymbolRole::Definition: OS << "Def"; break;
    case SymbolRole::Reference: OS << "Ref"; break;
    case SymbolRole::Read: OS << "Read"; break;
    case SymbolRole::Write: OS << "Writ"; break;
    case SymbolRole::Call: OS << "Call"; break;
    case SymbolRole::Dynamic: OS << "Dyn"; break;
    case SymbolRole::AddressOf: OS << "Addr"; break;
    case SymbolRole::Implicit: OS << "Impl"; break;
    case SymbolRole::RelationChildOf: OS << "RelChild"; break;
    case SymbolRole::RelationBaseOf: OS << "RelBase"; break;
    case SymbolRole::RelationOverrideOf: OS << "RelOver"; break;
    case SymbolRole::RelationReceivedBy: OS << "RelRec"; break;
    case SymbolRole::RelationCalledBy: OS << "RelCall"; break;
    case SymbolRole::RelationExtendedBy: OS << "RelExt"; break;
    case SymbolRole::RelationAccessorOf: OS << "RelAcc"; break;
    case SymbolRole::RelationContainedBy: OS << "RelCont"; break;
    case SymbolRole::RelationIBTypeOf: OS << "RelIBType"; break;
    case SymbolRole::RelationSpecializationOf: OS << "RelSpecializationOf"; break;
    case SymbolRole::Canonical: OS << "Canon"; break;
    }
  });
}

Optional<SymbolProviderKind> IndexStoreDB::getSymbolProviderKindFromIdentifer(StringRef ident) {
  return llvm::StringSwitch<Optional<SymbolProviderKind>>(ident)
    .Case("clang", SymbolProviderKind::Clang)
    .Case("swift", SymbolProviderKind::Swift)
    .Default(llvm::None);
}
