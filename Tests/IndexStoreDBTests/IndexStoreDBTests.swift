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

import Foundation
import ISDBTibs
import IndexStoreDB
import XCTest

let isTSanEnabled: Bool = {
  if let value = ProcessInfo.processInfo.environment["INDEXSTOREDB_ENABLED_THREAD_SANITIZER"] {
    return value == "1" || value == "YES"
  }
  return false
}()

func checkThrows(
  _ expected: IndexStoreDBError,
  file: StaticString = #file,
  line: UInt = #line,
  _ body: () throws -> Void
) {
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
      _ = try IndexStoreDB(storePath: "/nope:", databasePath: "/nope:", library: nil)
    }

    checkThrows(.create("could not determine indexstore library")) {
      _ = try IndexStoreDB(storePath: "\(tmp)/idx", databasePath: "\(tmp)/db", library: nil)
    }
  }

  func testCreateIndexStoreAndDBDirs() {
    let toolchain = TibsToolchain.testDefault
    let libIndexStore = try! IndexStoreLibrary(dylibPath: toolchain.libIndexStore.path)

    // Normal - create the missing directories.
    XCTAssertNoThrow(try IndexStoreDB(storePath: tmp + "/store", databasePath: tmp + "/db", library: libIndexStore))

    // Readonly - do not create.
    checkThrows(.create("failed opening database")) {
      _ = try IndexStoreDB(
        storePath: tmp + "/store",
        databasePath: tmp + "/db-readonly",
        library: libIndexStore,
        readonly: true
      )
    }
    // Readonly - do not create.
    checkThrows(.create("index store path does not exist")) {
      _ = try IndexStoreDB(
        storePath: tmp + "/store-readonly",
        databasePath: tmp + "/db",
        library: libIndexStore,
        readonly: true
      )
    }
  }

  func testSymlinkedDBPaths() throws {
    // FIXME: This test seems to trigger a false-positive in TSan.
    try XCTSkipIf(isTSanEnabled, "skipping because TSan is enabled")

    let toolchain = TibsToolchain.testDefault
    let libIndexStore = try! IndexStoreLibrary(dylibPath: toolchain.libIndexStore.path)

    // This tests against a crash that was manifesting when creating separate `IndexStoreDB` instances against 2 DB paths
    // that resolve to the same underlying directory (e.g. they were symlinked).
    // It runs a number of iterations though it was not guaranteed that the issue would hit within a specific number
    // of iterations, only that at *some* point, if you let it run indefinitely, it could trigger.
    let iterations = 100

    let indexStorePath: String
    do {
      // Don't care about specific index data, just want an index store directory containing *something*.
      guard let ws = try staticTibsTestWorkspace(name: "SingleUnit") else { return }
      try ws.buildAndIndex()
      indexStorePath = ws.builder.indexstore.path
    }

    let fileMgr = FileManager.default
    let mainDBPath = tmp + "/db"
    let symlinkDBPath1 = tmp + "/db-link1"
    let symlinkDBPath2 = tmp + "/db-link2"
    try fileMgr.createDirectory(atPath: mainDBPath, withIntermediateDirectories: true, attributes: nil)
    try fileMgr.createSymbolicLink(atPath: symlinkDBPath1, withDestinationPath: mainDBPath)
    try fileMgr.createSymbolicLink(atPath: symlinkDBPath2, withDestinationPath: mainDBPath)

    for _ in 0..<iterations {
      DispatchQueue.concurrentPerform(iterations: 2) { idx in
        let dbPath = idx == 0 ? symlinkDBPath1 : symlinkDBPath2
        _ = try! IndexStoreDB(
          storePath: indexStorePath,
          databasePath: dbPath,
          library: libIndexStore,
          waitUntilDoneInitializing: true
        )
      }
    }
  }
}
