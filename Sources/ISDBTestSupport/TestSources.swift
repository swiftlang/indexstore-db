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

/// Test helper for looking up source location mappings and contents of source files in a project,
/// as well as managing modifications to the sources.
public final class TestSources {

  /// The root source directory.
  public var rootDirectory: URL

  /// The source contents.
  public let sourceCache: SourceFileCache = SourceFileCache()

  /// The map of known source locations.
  public var locations: [String: TestLocation]

  public init(rootDirectory: URL) throws {
    self.rootDirectory = rootDirectory
    self.locations = try scanLocations(rootDirectory: rootDirectory, sourceCache: sourceCache)
  }

  public struct ChangeSet {
    public var remove: [URL] = []
    public var rename: [(URL, URL)] = []
    public var write: [(URL, String)] = []

    public func isDirty(_ url: URL) -> Bool {
      return remove.contains(url)
        || rename.contains { $0 == url || $1 == url }
        || write.contains { $0.0 == url }
    }
  }

  public struct ChangeBuilder {
    public var changes: ChangeSet = ChangeSet()
    var seen: Set<URL> = []

    public mutating func write(_ content: String, to url: URL) {
      precondition(seen.insert(url).inserted, "multiple edits to same file")
      changes.write.append((url, content))
    }
    public mutating func remove(_ url: URL) {
      precondition(seen.insert(url).inserted, "multiple edits to same file")
      changes.remove.append(url)
    }
    public mutating func rename(from: URL, to: URL) {
      precondition(seen.insert(from).inserted && seen.insert(to).inserted, "multiple edits to same file")
      changes.rename.append((from, to))
    }
  }

  public func apply(_ changes: ChangeSet) throws {
    for (url, content) in changes.write {
      guard let data = content.data(using: .utf8) else {
        fatalError("failed to encode edited contents to utf8")
      }
      try data.write(to: url)
      sourceCache.set(url, to: content)
    }

    let fm = FileManager.default
    for (from, to) in changes.rename {
      try fm.moveItem(at: from, to: to)
      sourceCache.set(to, to: sourceCache.cache[from])
      sourceCache.set(from, to: nil)
    }

    for url in changes.remove {
      try fm.removeItem(at: url)
      sourceCache.set(url, to: nil)
    }

    // FIXME: update incrementally without rescanning everything.
    locations = try scanLocations(rootDirectory: rootDirectory, sourceCache: sourceCache)
  }

  /// Perform a set of edits (modifications, removals, renames) to the sources and return the
  /// change set. This modifies the files on disk.
  ///
  /// * parameters:
  ///   * block: Callback to collect the desired changes.
  /// * returns: The ChangeSet corresponding to these changes.
  /// * throws: Any file system errors seen while modifying the sources. If this happens, the state
  ///   of the source files is not defined.
  public func edit(_ block: (_ builder: inout ChangeBuilder) throws -> Void) throws -> ChangeSet {
    var builder = ChangeBuilder()
    try block(&builder)
    try apply(builder.changes)
    return builder.changes
  }
}
