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

public import IndexStoreDB_CIndexStoreDB

/// A role with which a symbol may occur in a source file or how it is related to another symbol.
///
/// The roles are generally divided into normal roles and relationship roles. Normal roles specify in which role a
/// symbol occurs in a source file. For an occurrence's related symbols, the relationship roles specify how the
/// occurrence is related to the related symbol.
public struct IndexStoreSymbolRoles: OptionSet, Sendable {
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

  @inlinable
  public init(rawValue: UInt64) {
    self.rawValue = rawValue
  }

  @usableFromInline
  init(_ rawValue: indexstore_symbol_role_t) {
    self.rawValue = UInt64(rawValue.rawValue)
  }
}
