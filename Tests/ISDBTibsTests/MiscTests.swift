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

import ISDBTibs
import Foundation
import XCTest

final class MiscTests: XCTestCase {
  func testDataWriteIfChanged() throws {
    let data1 = "1\n".data(using: .utf8)!
    let data2 = "2\n".data(using: .utf8)!

    let url = URL(fileURLWithPath: NSTemporaryDirectory())
      .appendingPathComponent("\(MiscTests.self).\(#function).txt", isDirectory: false)

    _ = try? FileManager.default.removeItem(at: url)
    XCTAssertTrue(try data1.writeIfChanged(to: url))
    XCTAssertFalse(try data1.writeIfChanged(to: url))
    XCTAssertTrue(try data2.writeIfChanged(to: url))
    XCTAssertFalse(try data2.writeIfChanged(to: url))
    try FileManager.default.removeItem(at: url)
  }
}
