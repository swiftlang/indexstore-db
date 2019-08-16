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

/// Reads and caches file contents by URL.
///
/// Use `cache.get(url)` to read a file, or get its cached contents. The contents can be overridden
/// or removed from the cache by calling `cache.set(url, to: "new contents")`
public final class SourceFileCache {
  var cache: [URL: String] = [:]

  public init(_ cache: [URL: String] = [:]) {
    self.cache = cache
  }

  /// Read the contents of `file`, or retrieve them from the cache if available.
  ///
  /// * parameter file: The file to read.
  /// * returns: The file contents as a String.
  /// * throws: If there are any errors reading the file.
  public func get(_ file: URL) throws -> String {
    if let content = cache[file] {
      return content
    }
    let content = try String(contentsOfFile: file.path, encoding: .utf8)
    cache[file] = content
    return content
  }

  /// Set the cached contents of `file` to `content`.
  ///
  /// * parameters
  ///   * file: The file to read.
  ///   * content: The new file content.
  public func set(_ file: URL, to content: String?) {
    cache[file] = content
  }
}
