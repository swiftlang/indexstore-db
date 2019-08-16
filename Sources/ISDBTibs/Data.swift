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

import Foundation

extension Data {

  /// Writes the contents of the data to `url` if it is different than the existing contents, or if
  /// the URL does not exist.
  ///
  /// Checking if the contents have changed and writing the new contents are not done atomically,
  /// so there is no guarantee that there are no spurious writes if this API is used concurrently.
  ///
  /// * parameters:
  ///   * url: The location to write the data into.
  ///   * options: Options for writing the data. Default value is [].
  /// * returns: `true` if the data was written.
  @discardableResult
  public func writeIfChanged(to url: URL, options: Data.WritingOptions = []) throws -> Bool {
    let prev: Data?
    do {
      prev = try Data(contentsOf: url)
    } catch CocoaError.fileReadNoSuchFile {
      prev = nil
    } catch let error as NSError where error.code == POSIXError.ENOENT.rawValue {
      prev = nil
    }

    if prev != self {
      try write(to: url, options: options)
      return true
    } else {
      return false
    }
  }
}
