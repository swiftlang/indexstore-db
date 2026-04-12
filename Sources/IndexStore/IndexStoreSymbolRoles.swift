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

/// A role with which a symbol may occur in a source file or how it is related to another symbol.
///
/// The roles are generally divided into normal roles and relationship roles. Normal roles specify in which role a
/// symbol occurs in a source file. For an occurrence's related symbols, the relationship roles specify how the
/// occurrence is related to the related symbol.
public struct IndexStoreSymbolRoles: OptionSet, Sendable, CustomStringConvertible {
  public let rawValue: UInt64

  public static let declaration = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_DECLARATION)
  public static let definition = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_DEFINITION)
  public static let reference = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REFERENCE)
  public static let read = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_READ)
  public static let write = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_WRITE)
  public static let call = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_CALL)
  public static let dynamic = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_DYNAMIC)
  public static let addressOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_ADDRESSOF)
  public static let implicit = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_IMPLICIT)
  public static let undefinition = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_UNDEFINITION)

  // Relationship roles
  public static let childOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_CHILDOF)
  public static let baseOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_BASEOF)
  public static let overrideOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_OVERRIDEOF)
  public static let receivedBy = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_RECEIVEDBY)
  public static let calledBy = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_CALLEDBY)
  public static let extendedBy = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_EXTENDEDBY)
  public static let accessorOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_ACCESSOROF)
  public static let containedBy = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_CONTAINEDBY)
  public static let ibTypeOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_IBTYPEOF)
  public static let specializationOf = IndexStoreSymbolRoles(INDEXSTORE_SYMBOL_ROLE_REL_SPECIALIZATIONOF)

  public var description: String {
    var roleStrings: [String] = []

    if self.contains(.declaration) { roleStrings.append("declaration") }
    if self.contains(.definition) { roleStrings.append("definition") }
    if self.contains(.reference) { roleStrings.append("reference") }
    if self.contains(.read) { roleStrings.append("read") }
    if self.contains(.write) { roleStrings.append("write") }
    if self.contains(.call) { roleStrings.append("call") }
    if self.contains(.dynamic) { roleStrings.append("dynamic") }
    if self.contains(.addressOf) { roleStrings.append("addressOf") }
    if self.contains(.implicit) { roleStrings.append("implicit") }
    if self.contains(.undefinition) { roleStrings.append("undefinition") }
    if self.contains(.childOf) { roleStrings.append("childOf") }
    if self.contains(.baseOf) { roleStrings.append("baseOf") }
    if self.contains(.overrideOf) { roleStrings.append("overrideOf") }
    if self.contains(.receivedBy) { roleStrings.append("receivedBy") }
    if self.contains(.calledBy) { roleStrings.append("calledBy") }
    if self.contains(.extendedBy) { roleStrings.append("extendedBy") }
    if self.contains(.accessorOf) { roleStrings.append("accessorOf") }
    if self.contains(.containedBy) { roleStrings.append("containedBy") }
    if self.contains(.ibTypeOf) { roleStrings.append("ibTypeOf") }
    if self.contains(.specializationOf) { roleStrings.append("specializationOf") }

    return roleStrings.joined(separator: ", ")
  }

  @inlinable
  public init(rawValue: UInt64) {
    self.rawValue = rawValue
  }

  @usableFromInline
  init(_ rawValue: indexstore_symbol_role_t) {
    self.rawValue = UInt64(rawValue.rawValue)
  }
}
