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

import Foundation

@_implementationOnly import IndexStoreDB_CIndexStoreDB

public enum IndexStoreDBError: Error, Sendable {
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

// Note: this cannot conform to CustomStringConvertible, since it is a typealias
// of an UnsafeMutableRawPointer.
extension indexstoredb_error_t {
  var description: String {
    return String(cString: indexstoredb_error_get_description(self))
  }
}
