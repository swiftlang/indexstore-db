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

fileprivate actor IndexStoreLibraryRegistry {
  static let shared = IndexStoreLibraryRegistry()

  var libraries: [URL: IndexStoreLibrary] = [:]

  func library(withDylibUrl dylibUrl: URL) throws -> IndexStoreLibrary {
    if let existingLibrary = libraries[dylibUrl] {
      return existingLibrary
    }
    let library = try IndexStoreLibrary(dylibPath: dylibUrl)
    libraries[dylibUrl] = library
    return library
  }
}

/// Represent a loaded `libIndexStore` dynamic library. This object hasn't opened a specific Index Store yet, it
/// represents the loaded library and the functions within it themselves.
public struct IndexStoreLibrary: Sendable {
  struct DlopenFailedError: Error, CustomStringConvertible {
    let path: URL
    let error: String

    var description: String {
      "dlopen '\(path)' failed: \(error)"
    }
  }

  @usableFromInline
  let api: indexstore_functions_t

  /// Open the `libIndexStore` dynamic library at the given path.
  ///
  /// Typically, this library is located in `usr/lib/libIndexStore.{dylib,so}` inside macOS or Linux toolchains or at
  /// `usr/bin/libIndexStore.dll` inside Windows toolchains.
  public static func at(dylibPath: URL) async throws -> IndexStoreLibrary {
    return try await IndexStoreLibraryRegistry.shared.library(withDylibUrl: dylibPath)
  }

  /// `dlopen` the `libIndexStore` dynamic library located at `dylibPath` and load all known functions from it
  fileprivate init(dylibPath: URL) throws {
    #if os(Windows)
    let dlopenModes: DLOpenFlags = []
    #else
    let dlopenModes: DLOpenFlags = [.lazy, .local, .first]
    #endif
    // We never dlclose the dylib. That way we do not have to track which types are still using this `IndexStoreLibrary`.
    // Since we only open dylibs by path ones (through `IndexStoreLibraryRegistry`), this seems fine because most
    // processes dealing with an index store will do so for the remainder of their execution time anyway.
    let dlHandle = try dylibPath.withUnsafeFileSystemRepresentation { filePath in
      try dlopen(String(cString: filePath!), mode: dlopenModes)
    }
    self.api = try indexstore_functions_t(dlHandle: dlHandle)
  }

  /// Use this library to open an Index Store at the given path, ie. create a handle that can read the Index Store’s
  /// contents. `path` should point to the directory that contains the `v5` directory. For example for SwiftPM packages,
  /// the path could be `.build/debug/index/store`
  public func indexStore(at path: URL) throws -> IndexStore {
    return try IndexStore(at: path, library: self)
  }
}
