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

/// An entire index store, ie. a directory containing the unit and record files.
public final class IndexStore: Sendable {
  @usableFromInline nonisolated(unsafe) let store: indexstore_t
  @usableFromInline let library: IndexStoreLibrary

  init(at path: String, library: IndexStoreLibrary) throws {
    self.library = library
    store = try path.withCString { path in
      try library.capturingError { error in
        library.api.store_create(path, &error)
      }
    }
  }

  @inlinable
  deinit {
    library.api.store_dispose(store)
  }

  @inlinable
  public func unitNames(sorted: Bool) -> IndexStoreSequence<IndexStoreStringRef> {
    return IndexStoreSequence { body in
      _ = iterateWithClosureAsContextToCFunctionPointer { context, handleResult in
        self.library.api.store_units_apply_f(self.store, sorted ? 1 : 0, context, handleResult)
      } handleResult: { result in
        return body(IndexStoreStringRef(result))
      }
    }
  }

  @inlinable
  public func unit(named unitName: IndexStoreStringRef) throws -> IndexStoreUnit {
    return try IndexStoreUnit(store: self, unitName: unitName, library: self.library)
  }

  @inlinable
  public func record(named recordName: IndexStoreStringRef) throws -> IndexStoreRecord {
    return try IndexStoreRecord(store: self, recordName: recordName, library: self.library)
  }
}
