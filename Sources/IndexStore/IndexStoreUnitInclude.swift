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

/// A `#include` (or equivalent like `#import`) directive that was processed while indexing a unit.
public struct IndexStoreUnitInclude: ~Escapable, Sendable {
  @usableFromInline nonisolated(unsafe) let include: indexstore_unit_include_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline @_lifetime(borrow library)
  init(include: indexstore_unit_include_t, library: borrowing IndexStoreLibrary) {
    self.include = include
    self.library = library
  }

  /// The path of the source file that contains the `#include` directive.
  @inlinable
  public var sourcePath: IndexStoreStringRef {
    @_lifetime(borrow self)
    get {
      let stringRef = IndexStoreStringRef(library.api.unit_include_get_source_path(include))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  /// The line in `sourcePath` at which the `#include` directive occurs.
  @inlinable
  public var line: Int {
    return Int(library.api.unit_include_get_source_line(include))
  }

  /// The path of the source file that is included.
  @inlinable
  public var targetPath: IndexStoreStringRef {
    @_lifetime(borrow self)
    get {
      let stringRef = IndexStoreStringRef(library.api.unit_include_get_target_path(include))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }
}
