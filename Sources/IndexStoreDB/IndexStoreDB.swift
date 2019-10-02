//===--- IndexStoreDB.swift -----------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import CIndexStoreDB
import protocol Foundation.LocalizedError

public enum IndexStoreDBError: Error {
  case create(String)
  case loadIndexStore(String)
}

extension IndexStoreDBError: LocalizedError {
  public var errorDescription: String? {
    switch self {
    case .create(let msg):
      return "indexstoredb_index_create error: \(msg)"
    case .loadIndexStore(let msg):
      return "indexstoredb_load_indexstore_library error: \(msg)"
    }
  }
}

/// IndexStoreDB index.
public final class IndexStoreDB {

  let impl: indexstoredb_index_t

  public init(
    storePath: String,
    databasePath: String,
    library: IndexStoreLibrary?,
    readonly: Bool = false,
    listenToUnitEvents: Bool = true
  ) throws {

    let libProviderFunc = { (cpath: UnsafePointer<Int8>) -> indexstoredb_indexstore_library_t? in
      return library?.library
    }

    var error: indexstoredb_error_t? = nil
    guard let index = indexstoredb_index_create(storePath, databasePath, libProviderFunc, readonly, listenToUnitEvents, &error) else {
      defer { indexstoredb_error_dispose(error) }
      throw IndexStoreDBError.create(error?.description ?? "unknown")
    }

    impl = index
  }

  deinit {
    indexstoredb_release(impl)
  }

  /// *For Testing* Poll for any changes to units and wait until they have been registered.
  public func pollForUnitChangesAndWait() {
    indexstoredb_index_poll_for_unit_changes_and_wait(impl)
  }

  @discardableResult
  public func forEachSymbolOccurrence(byUSR usr: String, roles: SymbolRole, _ body: @escaping (SymbolOccurrence) -> Bool) -> Bool {
    return indexstoredb_index_symbol_occurrences_by_usr(impl, usr, roles.rawValue) { occur in
      return body(SymbolOccurrence(occur))
    }
  }

  public func occurrences(ofUSR usr: String, roles: SymbolRole) -> [SymbolOccurrence] {
    var result: [SymbolOccurrence] = []
    forEachSymbolOccurrence(byUSR: usr, roles: roles) { occur in
      result.append(occur)
      return true
    }
    return result
  }

  @discardableResult
  public func forEachRelatedSymbolOccurrence(byUSR usr: String, roles: SymbolRole, _ body: @escaping (SymbolOccurrence) -> Bool) -> Bool {
    return indexstoredb_index_related_symbol_occurrences_by_usr(impl, usr, roles.rawValue) { occur in
      return body(SymbolOccurrence(occur))
    }
  }

  public func occurrences(relatedToUSR usr: String, roles: SymbolRole) -> [SymbolOccurrence] {
    var result: [SymbolOccurrence] = []
    forEachRelatedSymbolOccurrence(byUSR: usr, roles: roles) { occur in
      result.append(occur)
      return true
    }
    return result
  }

  @discardableResult public func forEachCanonicalSymbolOccurrence(byName: String, body: @escaping (SymbolOccurrence) -> Bool) -> Bool {
    return indexstoredb_index_canonical_symbol_occurences_by_name(impl, byName) { occur in
      return body(SymbolOccurrence(occur))
    }
  }

  public func canonicalOccurrences(ofName name: String) -> [SymbolOccurrence] {
    var result: [SymbolOccurrence] = []
    forEachCanonicalSymbolOccurrence(byName: name) { occur in
      result.append(occur)
      return true
    }
    return result
  }

  @discardableResult public func forEachCanonicalSymbolOccurrence(
    containing pattern: String,
    anchorStart: Bool,
    anchorEnd: Bool,
    subsequence: Bool,
    ignoreCase: Bool,
    body: @escaping (SymbolOccurrence) -> Bool
  ) -> Bool {
    return indexstoredb_index_canonical_symbol_occurences_containing_pattern(
      impl,
      pattern,
      anchorStart,
      anchorEnd,
      subsequence,
      ignoreCase
    ) { occur in
      body(SymbolOccurrence(occur))
    }
  }

  public func canonicalOccurrences(
    containing pattern: String,
    anchorStart: Bool,
    anchorEnd: Bool,
    subsequence: Bool,
    ignoreCase: Bool
  ) -> [SymbolOccurrence] {
    var result: [SymbolOccurrence] = []
    forEachCanonicalSymbolOccurrence(
      containing: pattern,
      anchorStart: anchorStart,
      anchorEnd: anchorEnd,
      subsequence: subsequence,
      ignoreCase: ignoreCase)
    { occur in
      result.append(occur)
      return true
    }
    return result
  }
}

public protocol IndexStoreLibraryProvider {
  func library(forStorePath: String) -> IndexStoreLibrary?
}

public class IndexStoreLibrary {
  let library: indexstoredb_indexstore_library_t

  public init(dylibPath: String) throws {
    var error: indexstoredb_error_t? = nil
    guard let lib = indexstoredb_load_indexstore_library(dylibPath, &error) else {
      defer { indexstoredb_error_dispose(error) }
      throw IndexStoreDBError.loadIndexStore(error?.description ?? "unknown")
    }

    self.library = lib
  }

  deinit {
    indexstoredb_release(library)
  }
}

// Note: this cannot conform to CustomStringConvertible, since it is a typealias
// of an UnsafeMutableRawPointer.
extension indexstoredb_error_t {
  var description: String {
    return String(cString: indexstoredb_error_get_description(self))
  }
}
