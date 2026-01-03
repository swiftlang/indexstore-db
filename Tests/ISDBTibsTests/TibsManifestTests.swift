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

final class TibsManifestTests: XCTestCase {
  func testParseSingle() throws {
    let serialized1 = """
      { "sources": ["a.swift"] }
      """

    let decoder = JSONDecoder()
    let manifest1 = try decoder.decode(TibsManifest.self, from: serialized1.data(using: .utf8)!)

    XCTAssertEqual(
      manifest1,
      TibsManifest(targets: [
        TibsManifest.Target(sources: ["a.swift"])
      ])
    )

    let serialized2 = """
      {
        "targets": [
          {
            "sources": ["a.swift"],
          },
        ]
      }
      """

    let manifest2 = try decoder.decode(TibsManifest.self, from: serialized2.data(using: .utf8)!)

    XCTAssertEqual(manifest1, manifest2)
  }

  func testWriteSingle() throws {
    let manifest = TibsManifest.Target(sources: ["a.swift"])
    let encoder = JSONEncoder()
    let serialized = try encoder.encode(manifest)
    XCTAssertEqual(
      String(data: serialized, encoding: .utf8),
      """
      {"sources":["a.swift"]}
      """
    )
  }

  func testRoundTrip() throws {
    let manifest = TibsManifest(targets: [
      TibsManifest.Target(
        name: "A",
        swiftFlags: ["-A", "-B"],
        clangFlags: ["-CA"],
        sources: ["a.swift", "b.c"],
        bridgingHeader: nil,
        dependencies: []
      ),
      TibsManifest.Target(name: "B", sources: ["b.swift"], dependencies: ["A"]),
    ])

    let encoder = JSONEncoder()
    let decoder = JSONDecoder()

    let after = try decoder.decode(TibsManifest.self, from: encoder.encode(manifest))

    XCTAssertEqual(after.targets[0].clangFlags, ["-CA"])
    XCTAssertEqual(after.targets[1].dependencies, ["A"])

    XCTAssertEqual(manifest, after)
  }
}
