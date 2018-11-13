//===--- IndexStoreDB.swift -----------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
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
    readonly: Bool = false
  ) throws {

    let libProviderFunc = { (cpath: UnsafePointer<Int8>) -> indexstoredb_indexstore_library_t? in
      return library?.library
    }

    var error: indexstoredb_error_t? = nil
    guard let index = indexstoredb_index_create(storePath, databasePath, libProviderFunc, readonly, &error) else {
      defer { indexstoredb_error_dispose(error) }
      throw IndexStoreDBError.create(error?.description ?? "unknown")
    }

    impl = index
  }

  deinit {
    indexstoredb_release(impl)
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
}

public struct SymbolRole: OptionSet {

  public var rawValue: UInt64

  // MARK: Primary roles, from indexstore
  public static let declaration: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_DECLARATION)
  public static let definition: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_DEFINITION)
  public static let reference: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REFERENCE)
  public static let read: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_READ)
  public static let write: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_WRITE)
  public static let call: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_CALL)
  public static let `dynamic`: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_DYNAMIC)
  public static let addressOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_ADDRESSOF)
  public static let implicit: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_IMPLICIT)

  // MARK: Relation roles, from indexstore
  public static let childOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_CHILDOF)
  public static let baseOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_BASEOF)
  public static let overrideOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_OVERRIDEOF)
  public static let receivedBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_RECEIVEDBY)
  public static let calledBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_CALLEDBY)
  public static let extendedBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_EXTENDEDBY)
  public static let accessorOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_ACCESSOROF)
  public static let containedBy: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_CONTAINEDBY)
  public static let ibTypeOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_IBTYPEOF)
  public static let specializationOf: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_REL_SPECIALIZATIONOF)

  // MARK: Additional IndexStoreDB index roles
  public static let canonical: SymbolRole = SymbolRole(rawValue: INDEXSTOREDB_SYMBOL_ROLE_CANONICAL)

  public init(rawValue: UInt64) {
    self.rawValue = rawValue
  }

  public init(rawValue: indexstoredb_symbol_role_t) {
    self.rawValue = UInt64(rawValue.rawValue)
  }
}

public final class Symbol {

  let value: indexstoredb_symbol_t

  public lazy var usr: String = String(cString: indexstoredb_symbol_usr(value))
  public lazy var name: String = String(cString: indexstoredb_symbol_name(value))

  init(_ value: indexstoredb_symbol_t) {
    self.value = value
  }

  deinit {
    indexstoredb_release(value)
  }
}

public struct SymbolLocation {
  public var path: String
  public var isSystem: Bool
  public var line: Int
  public var utf8Column: Int

  public init(path: String, isSystem: Bool = false, line: Int, utf8Column: Int) {
    self.path = path
    self.isSystem = isSystem
    self.line = line
    self.utf8Column = utf8Column
  }

  public init(_ loc: indexstoredb_symbol_location_t) {
    path = String(cString: indexstoredb_symbol_location_path(loc))
    isSystem = indexstoredb_symbol_location_is_system(loc)
    line = Int(indexstoredb_symbol_location_line(loc))
    utf8Column = Int(indexstoredb_symbol_location_column_utf8(loc))
  }
}

public final class SymbolOccurrence {

  let value: indexstoredb_symbol_occurrence_t

  public lazy var symbol: Symbol = Symbol(indexstoredb_symbol_occurrence_symbol(value))
  public lazy var roles: SymbolRole = SymbolRole(rawValue: indexstoredb_symbol_occurrence_roles(value))
  public lazy var location: SymbolLocation = SymbolLocation(indexstoredb_symbol_occurrence_location(value))

  init(_ value: indexstoredb_symbol_occurrence_t) {
    self.value = value
  }

  deinit {
    indexstoredb_release(value)
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

extension indexstoredb_error_t: CustomStringConvertible {
  public var description: String {
    return String(cString: indexstoredb_error_get_description(self))
  }
}
