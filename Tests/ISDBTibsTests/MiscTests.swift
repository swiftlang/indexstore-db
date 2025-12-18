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

  func testMakefile() throws {
    typealias Outputs = [(target: Substring, deps: [Substring])]
    func outputsEqual(actual: Outputs, expected: Outputs) -> Bool {
      return actual.count == expected.count && zip(actual, expected).allSatisfy(==)
    }

    let makefiles: [String: Outputs] = [
      /*simple*/
      "target: dep1 dep2 dep3": [
        (target: "target", deps: ["dep1", "dep2", "dep3"])
      ],
      /*newlines*/
      "target1: dep1 \ntarget2: dep1 dep2 dep3": [
        (target: "target1", deps: ["dep1"]),
        (target: "target2", deps: ["dep1", "dep2", "dep3"]),
      ],
      /*nodeps*/
      "target: ": [
        (target: "target", deps: [])
      ],
      /*spaces*/
      "target: Dep\\ with\\ spaces": [
        (target: "target", deps: ["Dep\\ with\\ spaces"])
      ],
      "target\\ with\\ spaces: Dep": [
        (target: "target\\ with\\ spaces", deps: ["Dep"])
      ],
      /*paths*/
      "target: Dep/with\\slashes": [
        (target: "target", deps: ["Dep/with\\slashes"])
      ],
    ]

    for (makefile, exp) in makefiles {
      guard let parsed = Makefile(contents: makefile) else {
        XCTFail("Could not parse: \(makefile)")
        return
      }
      XCTAssertTrue(
        outputsEqual(actual: parsed.outputs, expected: exp),
        "Makefile parse did not match!\nExpected: \(exp)\nAcutal: \(parsed.outputs)"
      )
    }
  }
}
