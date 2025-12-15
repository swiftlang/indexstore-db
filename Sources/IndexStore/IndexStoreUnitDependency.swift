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

public struct IndexStoreUnitDependency: ~Escapable, Sendable {
  public struct Kind: RawRepresentable, Sendable, Hashable {
    public let rawValue: UInt8

    public static let unit = Kind(INDEXSTORE_UNIT_DEPENDENCY_UNIT)
    public static let record = Kind(INDEXSTORE_UNIT_DEPENDENCY_RECORD)
    public static let file = Kind(INDEXSTORE_UNIT_DEPENDENCY_FILE)

    @inlinable
    public init(rawValue: UInt8) {
      self.rawValue = rawValue
    }

    @usableFromInline
    init(_ kind: indexstore_unit_dependency_kind_t) {
      self.rawValue = UInt8(kind.rawValue)
    }
  }

  @usableFromInline nonisolated(unsafe) let dependency: indexstore_unit_dependency_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline @_lifetime(borrow dependency, borrow library)
  init(dependency: indexstore_unit_dependency_t, library: borrowing IndexStoreLibrary) {
    self.dependency = dependency
    self.library = library
  }

  @inlinable
  public var kind: Kind {
    Kind(library.api.unit_dependency_get_kind(dependency))
  }

  @inlinable
  public var isSystem: Bool {
    library.api.unit_dependency_is_system(dependency)
  }

  @inlinable
  public var filePath: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_dependency_get_filepath(dependency))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var moduleName: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_dependency_get_modulename(dependency))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var name: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_dependency_get_name(dependency))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }
}
