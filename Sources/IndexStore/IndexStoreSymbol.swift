//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

public import IndexStoreCAPI

/// A symbol, like a type declaration, function, property or global variable.
///
/// A symbol by itself does not have any source information associated with it. It just represents the declaration
/// itself, `IndexStoreSymbolOccurrence` represents actual occurrences of symbols within a source file.
public struct IndexStoreSymbol: ~Escapable, Sendable {
  public struct Kind: RawRepresentable, Hashable, Sendable, CustomStringConvertible {
    public let rawValue: UInt16

    public static let unknown = Kind(INDEXSTORE_SYMBOL_KIND_UNKNOWN)
    public static let module = Kind(INDEXSTORE_SYMBOL_KIND_MODULE)
    public static let namespace = Kind(INDEXSTORE_SYMBOL_KIND_NAMESPACE)
    public static let namespaceAlias = Kind(INDEXSTORE_SYMBOL_KIND_NAMESPACEALIAS)
    public static let macro = Kind(INDEXSTORE_SYMBOL_KIND_MACRO)
    public static let `enum` = Kind(INDEXSTORE_SYMBOL_KIND_ENUM)
    public static let `struct` = Kind(INDEXSTORE_SYMBOL_KIND_STRUCT)
    public static let `class` = Kind(INDEXSTORE_SYMBOL_KIND_CLASS)
    public static let `protocol` = Kind(INDEXSTORE_SYMBOL_KIND_PROTOCOL)
    public static let `extension` = Kind(INDEXSTORE_SYMBOL_KIND_EXTENSION)
    public static let union = Kind(INDEXSTORE_SYMBOL_KIND_UNION)
    public static let `typealias` = Kind(INDEXSTORE_SYMBOL_KIND_TYPEALIAS)
    public static let function = Kind(INDEXSTORE_SYMBOL_KIND_FUNCTION)
    public static let variable = Kind(INDEXSTORE_SYMBOL_KIND_VARIABLE)
    public static let field = Kind(INDEXSTORE_SYMBOL_KIND_FIELD)
    public static let enumConstant = Kind(INDEXSTORE_SYMBOL_KIND_ENUMCONSTANT)
    public static let instanceMethod = Kind(INDEXSTORE_SYMBOL_KIND_INSTANCEMETHOD)
    public static let classMethod = Kind(INDEXSTORE_SYMBOL_KIND_CLASSMETHOD)
    public static let staticMethod = Kind(INDEXSTORE_SYMBOL_KIND_STATICMETHOD)
    public static let instanceProperty = Kind(INDEXSTORE_SYMBOL_KIND_INSTANCEPROPERTY)
    public static let classProperty = Kind(INDEXSTORE_SYMBOL_KIND_CLASSPROPERTY)
    public static let staticProperty = Kind(INDEXSTORE_SYMBOL_KIND_STATICPROPERTY)
    public static let constructor = Kind(INDEXSTORE_SYMBOL_KIND_CONSTRUCTOR)
    public static let destructor = Kind(INDEXSTORE_SYMBOL_KIND_DESTRUCTOR)
    public static let conversionFunction = Kind(INDEXSTORE_SYMBOL_KIND_CONVERSIONFUNCTION)
    public static let parameter = Kind(INDEXSTORE_SYMBOL_KIND_PARAMETER)
    public static let using = Kind(INDEXSTORE_SYMBOL_KIND_USING)
    public static let concept = Kind(INDEXSTORE_SYMBOL_KIND_CONCEPT)
    public static let commentTag = Kind(INDEXSTORE_SYMBOL_KIND_COMMENTTAG)

    public var description: String {
      switch self {
      case .unknown: return "unknown"
      case .module: return "module"
      case .namespace: return "namespace"
      case .namespaceAlias: return "namespaceAlias"
      case .macro: return "macro"
      case .enum: return "enum"
      case .struct: return "struct"
      case .class: return "class"
      case .protocol: return "protocol"
      case .extension: return "extension"
      case .union: return "union"
      case .typealias: return "typealias"
      case .function: return "function"
      case .variable: return "variable"
      case .field: return "field"
      case .enumConstant: return "enumConstant"
      case .instanceMethod: return "instanceMethod"
      case .classMethod: return "classMethod"
      case .staticMethod: return "staticMethod"
      case .instanceProperty: return "instanceProperty"
      case .classProperty: return "classProperty"
      case .staticProperty: return "staticProperty"
      case .constructor: return "constructor"
      case .destructor: return "destructor"
      case .conversionFunction: return "conversionFunction"
      case .parameter: return "parameter"
      case .using: return "using"
      case .concept: return "concept"
      case .commentTag: return "commentTag"
      default: return "unknown symbol kind \(rawValue)"
      }
    }

    @inlinable
    public init(rawValue: UInt16) {
      self.rawValue = rawValue
    }

    @usableFromInline
    init(_ kind: indexstore_symbol_kind_t) {
      self.rawValue = UInt16(kind.rawValue)
    }
  }

  public struct SubKind: RawRepresentable, Hashable, Sendable, CustomStringConvertible {
    public let rawValue: UInt16

    public static let none = SubKind(INDEXSTORE_SYMBOL_SUBKIND_NONE)
    public static let cxxCopyConstructor = SubKind(INDEXSTORE_SYMBOL_SUBKIND_CXXCOPYCONSTRUCTOR)
    public static let cxxMoveConstructor = SubKind(INDEXSTORE_SYMBOL_SUBKIND_CXXMOVECONSTRUCTOR)
    public static let accessorGetter = SubKind(INDEXSTORE_SYMBOL_SUBKIND_ACCESSORGETTER)
    public static let accessorSetter = SubKind(INDEXSTORE_SYMBOL_SUBKIND_ACCESSORSETTER)
    public static let usingTypename = SubKind(INDEXSTORE_SYMBOL_SUBKIND_USINGTYPENAME)
    public static let usingValue = SubKind(INDEXSTORE_SYMBOL_SUBKIND_USINGVALUE)
    public static let swiftAccessorWillSet = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORWILLSET)
    public static let swiftAccessorDidSet = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORDIDSET)
    public static let swiftAccessorAddressor = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORADDRESSOR)
    public static let swiftAccessorMutableAddressor = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORMUTABLEADDRESSOR)
    public static let swiftExtensionOfStruct = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFSTRUCT)
    public static let swiftExtensionOfClass = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFCLASS)
    public static let swiftExtensionOfEnum = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFENUM)
    public static let swiftExtensionOfProtocol = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTEXTENSIONOFPROTOCOL)
    public static let swiftPrefixOperator = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTPREFIXOPERATOR)
    public static let swiftPostfixOperator = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTPOSTFIXOPERATOR)
    public static let swiftInfixOperator = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTINFIXOPERATOR)
    public static let swiftSubscript = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTSUBSCRIPT)
    public static let swiftAssociatedtype = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTASSOCIATEDTYPE)
    public static let swiftGenericTypeParam = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTGENERICTYPEPARAM)
    public static let swiftAccessorRead = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORREAD)
    public static let swiftAccessorModify = SubKind(INDEXSTORE_SYMBOL_SUBKIND_SWIFTACCESSORMODIFY)

    @inlinable
    public init(rawValue: UInt16) {
      self.rawValue = rawValue
    }

    @usableFromInline
    init(_ subKind: indexstore_symbol_subkind_t) {
      self.rawValue = UInt16(subKind.rawValue)
    }

    public var description: String {
      switch self {
      case .none: return "none"
      case .cxxCopyConstructor: return "cxxCopyConstructor"
      case .cxxMoveConstructor: return "cxxMoveConstructor"
      case .accessorGetter: return "accessorGetter"
      case .accessorSetter: return "accessorSetter"
      case .usingTypename: return "usingTypename"
      case .usingValue: return "usingValue"
      case .swiftAccessorWillSet: return "swiftAccessorWillSet"
      case .swiftAccessorDidSet: return "swiftAccessorDidSet"
      case .swiftAccessorAddressor: return "swiftAccessorAddressor"
      case .swiftAccessorMutableAddressor: return "swiftAccessorMutableAddressor"
      case .swiftExtensionOfStruct: return "swiftExtensionOfStruct"
      case .swiftExtensionOfClass: return "swiftExtensionOfClass"
      case .swiftExtensionOfEnum: return "swiftExtensionOfEnum"
      case .swiftExtensionOfProtocol: return "swiftExtensionOfProtocol"
      case .swiftPrefixOperator: return "swiftPrefixOperator"
      case .swiftPostfixOperator: return "swiftPostfixOperator"
      case .swiftInfixOperator: return "swiftInfixOperator"
      case .swiftSubscript: return "swiftSubscript"
      case .swiftAssociatedtype: return "swiftAssociatedtype"
      case .swiftGenericTypeParam: return "swiftGenericTypeParam"
      case .swiftAccessorRead: return "swiftAccessorRead"
      case .swiftAccessorModify: return "swiftAccessorModify"
      default: return "unknown SubKind \(rawValue)"
      }
    }
  }

  public struct Properties: OptionSet, Sendable, CustomStringConvertible {
    public let rawValue: UInt64

    public static let generic = Properties(INDEXSTORE_SYMBOL_PROPERTY_GENERIC)
    public static let templatePartialSpecialization = Properties(
      INDEXSTORE_SYMBOL_PROPERTY_TEMPLATE_PARTIAL_SPECIALIZATION
    )
    public static let templateSpecialization = Properties(INDEXSTORE_SYMBOL_PROPERTY_TEMPLATE_SPECIALIZATION)
    public static let unittest = Properties(INDEXSTORE_SYMBOL_PROPERTY_UNITTEST)
    public static let ibAnnotated = Properties(INDEXSTORE_SYMBOL_PROPERTY_IBANNOTATED)
    public static let ibOutletCollection = Properties(INDEXSTORE_SYMBOL_PROPERTY_IBOUTLETCOLLECTION)
    public static let gkInspectable = Properties(INDEXSTORE_SYMBOL_PROPERTY_GKINSPECTABLE)
    public static let local = Properties(INDEXSTORE_SYMBOL_PROPERTY_LOCAL)
    public static let protocolInterface = Properties(INDEXSTORE_SYMBOL_PROPERTY_PROTOCOL_INTERFACE)
    public static let swiftAsync = Properties(INDEXSTORE_SYMBOL_PROPERTY_SWIFT_ASYNC)

    public init(rawValue: UInt64) {
      self.rawValue = rawValue
    }

    @usableFromInline
    init(_ rawValue: indexstore_symbol_property_t) {
      self.rawValue = UInt64(rawValue.rawValue)
    }

    public var description: String {
      var components: [String] = []
      if self.contains(.generic) { components.append("generic") }
      if self.contains(.templatePartialSpecialization) { components.append("templatePartialSpecialization") }
      if self.contains(.templateSpecialization) { components.append("templateSpecialization") }
      if self.contains(.unittest) { components.append("unittest") }
      if self.contains(.ibAnnotated) { components.append("ibAnnotated") }
      if self.contains(.ibOutletCollection) { components.append("ibOutletCollection") }
      if self.contains(.gkInspectable) { components.append("gkInspectable") }
      if self.contains(.local) { components.append("local") }
      if self.contains(.protocolInterface) { components.append("protocolInterface") }
      if self.contains(.swiftAsync) { components.append("swiftAsync") }
      return components.joined(separator: ", ")
    }
  }

  @usableFromInline nonisolated(unsafe) let symbol: indexstore_symbol_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline @_lifetime(borrow symbol, borrow library)
  init(symbol: indexstore_symbol_t, library: borrowing IndexStoreLibrary) {
    self.symbol = symbol
    self.library = library
  }

  /// The language in which the symbol is defined.
  ///
  /// Note that eg. symbols defined in  C may be used in Swift source code, so this isn't necessarily the same as the
  /// language of the unit in which this symbol occurs.
  @inlinable
  public var language: IndexStoreLanguage {
    IndexStoreLanguage(library.api.symbol_get_language(symbol))
  }

  /// The kind of declaration this symbol represents.
  @inlinable
  public var kind: Kind {
    return Kind(library.api.symbol_get_kind(symbol))
  }

  /// Some symbol kinds are further differentiated into sub-kinds, if this is such a symbol, the sub-kind.
  @inlinable
  public var subKind: SubKind {
    return SubKind(library.api.symbol_get_subkind(symbol))
  }

  /// Set of properties associated with this symbol.
  @inlinable
  public var properties: Properties {
    return Properties(rawValue: library.api.symbol_get_properties(symbol))
  }

  /// The union of all roles with which the symbol occurs in the current record
  ///
  /// Ie. the result of iterating over all `IndexStoreOccurrence`s of this symbol in the record and collecting their
  /// roles.
  @inlinable
  public var roles: IndexStoreSymbolRoles {
    return IndexStoreSymbolRoles(rawValue: library.api.symbol_get_roles(symbol))
  }

  /// The roles with which this symbol is related to other symbol occurrences in the current record.
  ///
  /// Ie. the result of iterating over all `IndexStoreSymbolRelation`s of this symbol in the record and collecting
  /// their roles.
  @inlinable
  public var relatedRoles: IndexStoreSymbolRoles {
    return IndexStoreSymbolRoles(rawValue: library.api.symbol_get_related_roles(symbol))
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  /// A human-readable name that identifies this symbol. This does not have to be unique.
  @inlinable
  public var name: IndexStoreStringRef {
    @_lifetime(borrow self)
    get {
      let stringRef = IndexStoreStringRef(library.api.symbol_get_name(symbol))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  /// A USR that uniquely identifies this symbol.
  @inlinable
  public var usr: IndexStoreStringRef {
    @_lifetime(borrow self)
    get {
      let stringRef = IndexStoreStringRef(library.api.symbol_get_usr(symbol))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  /// The codegen name of this symbol, if it was recorded by clang by setting the `-index-record-codegen-name` command
  /// line option.
  @inlinable
  public var codegenName: IndexStoreStringRef {
    @_lifetime(borrow self)
    get {
      let stringRef = IndexStoreStringRef(library.api.symbol_get_codegen_name(symbol))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  var description: String {
    var kindString = kind.description
    if subKind != .none {
      kindString += ".\(subKind)"
    }
    if !properties.isEmpty {
      kindString += "(\(properties))"
    }

    return [
      name.string,
      kindString,
      usr.string,
      roles.description,
    ].joined(separator: " | ")
  }
}
