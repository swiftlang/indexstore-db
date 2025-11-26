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
public struct IndexStoreOccurrence: ~Escapable, Sendable {
  @usableFromInline nonisolated(unsafe) let occurrence: indexstore_occurrence_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline @_lifetime(borrow occurrence, borrow library)
  init(occurrence: indexstore_occurrence_t, library: borrowing IndexStoreLibrary) {
    self.occurrence = occurrence
    self.library = library
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  @inlinable
  public var symbol: IndexStoreSymbol {
    @_lifetime(borrow self)
    get {
      let symbol = IndexStoreSymbol(symbol: library.api.occurrence_get_symbol(occurrence)!, library: library)
      return _overrideLifetime(symbol, borrowing: self)
    }
  }

  // swift-format-ignore: UseSingleLinePropertyGetter, https://github.com/swiftlang/swift-format/issues/1102
  @inlinable
  public var relations: RelationsSequence {
    @_lifetime(borrow self)
    get {
      return RelationsSequence(self)
    }
  }

  @inlinable
  public var roles: IndexStoreSymbolRoles {
    IndexStoreSymbolRoles(rawValue: self.library.api.occurrence_get_roles(occurrence))
  }

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
