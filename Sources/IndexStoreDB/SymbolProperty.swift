//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

@_implementationOnly import IndexStoreDB_CIndexStoreDB

public struct SymbolProperty: Sendable, Hashable, CustomStringConvertible {
  public let rawValue: UInt64

  public static let generic: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_GENERIC)
  public static let templatePartialSpecialization: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_TEMPLATE_PARTIAL_SPECIALIZATION
  )
  public static let templateSpecialization: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_TEMPLATE_SPECIALIZATION
  )
  public static let unitTest: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_UNITTEST)
  public static let ibAnnotated: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_IBANNOTATED)
  public static let ibOutletCollection: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_IBOUTLETCOLLECTION)
  public static let gkInspectable: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_GKINSPECTABLE)
  public static let local: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_LOCAL)
  public static let protocolInterface: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_PROTOCOL_INTERFACE)
  public static let swiftAsync: SymbolProperty = SymbolProperty(INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ASYNC)

  public static let swiftAccessControlLessThanFilePrivate: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ACCESSCONTROL_LESSTHANFILEPRIVATE
  )
  public static let swiftAccessControlFileprivate: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ACCESSCONTROL_FILEPRIVATE
  )
  public static let swiftAccessControlInternal: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ACCESSCONTROL_INTERNAL
  )
  public static let swiftAccessControlPackage: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ACCESSCONTROL_PACKAGE
  )
  public static let swiftAccessControlSpi: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ACCESSCONTROL_SPI
  )
  public static let swiftAccessControlPublic: SymbolProperty = SymbolProperty(
    INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ACCESSCONTROL_PUBLIC
  )

  /// The bits that encode the Swift access control level of a symbol.
  ///
  /// Access control levels are not single-bit flags: some levels (`internal`, `spi`, `public`) share bits with others,
  /// so testing for a specific level requires an exact match against this mask rather than a bitwise-and.
  internal static let swiftAccessControlBitmask: UInt64 =
    SymbolProperty.swiftAccessControlLessThanFilePrivate.rawValue
    | SymbolProperty.swiftAccessControlFileprivate.rawValue
    | SymbolProperty.swiftAccessControlInternal.rawValue
    | SymbolProperty.swiftAccessControlPackage.rawValue
    | SymbolProperty.swiftAccessControlSpi.rawValue
    | SymbolProperty.swiftAccessControlPublic.rawValue

  public init(rawValue: UInt64) {
    self.rawValue = rawValue
  }

  /// Create the union of the given properties.
  public init(_ properties: SymbolProperty...) {
    self.rawValue = properties.reduce(0) { $0 | $1.rawValue }
  }

  internal init(_ rawValue: indexstoredb_symbol_property_t) {
    self.rawValue = UInt64(rawValue.rawValue)
  }

  public func contains(_ property: SymbolProperty) -> Bool {
    let propertyAccessControl = property.rawValue & Self.swiftAccessControlBitmask
    if propertyAccessControl != 0 && (self.rawValue & Self.swiftAccessControlBitmask) != propertyAccessControl {
      return false
    }
    let propertyOther = property.rawValue & ~Self.swiftAccessControlBitmask
    return (self.rawValue & propertyOther) == propertyOther
  }

  /// The individual properties that this symbol carries.
  ///
  /// Each entry is equal to one of the static properties on this type or carries the remaining bits which aren't known
  /// to represent one of the static properties.
  public var components: [SymbolProperty] {
    let singleBitComponents: [SymbolProperty] = [
      .generic,
      .templatePartialSpecialization,
      .templateSpecialization,
      .unitTest,
      .ibAnnotated,
      .ibOutletCollection,
      .gkInspectable,
      .local,
      .protocolInterface,
      .swiftAsync,
    ]
    var remainingRawValue = self.rawValue
    var result: [SymbolProperty] = []
    for component in singleBitComponents {
      if remainingRawValue & component.rawValue != 0 {
        result.append(component)
        remainingRawValue &= ~component.rawValue
      }
    }

    let swiftAccessControlValue = remainingRawValue & Self.swiftAccessControlBitmask
    if swiftAccessControlValue != 0 {
      result.append(SymbolProperty(rawValue: swiftAccessControlValue))
      remainingRawValue &= ~Self.swiftAccessControlBitmask
    }
    if remainingRawValue != 0 {
      result.append(SymbolProperty(rawValue: remainingRawValue))
    }

    return result
  }

  public var description: String {
    if rawValue == 0 { return "[]" }
    let parts = components.map { component -> String in
      switch component {
      case .generic: return "gen"
      case .templatePartialSpecialization: return "tps"
      case .templateSpecialization: return "ts"
      case .unitTest: return "test"
      case .ibAnnotated: return "ib"
      case .ibOutletCollection: return "ib_coll"
      case .gkInspectable: return "gki"
      case .local: return "local"
      case .protocolInterface: return "protocol"
      case .swiftAsync: return "swift_async"
      case .swiftAccessControlLessThanFilePrivate: return "less_than_fileprivate"
      case .swiftAccessControlFileprivate: return "fileprivate"
      case .swiftAccessControlInternal: return "internal"
      case .swiftAccessControlPackage: return "package"
      case .swiftAccessControlSpi: return "spi"
      case .swiftAccessControlPublic: return "public"
      default: return "<<unknown properties \(component.rawValue)>>"
      }
    }
    return "[\(parts.joined(separator: "|"))]"
  }
}
