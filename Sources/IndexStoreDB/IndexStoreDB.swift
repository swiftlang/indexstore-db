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

/// IndexStoreDB index.
public final class IndexStoreDB {

  let delegate: IndexDelegate?
  let impl: indexstoredb_index_t

  /// Create or open an IndexStoreDB at the givin `databasePath`.
  ///
  /// * Parameters:
  ///   * storePath: Path to the index store.
  ///   * databasePath: Path to the index database (or where it will be created).
  ///   * library: The index store library to use.
  ///   * wait: If `true`, wait for the database to be populated from the
  ///     (current) contents of the index store at `storePath` before returning.
  ///   * readonly: If `true`, read an existing database, but do not create or modify.
  ///   * listenToUnitEvents: Only `true` is supported outside unit tests. Setting to `false`
  ///     disables reading or updating from the index store unless `pollForUnitChangesAndWait()`
  ///     is called.
  public init(
    storePath: String,
    databasePath: String,
    library: IndexStoreLibrary?,
    delegate: IndexDelegate? = nil,
    waitUntilDoneInitializing wait: Bool = false,
    readonly: Bool = false,
    listenToUnitEvents: Bool = true
  ) throws {
    self.delegate = delegate

    let libProviderFunc = { (cpath: UnsafePointer<Int8>) -> indexstoredb_indexstore_library_t? in
      return library?.library
    }

    let delegateFunc = { [weak delegate] (event: indexstoredb_delegate_event_t) in
      guard let delegate = delegate else { return }
      let kind = indexstoredb_delegate_event_get_kind(event)
      switch kind {
      case INDEXSTOREDB_EVENT_PROCESSING_ADDED_PENDING:
        let count = indexstoredb_delegate_event_get_count(event)
        delegate.processingAddedPending(Int(count))
      case INDEXSTOREDB_EVENT_PROCESSING_COMPLETED:
        let count = indexstoredb_delegate_event_get_count(event)
        delegate.processingCompleted(Int(count))
      default:
        return
      }
    }

    var error: indexstoredb_error_t? = nil
    guard let index = indexstoredb_index_create(storePath, databasePath, libProviderFunc, delegateFunc, wait, readonly, listenToUnitEvents, &error) else {
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

  @discardableResult
  public func forEachMainFileContainingFile(path: String, body: @escaping (String) -> Bool) -> Bool {
    return indexstoredb_index_main_files_containing_file(impl, path) { mainFile in
      body(String(cString: mainFile))
    }
  }

  public func mainFilesContainingFile(path: String) -> [String] {
    var result: [String] = []
    forEachMainFileContainingFile(path: path) { mainFile in
      result.append(mainFile)
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
