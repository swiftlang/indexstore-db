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

public struct SymbolProperty: OptionSet, Hashable, Sendable {
  public var rawValue: UInt64

  public static let generic: SymbolProperty = SymbolProperty(rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_GENERIC)
  public static let templatePartialSpecialization: SymbolProperty = SymbolProperty(
    rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_TEMPLATE_PARTIAL_SPECIALIZATION
  )
  public static let templateSpecialization: SymbolProperty = SymbolProperty(
    rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_TEMPLATE_SPECIALIZATION
  )
  public static let unitTest: SymbolProperty = SymbolProperty(rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_UNITTEST)
  public static let ibAnnotated: SymbolProperty = SymbolProperty(rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_IBANNOTATED)
  public static let ibOutletCollection: SymbolProperty = SymbolProperty(
    rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_IBOUTLETCOLLECTION
  )
  public static let gkInspectable: SymbolProperty = SymbolProperty(rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_GKINSPECTABLE)
  public static let local: SymbolProperty = SymbolProperty(rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_LOCAL)
  public static let protocolInterface: SymbolProperty = SymbolProperty(
    rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_PROTOCOL_INTERFACE
  )
  public static let swiftAsync: SymbolProperty = SymbolProperty(rawValue: INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ASYNC)

  public static let all: SymbolProperty = SymbolProperty(rawValue: ~0)

  public init(rawValue: UInt64) {
    self.rawValue = rawValue
  }

  internal init(rawValue: indexstoredb_symbol_property_t) {
    self.rawValue = UInt64(rawValue.rawValue)
  }
}

extension SymbolProperty: CustomStringConvertible {
  public var description: String {
    if self.isEmpty { return "[]" }

    var value = self
    var desc = ""
    if contains(.generic) {
      value.subtract(.generic)
      desc += "gen|"
    }
    if contains(.templatePartialSpecialization) {
      value.subtract(.templatePartialSpecialization)
      desc += "tps|"
    }
    if contains(.templateSpecialization) {
      value.subtract(.templateSpecialization)
      desc += "ts|"
    }
    if contains(.unitTest) {
      value.subtract(.unitTest)
      desc += "test|"
    }
    if contains(.ibAnnotated) {
      value.subtract(.ibAnnotated)
      desc += "ib|"
    }
    if contains(.ibOutletCollection) {
      value.subtract(.ibOutletCollection)
      desc += "ib_coll|"
    }
    if contains(.gkInspectable) {
      value.subtract(.gkInspectable)
      desc += "gki|"
    }
    if contains(.local) {
      value.subtract(.local)
      desc += "local|"
    }
    if contains(.protocolInterface) {
      value.subtract(.protocolInterface)
      desc += "protocol|"
    }
    if contains(.swiftAsync) {
      value.subtract(.swiftAsync)
      desc += "swift_async|"
    }

    if !value.isEmpty {
      desc += "<<unknown properties \(value.rawValue)>>|"
    }

    return "[\(desc.dropLast())]"
  }
}
