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

/// The occurrence of a symbol in a source file.
///
/// A symbol occurrence is a position (line:column) inside a source file at which an `IndexStoreSymbol` is defined,
/// referenced or occurs in some other way. The occurrence’s `role` determines the type of the occurrence.
///
/// A symbol may also have related symbols. For example if this occurrence is inside a function declaration, than the
/// occurrence will have the a `childOf` relation to that function's symbol.
public struct IndexStoreOccurrence: ~Escapable, Sendable {
  @usableFromInline nonisolated(unsafe) let occurrence: indexstore_occurrence_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline @_lifetime(borrow occurrence, borrow library)
  init(occurrence: indexstore_occurrence_t, library: borrowing IndexStoreLibrary) {
    self.occurrence = occurrence
    self.library = library
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  /// The symbol that is referenced by this occurrence.
  @inlinable
  public var symbol: IndexStoreSymbol {
    @_lifetime(borrow self)
    get {
      let symbol = IndexStoreSymbol(symbol: library.api.occurrence_get_symbol(occurrence)!, library: library)
      return _overrideLifetime(symbol, borrowing: self)
    }
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  /// The symbols that this occurrence is related to, such as the function declaration that this occurrence is a child
  /// of or the classes/protocols that are base types of a class definition.
  @inlinable
  public var relations: RelationsSequence {
    @_lifetime(borrow self)
    get {
      return RelationsSequence(self)
    }
  }

  /// The role identifies the types of this symbol occurrence, such as a definition, reference, call etc.
  ///
  /// A symbol occurrence may have multiple roles. For example a function call has both the `reference` and the `call`
  /// role.
  ///
  /// In addition to the occurrences's own roles, this also contains all roles of the occurrence's related symbols.
  @inlinable
  public var roles: IndexStoreSymbolRoles {
    IndexStoreSymbolRoles(rawValue: self.library.api.occurrence_get_roles(occurrence))
  }

  /// The position of this occurrence as a 1-based line, column pair. `column` identifies the UTF-8 byte offset of this
  /// symbol from the first character in the line.
  @inlinable
  public var position: (line: Int, column: Int) {
    var line: UInt32 = 0
    var column: UInt32 = 0
    self.library.api.occurrence_get_line_col(occurrence, &line, &column)

    return (Int(line), Int(column))
  }

  /// Specialized version of `IndexStoreSequence` that can have a lifetime dependence on a `IndexStoreOccurrence`.
  ///
  /// There currently doesn't seem to be a way to model this using the closure-taking `IndexStoreSequence`.
  public struct RelationsSequence: ~Escapable {
    @usableFromInline let producer: IndexStoreOccurrence

    @usableFromInline @_lifetime(borrow producer)
    init(_ producer: borrowing IndexStoreOccurrence) {
      self.producer = producer
    }

    /// Iterate through all elements in this sequence until `.stop` is returned from the closure or an error is thrown.
    @inlinable
    public func forEach<Error>(
      _ body: (IndexStoreSymbolRelation) throws(Error) -> IterationContinuationBehavior
    ) throws(Error) {
      var caughtError: Error? = nil
      _ = iterateWithClosureAsContextToCFunctionPointer { context, handleResult in
        producer.library.api.occurrence_relations_apply_f(producer.occurrence, context, handleResult)
      } handleResult: { (result: indexstore_symbol_relation_t?) in
        do {
          return try body(IndexStoreSymbolRelation(relation: result!, library: producer.library))
        } catch let error as Error {
          caughtError = error
          return .stop
        } catch {
          // Should never happen because `body` can only throw errors of the generic argument type `Error`.
          preconditionFailure("Caught error of unexpected type \(type(of: error))")
        }
      }
      if let caughtError {
        throw caughtError
      }
    }

    // Cannot re-use the implementations of `map` and `compactMap` through a protocol because of
    // https://github.com/swiftlang/swift/issues/85773.

    @inlinable
    public func map<Result, Error>(
      _ transform: (IndexStoreSymbolRelation) throws(Error) -> Result
    ) throws(Error) -> [Result] {
      return try self.compactMap { (value) throws(Error) in
        try transform(value)
      }
    }

    @inlinable
    public func compactMap<Result, Error>(
      _ transform: (IndexStoreSymbolRelation) throws(Error) -> Result?
    ) throws(Error) -> [Result] {
      var result: [Result] = []
      try self.forEach { (value) throws(Error) in
        if let transformed = try transform(value) {
          result.append(transformed)
        }
        return .continue
      }
      return result
    }
  }
}
