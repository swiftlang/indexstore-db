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

/// A builder object for scanning source files for TestLocations specified in /*inline comments*/.
///
/// The scanner searches source files for inline comments /*foo*/ and builds up a mapping from name
/// (the contents of the inline comment) to the location in the source file that it was found.
///
/// For example:
///
/// ```
/// var scanner = TestLocationScanner()
/// scanner.scan("""
///   func /*foo:def*/foo() {}
///   """, url: myURL)
/// scanner.result == ["foo:def": TestLocation(url: myURL, line: 1, column: 17)]
/// ```
///
/// ## Special Syntax
///
/// If the location starts with `<', it will be the location of the start of the
/// comment instead of the end. E.g.
///
/// ```
/// /*a*/   // column 6
/// /*<b*/  // column 1
/// ```
public struct TestLocationScanner {

  /// The result of the scan (so far), mapping name to test location.
  public var result: [String: TestLocation] = [:]

  public init() {}

  public enum Error: Swift.Error {
    case noSuchFileOrDirectory(URL)

    /// The sources contained a `/*/*nested*/*/` inline comment, which is not supported.
    case nestedComment(TestLocation)

    /// The same test location name was used in multiple places.
    case duplicateKey(String, TestLocation, TestLocation)
  }

  public mutating func scan(_ str: String, url: URL) throws {
    if str.count < 4 {
      return
    }

    enum State {
      /// Outside any comment.
      case normal(prev: Character)

      /// Inside a comment. The payload contains the previous character and the index of the first
      /// character after the '*' (i.e. the start of the comment body).
      ///
      ///       bodyStart
      ///       |
      ///     /*XXX*/
      ///       ^^^
      case comment(bodyStart: String.Index, prev: Character)
    }

    var state = State.normal(prev: "_")
    var i = str.startIndex
    var line = 1
    var lineStart = i

    while i != str.endIndex {
      let c = str[i]

      switch (state, c) {
      case (.normal("/"), "*"):
        state = .comment(bodyStart: str.index(after: i), prev: "_")
      case (.normal(_), _):
        state = .normal(prev: c)

      case (.comment(let start, "*"), "/"):
        let nameStart: String.Index
        let locIndex: String.Index
        if str[start] == "<" {
          nameStart = str.index(after: start)
          locIndex = str.index(start, offsetBy: -2)  // subtract '/' and '*'
        } else {
          nameStart = start
          locIndex = str.index(after: i)  // after trailing '/'
        }

        let name = String(str[nameStart..<str.index(before: i)])

        let loc = TestLocation(
          url: url,
          line: line,
          utf8Column: 1 + str.utf8.distance(from: lineStart, to: locIndex),
          utf16Column: 1 + str.utf16.distance(from: lineStart, to: locIndex)
        )

        if let prevLoc = result.updateValue(loc, forKey: name) {
          throw Error.duplicateKey(name, prevLoc, loc)
        }

        state = .normal(prev: "_")

      case (.comment(_, "/"), "*"):
        throw Error.nestedComment(
          TestLocation(
            url: url,
            line: line,
            utf8Column: 1 + str.utf8.distance(from: lineStart, to: i),
            utf16Column: 1 + str.utf16.distance(from: lineStart, to: i)
          )
        )

      case (.comment(let start, _), _):
        state = .comment(bodyStart: start, prev: c)
      }

      i = str.index(after: i)

      if c == "\n" {
        line += 1
        lineStart = i
      }
    }
  }

  public mutating func scan(file: URL, sourceCache: SourceFileCache) throws {
    let content = try sourceCache.get(file)
    try scan(content, url: file)
  }

  public mutating func scan(rootDirectory: URL, sourceCache: SourceFileCache) throws {
    let fm = FileManager.default

    guard let generator = fm.enumerator(at: rootDirectory, includingPropertiesForKeys: []) else {
      throw Error.noSuchFileOrDirectory(rootDirectory)
    }

    while let url = generator.nextObject() as? URL {
      if isSourceFileExtension(url.pathExtension) {
        try scan(file: url, sourceCache: sourceCache)
      }
    }
  }
}

/// Scans `rootDirectory` for test locations, returning a mapping from name to location.
///
/// See TestLocationScanner.
public func scanLocations(
  rootDirectory: URL,
  sourceCache: SourceFileCache
) throws -> [String: TestLocation] {
  var scanner = TestLocationScanner()
  try scanner.scan(rootDirectory: rootDirectory, sourceCache: sourceCache)
  return scanner.result
}

func isSourceFileExtension(_ ext: String) -> Bool {
  switch ext {
  case "swift", "c", "cpp", "m", "mm", "h", "hpp":
    return true
  default:
    return false
  }
}
