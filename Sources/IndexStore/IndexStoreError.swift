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

import Foundation
import IndexStoreDB_CIndexStoreDB

/// An error thrown by the libIndexStore dynamic library.
public final class IndexStoreError: Error, CustomStringConvertible, Sendable {
  private nonisolated(unsafe) let error: indexstore_error_t?
  private let library: IndexStoreLibrary

  fileprivate init(takingOwnershipOf error: indexstore_error_t?, library: IndexStoreLibrary) {
    self.error = error
    self.library = library
  }

  deinit {
    if let error {
      library.api.error_dispose(error)
    }
  }

  public var description: String {
    guard let descriptionPointer = library.api.error_get_description(error) else {
      return "Unknown error"
    }
    return String(cString: descriptionPointer)
  }
}

extension IndexStoreLibrary {
  func capturingError<T>(from body: (_ error: inout indexstore_error_t?) -> T?) throws -> T {
    var error: indexstore_error_t? = nil
    let result = body(&error)
    guard let result else {
      throw IndexStoreError(takingOwnershipOf: error, library: self)
    }
    return result
  }
}
