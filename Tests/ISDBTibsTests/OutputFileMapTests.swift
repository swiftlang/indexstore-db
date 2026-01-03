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
import ISDBTibs
import XCTest

final class OutputFileMapTests: XCTestCase {
  func testInsertOrder() {
    var ofm = OutputFileMap()
    ofm["a"] = OutputFileMap.Entry(swiftmodule: "A")
    ofm["b"] = OutputFileMap.Entry(swiftmodule: "B")
    ofm["c"] = OutputFileMap.Entry(swiftmodule: "C")

    XCTAssertEqual(
      Array(ofm.values),
      [
        OutputFileMap.Entry(swiftmodule: "A"),
        OutputFileMap.Entry(swiftmodule: "B"),
        OutputFileMap.Entry(swiftmodule: "C"),
      ]
    )

    ofm["a"] = ofm["c"]!
    ofm["a"]!.swiftdoc = "D"

    XCTAssertEqual(
      Array(ofm.values),
      [
        OutputFileMap.Entry(swiftmodule: "C", swiftdoc: "D"),
        OutputFileMap.Entry(swiftmodule: "B"),
        OutputFileMap.Entry(swiftmodule: "C"),
      ]
    )
  }
}
