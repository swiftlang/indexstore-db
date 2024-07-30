//===--- Symbol.h - Types and functions for indexing symbols --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Provides a C++ interface that matches the types in indexstore.h, and should
// be kept in sync with that header.
//
//===----------------------------------------------------------------------===//

#ifndef INDEXSTOREDB_CORE_SYMBOL_H
#define INDEXSTOREDB_CORE_SYMBOL_H

#include "IndexStoreDB/Support/LLVM.h"
#include "IndexStoreDB/Support/Path.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/OptionSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Chrono.h"
#include <memory>
#include <string>

namespace IndexStoreDB {

enum class SymbolKind : uint8_t {
  Unknown,

  Module,
  Namespace,
  NamespaceAlias,
  Macro,

  Enum,
  Struct,
  Class,
  Protocol,
  Extension,
  Union,
  TypeAlias,

  Function,
  Variable,
  Parameter,
  Field,
  EnumConstant,

  InstanceMethod,
  ClassMethod,
  StaticMethod,
  InstanceProperty,
  ClassProperty,
  StaticProperty,

  Constructor,
  Destructor,
  ConversionFunction,

  Concept,

  CommentTag,
};

enum class SymbolLanguage : uint8_t {
  C,
  ObjC,
  CXX,
  Swift,
};

enum class SymbolProviderKind : uint8_t {
  // Values need to be stable, changing an existing value requires bumping the
  // database format version.
  Clang = 1,
  Swift = 2,
};

/// Language specific sub-kinds.
enum class SymbolSubKind : uint8_t {
  None,
  CXXCopyConstructor,
  CXXMoveConstructor,
  AccessorGetter,
  AccessorSetter,

  // Swift sub-kinds

  SwiftAccessorWillSet,
  SwiftAccessorDidSet,
  SwiftAccessorAddressor,
  SwiftAccessorMutableAddressor,

  SwiftExtensionOfStruct,
  SwiftExtensionOfClass,
  SwiftExtensionOfEnum,
  SwiftExtensionOfProtocol,

  SwiftPrefixOperator,
  SwiftPostfixOperator,
  SwiftInfixOperator,

  SwiftSubscript,
  SwiftAssociatedType,
  SwiftGenericTypeParam,
};

/// Set of properties that provide additional info about a symbol.
enum class SymbolProperty : uint32_t {
  Generic                       = 1 << 0,
  TemplatePartialSpecialization = 1 << 1,
  TemplateSpecialization        = 1 << 2,
  UnitTest                      = 1 << 3,
  IBAnnotated                   = 1 << 4,
  IBOutletCollection            = 1 << 5,
  GKInspectable                 = 1 << 6,
  Local                         = 1 << 7,
  ProtocolInterface             = 1 << 8,
  SwiftAsync                    = 1 << 16,
};
typedef llvm::OptionSet<SymbolProperty> SymbolPropertySet;

/// Set of roles that are attributed to symbol occurrences.
enum class SymbolRole : uint64_t {
  Declaration = 1 << 0,
  Definition  = 1 << 1,
  Reference   = 1 << 2,
  Read        = 1 << 3,
  Write       = 1 << 4,
  Call        = 1 << 5,
  Dynamic     = 1 << 6,
  AddressOf   = 1 << 7,
  Implicit    = 1 << 8,

  // Relation roles.
  RelationChildOf     = 1 << 9,
  RelationBaseOf      = 1 << 10,
  RelationOverrideOf  = 1 << 11,
  RelationReceivedBy  = 1 << 12,
  RelationCalledBy    = 1 << 13,
  RelationExtendedBy  = 1 << 14,
  RelationAccessorOf  = 1 << 15,
  RelationContainedBy = 1 << 16,
  RelationIBTypeOf    = 1 << 17,
  RelationSpecializationOf = 1 << 18,
  // NOTE: If you add a new role, please add it to applyForEachSymbolRole.

  // Reserve the last bit to mark 'canonical' occurrences. This only exists
  // for the IndexStoreDB Index library, it is not coming from the toolchain.
  // Toolchains report 'raw' data (whether it is declaration or definition), the
  // concept of 'canonical' is higher-level, indicating which occurence is
  // preferable to navigate the user to.
  Canonical = uint64_t(1) << 63,
};
typedef llvm::OptionSet<SymbolRole> SymbolRoleSet;

struct SymbolInfo {
  SymbolKind Kind;
  SymbolSubKind SubKind;
  SymbolPropertySet Properties;
  SymbolLanguage Lang;

  SymbolInfo(SymbolKind kind, SymbolLanguage lang) : Kind(kind), Lang(lang) {}
  SymbolInfo(SymbolKind kind, SymbolSubKind subKind,
             SymbolPropertySet properties, SymbolLanguage lang)
  : Kind(kind), SubKind(subKind), Properties(properties), Lang(lang) {}

  bool isCallable() const;
  bool isClassLike() const;
  bool isClassLikeOrExtension() const;

  /// \returns true if we should lookup declaration occurrences as 'canonical'
  /// for this kind of symbol.
  bool preferDeclarationAsCanonical() const;

  bool includeInGlobalNameSearch() const;
};

class Symbol {
  SymbolInfo SymInfo;
  std::string Name;
  std::string USR;

public:
  Symbol(SymbolInfo Info,
         StringRef Name,
         StringRef USR)
  : SymInfo(Info),
    Name(Name),
    USR(USR) {}

  const SymbolInfo &getSymbolInfo() const { return SymInfo; }
  SymbolKind getSymbolKind() const { return SymInfo.Kind; }
  SymbolSubKind getSymbolSubKind() const { return SymInfo.SubKind; }
  SymbolPropertySet getSymbolProperties() const { return SymInfo.Properties; }
  const std::string &getName() const { return Name; }
  const std::string &getUSR() const { return USR; }
  SymbolLanguage getLanguage() const { return SymInfo.Lang; }

  bool isCallable() const { return SymInfo.isCallable(); }

  void print(raw_ostream &OS) const;
};

typedef std::shared_ptr<Symbol> SymbolRef;

class SymbolRelation {
  SymbolRoleSet Roles;
  SymbolRef Sym;

public:
  SymbolRelation() = default;
  SymbolRelation(SymbolRoleSet Roles, SymbolRef Sym)
    : Roles(Roles), Sym(std::move(Sym)) {}

  SymbolRoleSet getRoles() const { return Roles; }
  SymbolRef getSymbol() const { return Sym; }
};

class TimestampedPath {
  std::string Path;
  std::string ModuleName;
  llvm::sys::TimePoint<> ModificationTime;
  unsigned sysrootPrefixLength = 0;
  bool IsSystem;

public:
  TimestampedPath(StringRef Path, llvm::sys::TimePoint<> ModificationTime, StringRef moduleName, bool isSystem, CanonicalFilePathRef sysroot = {})
    : Path(Path), ModuleName(moduleName), ModificationTime(ModificationTime), IsSystem(isSystem) {
    if (sysroot.contains(CanonicalFilePathRef::getAsCanonicalPath(Path))) {
      sysrootPrefixLength = sysroot.getPath().size();
    }
  }

  const std::string &getPathString() const { return Path; }
  llvm::sys::TimePoint<> getModificationTime() const { return ModificationTime; }
  const std::string &getModuleName() const { return ModuleName; }
  unsigned isSystem() const { return IsSystem; }
  StringRef getPathWithoutSysroot() const {
    return StringRef(getPathString()).drop_front(sysrootPrefixLength);
  }

  bool isInvalid() const { return Path.empty(); }
};

class SymbolLocation {
  TimestampedPath Path;
  unsigned Line;
  unsigned Column;

public:
  SymbolLocation(TimestampedPath path, unsigned line, unsigned column)
  : Path(std::move(path)), Line(line), Column(column) {}

  const TimestampedPath &getPath() const { return Path; }
  unsigned getLine() const { return Line; }
  unsigned getColumn() const { return Column; }
  unsigned isSystem() const { return Path.isSystem(); }

  void print(llvm::raw_ostream &OS) const;
};

class SymbolOccurrence {
  SymbolRef Sym;
  SymbolRoleSet Roles;
  SymbolLocation SymLoc;
  SymbolProviderKind ProviderKind;
  std::string Target;
  SmallVector<SymbolRelation, 3> Relations;

public:
  SymbolOccurrence(SymbolRef Sym,
                   SymbolRoleSet Roles,
                   SymbolLocation SymLoc,
                   SymbolProviderKind providerKind,
                   std::string Target,
                   ArrayRef<SymbolRelation> Relations)
  : Sym(std::move(Sym)),
    Roles(Roles),
    SymLoc(std::move(SymLoc)),
    ProviderKind(providerKind),
    Target(Target),
    Relations(Relations.begin(), Relations.end()) {}

  SymbolProviderKind getSymbolProviderKind() const { return ProviderKind; }
  SymbolRef getSymbol() const { return Sym; }
  SymbolRoleSet getRoles() const { return Roles; }
  const SymbolLocation &getLocation() const { return SymLoc; }
  const std::string &getTarget() const { return Target; }
  ArrayRef<SymbolRelation> getRelations() const { return Relations; }

  bool isCanonical() const {
    return Roles.contains(SymbolRole::Canonical);
  }

  void foreachRelatedSymbol(SymbolRoleSet Roles,
                            function_ref<void(SymbolRef)> Receiver);

  void print(raw_ostream &OS) const;
};
typedef std::shared_ptr<SymbolOccurrence> SymbolOccurrenceRef;

const char *getSymbolKindString(SymbolKind kind);
void applyForEachSymbolRole(SymbolRoleSet Roles,
                            llvm::function_ref<void(SymbolRole)> Fn);
void printSymbolRoles(SymbolRoleSet Roles, raw_ostream &OS);

Optional<SymbolProviderKind> getSymbolProviderKindFromIdentifer(StringRef ident);

} // namespace IndexStoreDB

#endif
