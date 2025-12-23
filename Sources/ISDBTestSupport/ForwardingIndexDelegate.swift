//===----------------------------------------------------------------------===//
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

import Dispatch
import IndexStoreDB

/// A wrapper that forwards to another delegate that can be mutated at runtime.
final class ForwardingIndexDelegate: IndexDelegate {
  let queue: DispatchQueue = DispatchQueue(label: "ForwardingIndexDelegate.mutex")
  private var _delegate: IndexDelegate? = nil

  var delegate: IndexDelegate? {
    get { queue.sync { _delegate } }
    set { queue.sync { _delegate = newValue } }
  }

  func processingAddedPending(_ count: Int) {
    delegate?.processingAddedPending(count)
  }

  func processingCompleted(_ count: Int) {
    delegate?.processingCompleted(count)
  }

  func unitIsOutOfDate(
    _ unitInfo: StoreUnitInfo,
    outOfDateModTime: UInt64,
    triggerHintFile: String,
    triggerHintDescription: String,
    synchronous: Bool
  ) {
    delegate?.unitIsOutOfDate(
      unitInfo,
      outOfDateModTime: outOfDateModTime,
      triggerHintFile: triggerHintFile,
      triggerHintDescription: triggerHintDescription,
      synchronous: synchronous
    )
  }
}
