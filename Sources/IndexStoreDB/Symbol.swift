//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

@_implementationOnly import IndexStoreDB_CIndexStoreDB

public enum IndexSymbolKind: Hashable, Sendable {
  case unknown
  case module
  case namespace
  case namespaceAlias
  case macro
  case `enum`
  case `struct`
  case `class`
  case `protocol`
  case `extension`
  case union
  case `typealias`
  case function
  case variable
  case field
  case enumConstant
  case instanceMethod
  case classMethod
  case staticMethod
  case instanceProperty
  case classProperty
  case staticProperty
  case constructor
  case destructor
  case conversionFunction
  case parameter
  case using
  case concept
  case commentTag
}

public enum Language: Hashable, Sendable {
  case c
  case cxx
  case objc
  case swift
}

public enum IndexSymbolSubKind: Hashable, Sendable {
  case none
  case cxxCopyConstructor
  case cxxMoveConstructor
  case accessorGetter
  case accessorSetter
  case swiftAccessorWillSet
  case swiftAccessorDidSet
  case swiftAccessorAddressor
  case swiftAccessorMutableAddressor
  case swiftExtensionOfStruct
  case swiftExtensionOfClass
  case swiftExtensionOfEnum
  case swiftExtensionOfProtocol
  case swiftPrefixOperator
  case swiftPostfixOperator
  case swiftInfixOperator
  case swiftSubscript
  case swiftAssociatedType
  case swiftGenericTypeParam
}

public struct Symbol: Hashable, Sendable {

  public var usr: String
  public var name: String
  public var kind: IndexSymbolKind
  public var properties: SymbolProperty
  public var language: Language
  public var subKind: IndexSymbolSubKind

  public init(
    usr: String,
    name: String,
    kind: IndexSymbolKind,
    properties: SymbolProperty = SymbolProperty(),
    language: Language,
    subKind: IndexSymbolSubKind = .none
  ) {
    self.usr = usr
    self.name = name
    self.kind = kind
    self.properties = properties
    self.language = language
    self.subKind = subKind
  }
}

extension Symbol: Comparable {
  public static func < (a: Symbol, b: Symbol) -> Bool {
    return (a.usr, a.name) < (b.usr, b.name)
  }
}

extension Symbol: CustomStringConvertible {
  public var description: String {
    if properties.isEmpty {
      return "\(name) | \(kind) | \(usr) | \(language)"
    }
    return "\(name) | \(kind) (\(properties)) | \(usr) | \(language)"
  }
}

extension Symbol {

  /// Returns a copy of the symbol with the new name, usr, kind, and/or properties.
  public func with(
    name: String? = nil,
    usr: String? = nil,
    kind: IndexSymbolKind? = nil,
    properties: SymbolProperty? = nil,
    language: Language? = nil,
    subKind: IndexSymbolSubKind? = nil
  ) -> Symbol {
    return Symbol(
      usr: usr ?? self.usr,
      name: name ?? self.name,
      kind: kind ?? self.kind,
      properties: properties ?? self.properties,
      language: language ?? self.language,
      subKind: subKind ?? self.subKind
    )
  }

  /// Returns a SymbolOccurrence with the given location and roles.
  public func at(
    _ location: SymbolLocation,
    symbolProvider: SymbolProviderKind,
    roles: SymbolRole
  ) -> SymbolOccurrence {
    return SymbolOccurrence(symbol: self, location: location, roles: roles, symbolProvider: symbolProvider)
  }
}

// MARK: CIndexStoreDB conversions

fileprivate extension Language {
  init(_ value: indexstoredb_language_t) {
    switch value {
    case INDEXSTOREDB_LANGUAGE_C:
      self = .c
    case INDEXSTOREDB_LANGUAGE_CXX:
      self = .cxx
    case INDEXSTOREDB_LANGUAGE_OBJC:
      self = .objc
    case INDEXSTOREDB_LANGUAGE_SWIFT:
      self = .swift
    default:
      preconditionFailure("Unhandled case from C enum indexstoredb_language_t")
    }
  }
}

extension Symbol {

  /// Note: `value` is expected to be passed +1.
  init(_ value: indexstoredb_symbol_t) {
    self.init(
      usr: String(cString: indexstoredb_symbol_usr(value)),
      name: String(cString: indexstoredb_symbol_name(value)),
      kind: IndexSymbolKind(indexstoredb_symbol_kind(value)),
      properties: SymbolProperty(rawValue: indexstoredb_symbol_properties(value)),
      language: Language(indexstoredb_symbol_language(value)),
      subKind: IndexSymbolSubKind(indexstoredb_symbol_subkind(value))
    )
  }
}

extension IndexSymbolKind {
  init(_ cSymbolKind: indexstoredb_symbol_kind_t) {
    switch cSymbolKind {
    case INDEXSTOREDB_SYMBOL_KIND_UNKNOWN:
      self = .unknown
    case INDEXSTOREDB_SYMBOL_KIND_MODULE:
      self = .module
    case INDEXSTOREDB_SYMBOL_KIND_NAMESPACE:
      self = .namespace
    case INDEXSTOREDB_SYMBOL_KIND_NAMESPACEALIAS:
      self = .namespaceAlias
    case INDEXSTOREDB_SYMBOL_KIND_MACRO:
      self = .macro
    case INDEXSTOREDB_SYMBOL_KIND_ENUM:
      self = .enum
    case INDEXSTOREDB_SYMBOL_KIND_STRUCT:
      self = .struct
    case INDEXSTOREDB_SYMBOL_KIND_CLASS:
      self = .class
    case INDEXSTOREDB_SYMBOL_KIND_PROTOCOL:
      self = .protocol
    case INDEXSTOREDB_SYMBOL_KIND_EXTENSION:
      self = .extension
    case INDEXSTOREDB_SYMBOL_KIND_UNION:
      self = .union
    case INDEXSTOREDB_SYMBOL_KIND_TYPEALIAS:
      self = .typealias
    case INDEXSTOREDB_SYMBOL_KIND_FUNCTION:
      self = .function
    case INDEXSTOREDB_SYMBOL_KIND_VARIABLE:
      self = .variable
    case INDEXSTOREDB_SYMBOL_KIND_FIELD:
      self = .field
    case INDEXSTOREDB_SYMBOL_KIND_ENUMCONSTANT:
      self = .enumConstant
    case INDEXSTOREDB_SYMBOL_KIND_INSTANCEMETHOD:
      self = .instanceMethod
    case INDEXSTOREDB_SYMBOL_KIND_CLASSMETHOD:
      self = .classMethod
    case INDEXSTOREDB_SYMBOL_KIND_STATICMETHOD:
      self = .staticMethod
    case INDEXSTOREDB_SYMBOL_KIND_INSTANCEPROPERTY:
      self = .instanceProperty
    case INDEXSTOREDB_SYMBOL_KIND_CLASSPROPERTY:
      self = .classProperty
    case INDEXSTOREDB_SYMBOL_KIND_STATICPROPERTY:
      self = .staticProperty
    case INDEXSTOREDB_SYMBOL_KIND_CONSTRUCTOR:
      self = .constructor
    case INDEXSTOREDB_SYMBOL_KIND_DESTRUCTOR:
      self = .destructor
    case INDEXSTOREDB_SYMBOL_KIND_CONVERSIONFUNCTION:
      self = .conversionFunction
    case INDEXSTOREDB_SYMBOL_KIND_PARAMETER:
      self = .parameter
    case INDEXSTOREDB_SYMBOL_KIND_USING:
      self = .using
    case INDEXSTOREDB_SYMBOL_KIND_CONCEPT:
      self = .concept
    case INDEXSTOREDB_SYMBOL_KIND_COMMENTTAG:
      self = .commentTag
    default:
      self = .unknown
    }
  }
}


extension IndexSymbolSubKind {
  init(_ cSymbolSubKind: indexstoredb_symbol_subkind_t) {
    switch cSymbolSubKind {
    case INDEXSTOREDB_SYMBOL_SUBKIND_NONE:
      self = .none
    case INDEXSTOREDB_SYMBOL_SUBKIND_CXXCOPYCONSTRUCTOR:
      self = .cxxCopyConstructor
    case INDEXSTOREDB_SYMBOL_SUBKIND_CXXMOVECONSTRUCTOR:
      self = .cxxMoveConstructor
    case INDEXSTOREDB_SYMBOL_SUBKIND_ACCESSORGETTER:
      self = .accessorGetter
    case INDEXSTOREDB_SYMBOL_SUBKIND_ACCESSORSETTER:
      self = .accessorSetter
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTACCESSORWILLSET:
      self = .swiftAccessorWillSet
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTACCESSORDIDSET:
      self = .swiftAccessorDidSet
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTACCESSORADDRESSOR:
      self = .swiftAccessorAddressor
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTACCESSORMUTABLEADDRESSOR:
      self = .swiftAccessorMutableAddressor
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTEXTENSIONOFSTRUCT:
      self = .swiftExtensionOfStruct
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTEXTENSIONOFCLASS:
      self = .swiftExtensionOfClass
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTEXTENSIONOFENUM:
      self = .swiftExtensionOfEnum
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTEXTENSIONOFPROTOCOL:
      self = .swiftExtensionOfProtocol
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTPREFIXOPERATOR:
      self = .swiftPrefixOperator
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTPOSTFIXOPERATOR:
      self = .swiftPostfixOperator
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTINFIXOPERATOR:
      self = .swiftInfixOperator
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTSUBSCRIPT:
      self = .swiftSubscript
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTASSOCIATEDTYPE:
      self = .swiftAssociatedType
    case INDEXSTOREDB_SYMBOL_SUBKIND_SWIFTGENERICTYPEPARAM:
      self = .swiftGenericTypeParam
    default:
      self = .none
    }
  }
}
