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

import ISDBTestSupport
import IndexStoreDB
import XCTest

final class LocationScannerTests: XCTestCase {

  static let magicURL: URL = URL(fileURLWithPath: "/magic.swift")

  struct Loc: Equatable, Comparable {
    var url: URL
    var name: String
    var line: Int
    var utf8Column: Int
    var utf16Column: Int

    init(
      url: URL = LocationScannerTests.magicURL,
      _ name: String,
      _ line: Int,
      utf8Column: Int,
      utf16Column: Int
    ) {
      self.url = url
      self.name = name
      self.line = line
      self.utf8Column = utf8Column
      self.utf16Column = utf16Column
    }

    init(url: URL = LocationScannerTests.magicURL, _ name: String, _ line: Int, _ column: Int) {
      self.init(url: url, name, line, utf8Column: column, utf16Column: column)
    }

    init(_ name: String, _ loc: TestLocation) {
      self.init(
        url: loc.url,
        name,
        loc.line,
        utf8Column: loc.utf8Column,
        utf16Column: loc.utf16Column
      )
    }
    static func < (a: Loc, b: Loc) -> Bool {
      return (a.url.absoluteString, a.line, a.utf8Column, a.name) < (b.url.absoluteString, b.line, b.utf8Column, b.name)
    }
  }

  func scanString(_ str: String) throws -> [Loc] {
    var scanner = TestLocationScanner()
    try scanner.scan(str, url: LocationScannerTests.magicURL)
    return scanner.result.map { key, value in Loc(key, value) }.sorted()
  }

  func scanDir(_ dir: URL) throws -> [Loc] {
    return try scanLocations(rootDirectory: dir, sourceCache: SourceFileCache())
      .map { key, value in Loc(key, value) }.sorted()
  }

  func testSmall() throws {
    XCTAssertEqual(try scanString(""), [])
    XCTAssertEqual(try scanString("/"), [])
    XCTAssertEqual(try scanString("/*"), [])
    XCTAssertEqual(try scanString("/**"), [])
    XCTAssertEqual(try scanString("**/"), [])
    XCTAssertEqual(try scanString("/*/"), [])
    XCTAssertEqual(try scanString("/**/"), [Loc("", 1, 5)])
    XCTAssertEqual(try scanString("/*a*/*/*b*/"), [Loc("a", 1, 6), Loc("b", 1, 12)])
    XCTAssertEqual(try scanString("/**/ "), [Loc("", 1, 5)])
    XCTAssertEqual(try scanString(" /**/"), [Loc("", 1, 6)])
    XCTAssertEqual(try scanString("*/**/"), [Loc("", 1, 6)])
    XCTAssertEqual(try scanString(" /**/a"), [Loc("", 1, 6)])
  }

  func testName() throws {
    XCTAssertEqual(try scanString("/*a*/"), [Loc("a", 1, 6)])
    XCTAssertEqual(try scanString("/*abc*/"), [Loc("abc", 1, 8)])
    XCTAssertEqual(try scanString("/*a:b*/"), [Loc("a:b", 1, 8)])
  }

  func testDuplicate() throws {
    XCTAssertThrowsError(try scanString("/*a*//*a*/"))
    XCTAssertThrowsError(try scanString("/**//**/"))
  }

  func testNested() throws {
    XCTAssertThrowsError(try scanString("/*/**/*/"))
    XCTAssertThrowsError(try scanString("/* /**/*/"))
    XCTAssertThrowsError(try scanString("/*/**/ */"))
    XCTAssertThrowsError(try scanString("/*/* */*/"))
  }

  func testLocation() throws {
    XCTAssertEqual(try scanString("/*a*/"), [Loc("a", 1, 6)])
    XCTAssertEqual(try scanString("   /*a*/"), [Loc("a", 1, 9)])
    XCTAssertEqual(
      try scanString(
        """

        /*a*/
        """
      ),
      [Loc("a", 2, 6)]
    )
    XCTAssertEqual(
      try scanString(
        """


        /*a*/
        """
      ),
      [Loc("a", 3, 6)]
    )
    XCTAssertEqual(
      try scanString(
        """
        a
        b
        /*a*/
        """
      ),
      [Loc("a", 3, 6)]
    )
    XCTAssertEqual(
      try scanString(
        """
        a

        b /*a*/
        """
      ),
      [Loc("a", 3, 8)]
    )
    XCTAssertEqual(
      try scanString(
        """

        /*a*/

        """
      ),
      [Loc("a", 2, 6)]
    )
  }

  func testMultiple() throws {
    XCTAssertEqual(
      try scanString(
        """
        func /*f*/f() {
          /*g:call*/g(/*g:arg1*/1)
        }/*end*/
        """
      ),
      [
        Loc("f", 1, 11),
        Loc("g:call", 2, 13),
        Loc("g:arg1", 2, 25),
        Loc("end", 3, 9),
      ]
    )
  }

  func testLeft() throws {
    XCTAssertEqual(try scanString("/*a*/"), [Loc("a", 1, 6)])
    XCTAssertEqual(try scanString("/*<a*/"), [Loc("a", 1, 1)])

    XCTAssertEqual(
      try scanString("/*a*/foo/*<a:end*/"),
      [
        Loc("a", 1, 6),
        Loc("a:end", 1, 9),
      ]
    )

    XCTAssertThrowsError(try scanString("/*<a*//*a*/"))
  }

  func testDirectory() throws {
    let proj1 = XCTestCase.isdbInputsDirectory
      .appendingPathComponent("proj1", isDirectory: true)
    XCTAssertEqual(
      try scanDir(proj1),
      [
        Loc(url: proj1.appendingPathComponent("a.swift", isDirectory: false), "a:def", 1, 15),
        Loc(url: proj1.appendingPathComponent("a.swift", isDirectory: false), "b:call", 2, 13),
        Loc(url: proj1.appendingPathComponent("a.swift", isDirectory: false), "c:call", 3, 13),
        Loc(url: proj1.appendingPathComponent("b.swift", isDirectory: false), "b:def", 1, 15),
        Loc(url: proj1.appendingPathComponent("b.swift", isDirectory: false), "a:call", 2, 13),
        Loc(url: proj1.appendingPathComponent("rec/c.swift", isDirectory: false), "c", 1, 11),
      ]
    )
  }

  func testUnicode() throws {
    XCTAssertEqual(try scanString("😃/*a*/"), [Loc("a", 1, utf8Column: 10, utf16Column: 8)])
    XCTAssertEqual(try scanString("😃/*<a*/"), [Loc("a", 1, utf8Column: 5, utf16Column: 3)])
    XCTAssertEqual(try scanString("Ć/*a*/"), [Loc("a", 1, utf8Column: 8, utf16Column: 7)])
    XCTAssertEqual(try scanString("Ć/*<a*/"), [Loc("a", 1, utf8Column: 3, utf16Column: 2)])
    XCTAssertEqual(try scanString("👩‍👩‍👧‍👧/*a*/"), [Loc("a", 1, utf8Column: 31, utf16Column: 17)])
    XCTAssertEqual(try scanString("👩‍👩‍👧‍👧/*<a*/"), [Loc("a", 1, utf8Column: 26, utf16Column: 12)])
  }
}
