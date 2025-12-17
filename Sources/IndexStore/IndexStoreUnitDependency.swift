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

  /// Specifies the kind of dependency.
  @inlinable
  public var kind: Kind {
    Kind(library.api.unit_dependency_get_kind(dependency))
  }

  /// Whether the file of this dependency is in the SDK (system) or is part of the user's project.
  @inlinable
  public var isSystem: Bool {
    library.api.unit_dependency_is_system(dependency)
  }

  /// The path of the source file that represents the source file for a record dependency or that was used to generate a
  /// module in case of a unit dependency.
  @inlinable
  public var filePath: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_dependency_get_filepath(dependency))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  /// For Swift, the name of the module as part of which the dependency was compiled.
  @inlinable
  public var moduleName: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_dependency_get_modulename(dependency))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  /// The name of the record file in case of a record dependency and the name of a unit file in case of a unit
  /// dependency.
  @inlinable
  public var name: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_dependency_get_name(dependency))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }
}
