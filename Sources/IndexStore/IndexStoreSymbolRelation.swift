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

/// A relation of a symbol occurrence to another symbol.
public struct IndexStoreSymbolRelation: ~Escapable, Sendable {
  @usableFromInline nonisolated(unsafe) let relation: indexstore_symbol_relation_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline @_lifetime(borrow relation, borrow library)
  init(relation: indexstore_symbol_relation_t, library: borrowing IndexStoreLibrary) {
    self.relation = relation
    self.library = library
  }

  @inlinable
  public var roles: IndexStoreSymbolRoles {
    IndexStoreSymbolRoles(rawValue: self.library.api.symbol_relation_get_roles(relation))
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  @inlinable
  public var symbol: IndexStoreSymbol {
    @_lifetime(borrow self)
    get {
      let symbol = IndexStoreSymbol(symbol: library.api.symbol_relation_get_symbol(relation)!, library: library)
      return _overrideLifetime(symbol, borrowing: self)
    }
  }
}
