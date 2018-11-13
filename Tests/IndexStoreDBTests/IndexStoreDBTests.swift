//===--- IndexStoreDBTests.swift ------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import IndexStoreDB
import XCTest
import Foundation

func checkThrows(_ expected: IndexStoreDBError, file: StaticString = #file, line: UInt = #line, _ body: () throws -> ()) {
  do {
    try body()
    XCTFail("missing expected error \(expected)", file: file, line: line)

  } catch let error as IndexStoreDBError {

    switch (error, expected) {
    case (.create(let msg), .create(let expected)), (.loadIndexStore(let msg), .loadIndexStore(let expected)):
      XCTAssert(msg.hasPrefix(expected), "error \(error) does not match expected \(expected)", file: file, line: line)
    default:
      XCTFail("error \(error) does not match expected \(expected)", file: file, line: line)
    }
  } catch {
    XCTFail("error \(error) does not match expected \(expected)", file: file, line: line)
  }
}

final class IndexStoreDBTests: XCTestCase {

  var tmp: String = NSTemporaryDirectory()

  override func setUp() {
    tmp += "/indexstoredb_index_test\(getpid())"
  }

  override func tearDown() {
    _ = try? FileManager.default.removeItem(atPath: tmp)
  }

  func testErrors() {
    checkThrows(.create("failed creating directory")) {
      _ = try IndexStoreDB(storePath: "/nope", databasePath: "/nope", library: nil)
    }

    checkThrows(.create("could not determine indexstore library")) {
      _ = try IndexStoreDB(storePath: "\(tmp)/idx", databasePath: "\(tmp)/db", library: nil)
    }
  }

  static var allTests = [
    ("testErrors", testErrors),
    ]
}
