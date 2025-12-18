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

public struct SymbolRole: OptionSet, Hashable, Sendable {

  public var rawValue: UInt64

  // MARK: Primary roles, from indexstore
  public static let declaration: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_DECLARATION)
  public static let definition: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_DEFINITION)
  public static let reference: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REFERENCE)
  public static let read: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_READ)
  public static let write: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_WRITE)
  public static let call: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_CALL)
  public static let `dynamic`: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_DYNAMIC)
  public static let addressOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_ADDRESSOF)
  public static let implicit: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_IMPLICIT)

  // MARK: Relation roles, from indexstore
  public static let childOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_CHILDOF)
  public static let baseOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_BASEOF)
  public static let overrideOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_OVERRIDEOF)
  public static let receivedBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_RECEIVEDBY)
  public static let calledBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_CALLEDBY)
  public static let extendedBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_EXTENDEDBY)
  public static let accessorOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_ACCESSOROF)
  public static let containedBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_CONTAINEDBY)
  public static let ibTypeOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_IBTYPEOF)
  public static let specializationOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_SPECIALIZATIONOF)

  // MARK: Additional IndexStoreDB index roles

  public static let canonical: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_CANONICAL)

  public static let all: SymbolRole = SymbolRole(rawValue: ~0)

  public init(rawValue: UInt64) {
    self.rawValue = rawValue
  }

  internal init(rawValue: indexstoredb_symbol_role_t) {
    self.rawValue = UInt64(rawValue.rawValue)
  }
}

extension SymbolRole: Comparable {
  public static func < (a: SymbolRole, b: SymbolRole) -> Bool {
    return a.rawValue < b.rawValue
  }
}

extension SymbolRole: CustomStringConvertible {
  public var description: String {
    if self.isEmpty { return "[]" }

    var value = self
    var desc = ""
    if contains(.declaration) {
      value.subtract(.declaration)
      desc += "decl|"
    }
    if contains(.definition) {
      value.subtract(.definition)
      desc += "def|"
    }
    if contains(.reference) {
      value.subtract(.reference)
      desc += "ref|"
    }
    if contains(.read) {
      value.subtract(.read)
      desc += "read|"
    }
    if contains(.write) {
      value.subtract(.write)
      desc += "write|"
    }
    if contains(.call) {
      value.subtract(.call)
      desc += "call|"
    }
    if contains(.`dynamic`) {
      value.subtract(.`dynamic`)
      desc += "dyn|"
    }
    if contains(.addressOf) {
      value.subtract(.addressOf)
      desc += "addrOf|"
    }
    if contains(.implicit) {
      value.subtract(.implicit)
      desc += "implicit|"
    }
    if contains(.childOf) {
      value.subtract(.childOf)
      desc += "childOf|"
    }
    if contains(.baseOf) {
      value.subtract(.baseOf)
      desc += "baseOf|"
    }
    if contains(.overrideOf) {
      value.subtract(.overrideOf)
      desc += "overrideOf|"
    }
    if contains(.receivedBy) {
      value.subtract(.receivedBy)
      desc += "recBy|"
    }
    if contains(.calledBy) {
      value.subtract(.calledBy)
      desc += "calledBy|"
    }
    if contains(.extendedBy) {
      value.subtract(.extendedBy)
      desc += "extendedBy|"
    }
    if contains(.accessorOf) {
      value.subtract(.accessorOf)
      desc += "accOf|"
    }
    if contains(.containedBy) {
      value.subtract(.containedBy)
      desc += "contBy|"
    }
    if contains(.ibTypeOf) {
      value.subtract(.ibTypeOf)
      desc += "ibTypeOf|"
    }
    if contains(.specializationOf) {
      value.subtract(.specializationOf)
      desc += "specializationOf|"
    }
    if contains(.canonical) {
      value.subtract(.canonical)
      desc += "canon|"
    }

    if !value.isEmpty {
      desc += "<<unknown roles \(value.rawValue)>>"
    }

    return "[\(desc.dropLast())]"
  }
}
