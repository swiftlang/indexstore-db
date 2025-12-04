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

public import Foundation
public import IndexStoreDB_CIndexStoreDB

public final class IndexStoreUnit: Sendable {
  @usableFromInline nonisolated(unsafe) let unitReader: indexstore_unit_reader_t
  @usableFromInline let library: IndexStoreLibrary

  @usableFromInline
  init(store: IndexStore, unitName: IndexStoreStringRef, library: IndexStoreLibrary) throws {
    self.library = library
    self.unitReader = try unitName.withCString { unitNameCString in
      return try library.capturingError { error in
        library.api.unit_reader_create(store.store, unitNameCString, &error)
      }
    }
  }

  @inlinable
  public var isSystemUnit: Bool {
    return library.api.unit_reader_is_system_unit(unitReader)
  }

  @inlinable
  public var isModuleUnit: Bool {
    return library.api.unit_reader_is_module_unit(unitReader)
  }

  @inlinable
  public var isDebugCompilation: Bool {
    return library.api.unit_reader_is_debug_compilation(unitReader)
  }

  @inlinable
  public var hasMainFile: Bool {
    return library.api.unit_reader_has_main_file(unitReader)
  }

  @inlinable
  public var providerIdentifier: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_provider_identifier(unitReader))
      // _overrideLifetime is needed here and below because of https://github.com/swiftlang/swift/issues/85765.
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var providerVersion: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_provider_version(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var modificationDate: Date {
    var seconds: Int64 = 0
    var nanoseconds: Int64 = 0
    library.api.unit_reader_get_modification_time(unitReader, &seconds, &nanoseconds)
    return Date(timeIntervalSince1970: Double(nanoseconds) / 1_000_000_000 + Double(seconds))
  }

  @inlinable
  public var mainFile: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_main_file(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var moduleName: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_module_name(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var workingDirectory: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_working_dir(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var outputFile: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_output_file(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var sysrootPath: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_sysroot_path(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var target: IndexStoreStringRef {
    @_lifetime(borrow self)
    borrowing get {
      let stringRef = IndexStoreStringRef(library.api.unit_reader_get_target(unitReader))
      return _overrideLifetime(stringRef, borrowing: self)
    }
  }

  @inlinable
  public var dependencies: IndexStoreSequence<IndexStoreUnitDependency> {
    return IndexStoreSequence { body in
      _ = iterateWithClosureAsContextToCFunctionPointer { context, handleResult in
        self.library.api.unit_reader_dependencies_apply_f(self.unitReader, context, handleResult)
      } handleResult: { result in
        return body(IndexStoreUnitDependency(dependency: result!, library: self.library))
      }
    }
  }
}
