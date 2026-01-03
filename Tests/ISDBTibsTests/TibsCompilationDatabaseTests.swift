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

final class TibsCompilationDatabaseTests: XCTestCase {
  static let fakeToolchain: TibsToolchain = TibsToolchain(
    swiftc: URL(fileURLWithPath: "/swiftc"),
    clang: URL(fileURLWithPath: "/clang"),
    tibs: URL(fileURLWithPath: "/tibs"),
    ninja: URL(fileURLWithPath: "/ninja")
  )

  typealias Command = JSONCompilationDatabase.Command

  public func testCompilationDatabaseSwiftModule() throws {
    let dir = projectDir("SwiftModules")
    let m = try TibsManifest.load(projectRoot: dir)
    let tc = TibsCompilationDatabaseTests.fakeToolchain
    let src = URL(fileURLWithPath: "/src", isDirectory: true)
    let build = URL(fileURLWithPath: "/build", isDirectory: true)
    let builder = try TibsBuilder(manifest: m, sourceRoot: src, buildRoot: build, toolchain: tc)

    let sdkargs = TibsBuilder.defaultSDKPath.map { ["-sdk", $0] } ?? []

    let expected = JSONCompilationDatabase(commands: [
      Command(
        directory: "/build",
        file: "/src/a.swift",
        arguments: [
          "/swiftc", "/src/a.swift",
          "-module-name", "A",
          "-index-store-path", "/build/index", "-index-ignore-system-modules",
          "-output-file-map", "A-output-file-map.json",
          "-emit-module", "-emit-module-path", "A.swiftmodule",
          "-emit-dependencies",
          "-pch-output-dir", "pch",
          "-module-cache-path", "ModuleCache",
          "-c",
        ] + sdkargs + [
          "-working-directory", "/build",
        ]
      ),
      Command(
        directory: "/build",
        file: "/src/b.swift",
        arguments: [
          "/swiftc", "/src/b.swift",
          "-I", ".",
          "-module-name", "B",
          "-index-store-path", "/build/index", "-index-ignore-system-modules",
          "-output-file-map", "B-output-file-map.json",
          "-emit-module", "-emit-module-path", "B.swiftmodule",
          "-emit-dependencies",
          "-pch-output-dir", "pch",
          "-module-cache-path", "ModuleCache",
          "-c",
        ] + sdkargs + [
          "-working-directory", "/build",
        ]
      ),
      Command(
        directory: "/build",
        file: "/src/c.swift",
        arguments: [
          "/swiftc", "/src/c.swift",
          "-I", ".", "-module-name", "C",
          "-index-store-path", "/build/index", "-index-ignore-system-modules",
          "-output-file-map", "C-output-file-map.json",
          "-emit-module", "-emit-module-path", "C.swiftmodule",
          "-emit-dependencies",
          "-pch-output-dir", "pch",
          "-module-cache-path", "ModuleCache",
          "-c",
        ] + sdkargs + [
          "-working-directory", "/build",
        ]
      ),
    ])

    XCTAssertEqual(builder.compilationDatabase, expected)
  }

  public func testCompilationDatabaseMixedLangTarget() throws {
    let dir = projectDir("MixedLangTarget")
    let m = try TibsManifest.load(projectRoot: dir)
    let tc = TibsCompilationDatabaseTests.fakeToolchain
    let src = URL(fileURLWithPath: "/src", isDirectory: true)
    let build = URL(fileURLWithPath: "/build", isDirectory: true)
    let builder = try TibsBuilder(manifest: m, sourceRoot: src, buildRoot: build, toolchain: tc)

    let sdkargs = TibsBuilder.defaultSDKPath.map { ["-sdk", $0] } ?? []

    let swiftArgs =
      [
        "/swiftc", "/src/a.swift", "/src/b.swift",
        "-module-name", "main",
        "-index-store-path", "/build/index", "-index-ignore-system-modules",
        "-output-file-map", "main-output-file-map.json",
        "-emit-module", "-emit-module-path",
        "main.swiftmodule", "-emit-dependencies",
        "-pch-output-dir", "pch",
        "-module-cache-path", "ModuleCache",
        "-c",
        "-emit-objc-header", "-emit-objc-header-path", "main-Swift.h",
        "-import-objc-header", "/src/bridging-header.h",
      ] + sdkargs + [
        "-Xcc", "-Wno-objc-root-class",
        "-working-directory", "/build",
      ]

    let clangArgs = { (src: String) -> [String] in
      return [
        "/clang", "-fsyntax-only", "/src/\(src)",
        "-I", ".", "-I", "/src",
        "-index-store-path", "index", "-index-ignore-system-symbols",
        "-fmodules", "-fmodules-cache-path=ModuleCache",
        "-MMD", "-MF", "main-\(src).o.d",
        "-o", "main-\(src).o",
        "-Wno-objc-root-class",
      ]
    }

    let expected = JSONCompilationDatabase(commands: [
      Command(directory: "/build", file: "/src/a.swift", arguments: swiftArgs),
      Command(directory: "/build", file: "/src/b.swift", arguments: swiftArgs),
      Command(directory: "/build", file: "/src/b.c", arguments: clangArgs("b.c")),
      Command(directory: "/build", file: "/src/c.m", arguments: clangArgs("c.m")),
      Command(directory: "/build", file: "/src/d.cpp", arguments: clangArgs("d.cpp")),
      Command(directory: "/build", file: "/src/e.mm", arguments: clangArgs("e.mm")),
    ])

    XCTAssertEqual(builder.compilationDatabase, expected)
  }
}
