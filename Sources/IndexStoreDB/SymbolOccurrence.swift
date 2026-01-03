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

public struct SymbolOccurrence: Equatable, Sendable {
  public var symbol: Symbol
  public var location: SymbolLocation
  public var roles: SymbolRole
  public var symbolProvider: SymbolProviderKind
  public var relations: [SymbolRelation]

  public init(
    symbol: Symbol,
    location: SymbolLocation,
    roles: SymbolRole,
    symbolProvider: SymbolProviderKind,
    relations: [SymbolRelation] = []
  ) {
    self.symbol = symbol
    self.location = location
    self.roles = roles
    self.symbolProvider = symbolProvider
    self.relations = relations
  }
}

extension SymbolOccurrence: Comparable {
  public static func < (a: SymbolOccurrence, b: SymbolOccurrence) -> Bool {
    // FIXME: incorporate relations
    return (a.location, a.roles, a.symbol) < (b.location, b.roles, b.symbol)
  }
}

extension SymbolRelation: Comparable {
  public static func < (a: SymbolRelation, b: SymbolRelation) -> Bool {
    (a.roles, a.symbol) < (b.roles, b.symbol)
  }
}

extension SymbolOccurrence: CustomStringConvertible {
  public var description: String {
    "\(location) | \(symbol) | \(roles)" + relations.flatMap { "\n\t\($0.roles) | \($0.symbol.usr)" }
  }
}

public struct SymbolRelation: Equatable, Sendable {
  public var symbol: Symbol
  public var roles: SymbolRole

  public init(symbol: Symbol, roles: SymbolRole) {
    self.symbol = symbol
    self.roles = roles
  }
}

// MARK: CIndexStoreDB conversions

extension SymbolOccurrence {

  /// Note: `value` is expected to be passed +1.
  internal init(_ value: indexstoredb_symbol_occurrence_t) {
    var relations: [SymbolRelation] = []
    indexstoredb_symbol_occurrence_relations(value) { relation in
      relations.append(SymbolRelation(relation))
      return true
    }

    self.init(
      symbol: Symbol(indexstoredb_symbol_occurrence_symbol(value)),
      location: SymbolLocation(indexstoredb_symbol_occurrence_location(value)),
      roles: SymbolRole(rawValue: indexstoredb_symbol_occurrence_roles(value)),
      symbolProvider: SymbolProviderKind(indexstoredb_symbol_occurrence_symbol_provider_kind(value)),
      relations: relations
    )
  }
}

extension SymbolRelation {
  internal init(_ value: indexstoredb_symbol_relation_t) {
    self.init(
      symbol: Symbol(indexstoredb_symbol_relation_get_symbol(value)),
      roles: SymbolRole(rawValue: indexstoredb_symbol_relation_get_roles(value))
    )
  }
}
