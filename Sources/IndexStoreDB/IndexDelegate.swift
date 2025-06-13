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

@_implementationOnly import IndexStoreDB_CIndexStoreDB

public struct StoreUnitInfo: Sendable {
  public let mainFilePath: String
  public let unitName: String
}

/// Delegate for index events.
public protocol IndexDelegate: AnyObject {

  /// The index will process `count` unit files.
  func processingAddedPending(_ count: Int)

  /// The index finished processing `count` unit files.
  func processingCompleted(_ count: Int)

  /// Notification about out-of-date unit.
  /// - Parameters:
  ///   - outOfDateModTime: number of nanoseconds since clock's epoch.
  ///   - triggerHintFile: file path that was determined as out-of-date.
  ///   - triggerHintDescription: full description of the out-of-date trigger.
  ///   - synchronous: whether the event needs to be handled synchronously.
  func unitIsOutOfDate(
    _ unitInfo: StoreUnitInfo,
    outOfDateModTime: UInt64,
    triggerHintFile: String,
    triggerHintDescription: String,
    synchronous: Bool
  )
}

extension IndexDelegate {
  public func unitIsOutOfDate(
    _ unitInfo: StoreUnitInfo,
    outOfDateModTime: UInt64,
    triggerHintFile: String,
    triggerHintDescription: String,
    synchronous: Bool
  ) {
  }
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
    case INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE:
      let c_unitInfo = indexstoredb_delegate_event_get_outofdate_unit_info(event)!
      let unitInfo = StoreUnitInfo(
        mainFilePath: String(cString: indexstoredb_unit_info_main_file_path(c_unitInfo)),
        unitName: String(cString: indexstoredb_unit_info_unit_name(c_unitInfo))
      )
      self.unitIsOutOfDate(
        unitInfo,
        outOfDateModTime: indexstoredb_delegate_event_get_outofdate_modtime(event),
        triggerHintFile: String(cString: indexstoredb_delegate_event_get_outofdate_trigger_original_file(event)!),
        triggerHintDescription: String(cString: indexstoredb_delegate_event_get_outofdate_trigger_description(event)!),
        synchronous: indexstoredb_delegate_event_get_outofdate_is_synchronous(event)
      )
    default:
      return
    }
  }
}
