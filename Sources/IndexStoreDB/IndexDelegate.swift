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

@_implementationOnly import CIndexStoreDB

/// Delegate for index events.
public protocol IndexDelegate: AnyObject {

  /// The index will process `count` unit files.
  func processingAddedPending(_ count: Int)

  /// The index finished processing `count` unit files.
  func processingCompleted(_ count: Int)
}

extension IndexDelegate {
  internal func handleEvent(_ event: indexstoredb_delegate_event_t) {
    let kind = indexstoredb_delegate_event_get_kind(event)
    switch kind {
    case INDEXSTOREDB_EVENT_PROCESSING_ADDED_PENDING:
      let count = indexstoredb_delegate_event_get_count(event)
      self.processingAddedPending(Int(count))
    case INDEXSTOREDB_EVENT_PROCESSING_COMPLETED:
      let count = indexstoredb_delegate_event_get_count(event)
      self.processingCompleted(Int(count))
    default:
      return
    }
  }
}
