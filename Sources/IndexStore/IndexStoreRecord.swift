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

public final class IndexStoreRecord: Sendable {
  @usableFromInline nonisolated(unsafe) let recordReader: indexstore_record_reader_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline
  init(store: IndexStore, recordName: IndexStoreStringRef, library: IndexStoreLibrary) throws {
    self.library = library
    self.recordReader = try recordName.withCString { recordNameCString in
      return try library.capturingError { error in
        library.api.record_reader_create(store.store, recordNameCString, &error)
      }
    }
  }

  @inlinable
  deinit {
    library.api.record_reader_dispose(recordReader)
  }

  @inlinable
  public func symbols(matching filter: @escaping (IndexStoreSymbol) -> Bool) -> IndexStoreSequence<IndexStoreSymbol> {
    return IndexStoreSequence { body in
      var shouldStop: Bool = false
      typealias WrappedFilterType = (indexstore_symbol_t, UnsafeMutablePointer<Bool>) -> Bool
      let wrappedFilter: WrappedFilterType = { symbol, stop in
        if shouldStop {
          stop.pointee = true
          return false
        }
        return filter(IndexStoreSymbol(symbol: symbol, library: self.library))
      }
      return withoutActuallyEscaping(wrappedFilter) { wrappedFilter in
        withUnsafePointer(to: wrappedFilter) { wrappedFilterPointer in
          typealias WrappedBodyType = (indexstore_symbol_t) -> Void
          let wrappedBody: WrappedBodyType = { symbol in
            let iterationBehavior = body(IndexStoreSymbol(symbol: symbol, library: self.library))
            switch iterationBehavior {
            case .continue: break
            case .stop: shouldStop = true
            }
          }
          return withoutActuallyEscaping(wrappedBody) { wrappedBody in
            withUnsafePointer(to: wrappedBody) { bodyPointer in
              _ = self.library.api.record_reader_search_symbols_f(
                self.recordReader,
                UnsafeMutableRawPointer(mutating: wrappedFilterPointer),
                { wrappedFilterPointer, symbol, stop in
                  return wrappedFilterPointer!.assumingMemoryBound(to: (WrappedFilterType.self)).pointee(symbol!, stop!)
                },
                UnsafeMutableRawPointer(mutating: bodyPointer),
                { wrappedBodyPointer, symbol in
                  wrappedBodyPointer!.assumingMemoryBound(to: WrappedBodyType.self).pointee(symbol!)
                }
              )
            }
          }
        }
      }
    }
  }

  @inlinable
  public var symbols: IndexStoreSequence<IndexStoreSymbol> {
    return symbols(allowCached: true)
  }

  @inlinable
  public func symbols(allowCached: Bool) -> IndexStoreSequence<IndexStoreSymbol> {
    return IndexStoreSequence { body in
      _ = iterateWithClosureAsContextToCFunctionPointer { context, handleResult in
        self.library.api.record_reader_symbols_apply_f(
          self.recordReader,
          /*nocache*/!allowCached,
          UnsafeMutableRawPointer(mutating: context),
          handleResult
        )
      } handleResult: { (result: indexstore_symbol_t?) in
        return body(IndexStoreSymbol(symbol: result!, library: self.library))
      }
    }
  }

  @inlinable
  public var occurrences: IndexStoreSequence<IndexStoreOccurrence> {
    return IndexStoreSequence { body in
      _ = iterateWithClosureAsContextToCFunctionPointer { context, handleResult in
        self.library.api.record_reader_occurrences_apply_f(
          self.recordReader,
          UnsafeMutableRawPointer(mutating: context),
          handleResult
        )
      } handleResult: { (result: indexstore_occurrence_t?) in
        return body(IndexStoreOccurrence(occurrence: result!, library: self.library))
      }
    }
  }

  /// Return only occurrences in the given 1-based line range
  @inlinable
  public func occurrences(inLineRange lineRange: ClosedRange<Int>) -> IndexStoreSequence<IndexStoreOccurrence> {
    return IndexStoreSequence { body in
      _ = iterateWithClosureAsContextToCFunctionPointer { context, handleResult in
        self.library.api.record_reader_occurrences_in_line_range_apply_f(
          self.recordReader,
          /*line_start*/UInt32(lineRange.lowerBound),
          /*line_count*/UInt32((lineRange.upperBound - lineRange.lowerBound)),
          UnsafeMutableRawPointer(mutating: context),
          handleResult
        )
      } handleResult: { (result: indexstore_occurrence_t?) in
        return body(IndexStoreOccurrence(occurrence: result!, library: self.library))
      }
    }
  }

  // `record_reader_occurrences_of_symbols_apply_f` cannot be represented easily because we can't take an array of
  // `IndexStoreSymbol` as input to this function and has thus not been wrapped yet.
}
