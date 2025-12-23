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
import ISDBTestSupport
import ISDBTibs
import XCTest

final class TibsResolutionTests: XCTestCase {

  static let fakeToolchain: TibsToolchain = TibsToolchain(
    swiftc: URL(fileURLWithPath: "/swiftc"),
    clang: URL(fileURLWithPath: "/clang"),
    tibs: URL(fileURLWithPath: "/tibs"),
    ninja: URL(fileURLWithPath: "/ninja")
  )

  func testResolutionSingleSwiftModule() throws {
    let dir = projectDir("proj1")
    let m = try TibsManifest.load(projectRoot: dir)
    let tc = TibsResolutionTests.fakeToolchain
    let src = URL(fileURLWithPath: "/src", isDirectory: true)
    let build = URL(fileURLWithPath: "/build", isDirectory: true)
    let builder = try TibsBuilder(manifest: m, sourceRoot: src, buildRoot: build, toolchain: tc)

    XCTAssertEqual(1, builder.targets.count)
    guard let target = builder.targets.first else {
      return
    }
    XCTAssertEqual(target.name, "main")
    XCTAssertEqual(target.dependencies, [])
    XCTAssertEqual(target.clangTUs, [])
    XCTAssertNotNil(target.swiftModule)
    guard let module = target.swiftModule else {
      return
    }
    XCTAssertEqual(module.name, "main")
    XCTAssertEqual(module.emitModulePath, "main.swiftmodule")
    XCTAssertNil(module.emitHeaderPath)
    XCTAssertNil(module.bridgingHeader)
    XCTAssertEqual(module.moduleDeps, [])
    XCTAssertEqual(module.importPaths, [])
    XCTAssertEqual(module.extraArgs, [])
    XCTAssertEqual(
      module.sources,
      [
        src.appendingPathComponent("a.swift", isDirectory: false),
        src.appendingPathComponent("b.swift", isDirectory: false),
        src.appendingPathComponent("rec/c.swift", isDirectory: false),
      ]
    )
    #if os(macOS)
    XCTAssertNotNil(module.sdk)
    #else
    XCTAssertNil(module.sdk)
    #endif
  }

  func testResolutionMixedLangTarget() throws {
    let dir = projectDir("MixedLangTarget")
    let m = try TibsManifest.load(projectRoot: dir)
    let tc = TibsResolutionTests.fakeToolchain
    let src = URL(fileURLWithPath: "/src", isDirectory: true)
    let build = URL(fileURLWithPath: "/build", isDirectory: true)
    let builder = try TibsBuilder(manifest: m, sourceRoot: src, buildRoot: build, toolchain: tc)

    XCTAssertEqual(1, builder.targets.count)
    guard let target = builder.targets.first else {
      return
    }
    XCTAssertEqual(target.name, "main")
    XCTAssertEqual(target.dependencies, [])
    XCTAssertNotNil(target.swiftModule)
    guard let module = target.swiftModule else {
      return
    }
    XCTAssertEqual(module.name, "main")
    XCTAssertEqual(module.emitModulePath, "main.swiftmodule")
    XCTAssertEqual(module.emitHeaderPath, "main-Swift.h")
    XCTAssertEqual(
      module.bridgingHeader,
      src.appendingPathComponent("bridging-header.h", isDirectory: false)
    )
    XCTAssertEqual(module.moduleDeps, [])
    XCTAssertEqual(module.importPaths, [])
    XCTAssertEqual(module.extraArgs, ["-Xcc", "-Wno-objc-root-class"])
    XCTAssertEqual(
      module.sources,
      [
        src.appendingPathComponent("a.swift", isDirectory: false),
        src.appendingPathComponent("b.swift", isDirectory: false),
      ]
    )

    let clangTUs = target.clangTUs.sorted(by: { $0.outputPath < $1.outputPath })
    XCTAssertEqual(clangTUs.count, 4)
    if let tu = clangTUs.count >= 2 ? clangTUs[0] : nil {
      XCTAssertEqual(tu.source, src.appendingPathComponent("b.c", isDirectory: false))
      XCTAssertEqual(tu.outputPath, "main-b.c.o")
      XCTAssertEqual(tu.generatedHeaderDep, "main.swiftmodule")
      XCTAssertEqual(tu.importPaths, [".", "/src"])
      XCTAssertEqual(tu.extraArgs, ["-Wno-objc-root-class"])
    } else {
      XCTFail()
    }
    if let tu = clangTUs.count >= 2 ? clangTUs[1] : nil {
      XCTAssertEqual(tu.source, src.appendingPathComponent("c.m", isDirectory: false))
      XCTAssertEqual(tu.outputPath, "main-c.m.o")
      XCTAssertEqual(tu.generatedHeaderDep, "main.swiftmodule")
      XCTAssertEqual(tu.importPaths, [".", "/src"])
      XCTAssertEqual(tu.extraArgs, ["-Wno-objc-root-class"])
    } else {
      XCTFail()
    }
    if let tu = clangTUs.count >= 3 ? clangTUs[2] : nil {
      XCTAssertEqual(tu.source, src.appendingPathComponent("d.cpp", isDirectory: false))
      XCTAssertEqual(tu.outputPath, "main-d.cpp.o")
      XCTAssertEqual(tu.generatedHeaderDep, "main.swiftmodule")
      XCTAssertEqual(tu.importPaths, [".", "/src"])
      XCTAssertEqual(tu.extraArgs, ["-Wno-objc-root-class"])
    } else {
      XCTFail()
    }
    if let tu = clangTUs.count >= 4 ? clangTUs[3] : nil {
      XCTAssertEqual(tu.source, src.appendingPathComponent("e.mm", isDirectory: false))
      XCTAssertEqual(tu.outputPath, "main-e.mm.o")
      XCTAssertEqual(tu.generatedHeaderDep, "main.swiftmodule")
      XCTAssertEqual(tu.importPaths, [".", "/src"])
      XCTAssertEqual(tu.extraArgs, ["-Wno-objc-root-class"])
    } else {
      XCTFail()
    }
  }

  func testResolutionSwiftModules() throws {
    let dir = projectDir("SwiftModules")
    let m = try TibsManifest.load(projectRoot: dir)
    let tc = TibsResolutionTests.fakeToolchain
    let src = URL(fileURLWithPath: "/src", isDirectory: true)
    let build = URL(fileURLWithPath: "/build", isDirectory: true)
    let builder = try TibsBuilder(manifest: m, sourceRoot: src, buildRoot: build, toolchain: tc)

    XCTAssertEqual(3, builder.targets.count)
    if builder.targets.count != 3 { return }

    do {
      let target = builder.targets[0]
      XCTAssertEqual(target.name, "A")
      XCTAssertEqual(target.dependencies, [])
      XCTAssertNotNil(target.swiftModule)
      guard let module = target.swiftModule else {
        return
      }
      XCTAssertEqual(module.name, "A")
      XCTAssertEqual(module.emitModulePath, "A.swiftmodule")
      XCTAssertNil(module.emitHeaderPath)
      XCTAssertNil(module.bridgingHeader)
      XCTAssertEqual(module.moduleDeps, [])
      XCTAssertEqual(module.importPaths, [])
      XCTAssertEqual(module.extraArgs, [])
      XCTAssertEqual(
        module.sources,
        [
          src.appendingPathComponent("a.swift", isDirectory: false)
        ]
      )
      XCTAssertEqual(target.clangTUs, [])
    }

    do {
      let target = builder.targets[1]
      XCTAssertEqual(target.name, "B")
      XCTAssertEqual(target.dependencies, ["A"])
      XCTAssertNotNil(target.swiftModule)
      guard let module = target.swiftModule else {
        return
      }
      XCTAssertEqual(module.name, "B")
      XCTAssertEqual(module.emitModulePath, "B.swiftmodule")
      XCTAssertNil(module.emitHeaderPath)
      XCTAssertNil(module.bridgingHeader)
      XCTAssertEqual(module.moduleDeps, ["A.swiftmodule"])
      XCTAssertEqual(module.importPaths, ["."])
      XCTAssertEqual(module.extraArgs, [])
      XCTAssertEqual(
        module.sources,
        [
          src.appendingPathComponent("b.swift", isDirectory: false)
        ]
      )
      XCTAssertEqual(target.clangTUs, [])
    }

    do {
      let target = builder.targets[2]
      XCTAssertEqual(target.name, "C")
      XCTAssertEqual(target.dependencies, ["B"])
      XCTAssertNotNil(target.swiftModule)
      guard let module = target.swiftModule else {
        return
      }
      XCTAssertEqual(module.name, "C")
      XCTAssertEqual(module.emitModulePath, "C.swiftmodule")
      XCTAssertNil(module.emitHeaderPath)
      XCTAssertNil(module.bridgingHeader)
      XCTAssertEqual(module.moduleDeps, ["B.swiftmodule"])
      XCTAssertEqual(module.importPaths, ["."])
      XCTAssertEqual(module.extraArgs, [])
      XCTAssertEqual(
        module.sources,
        [
          src.appendingPathComponent("c.swift", isDirectory: false)
        ]
      )
      XCTAssertEqual(target.clangTUs, [])
    }
  }
}

func projectDir(_ name: String) -> URL {
  XCTestCase.isdbInputsDirectory.appendingPathComponent(name, isDirectory: true)
}
