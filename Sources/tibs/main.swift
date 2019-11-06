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
import ArgumentParser

struct RuntimeError: LocalizedError {
  var errorDescription: String?
}

struct Tibs: ParsableCommand {
  static var configuration = CommandConfiguration(
    subcommands: [Build.self, MergeDependencies.self],
    defaultSubcommand: Build.self)
}

extension Tibs {
  struct Build: ParsableCommand {
    @Argument()
    var projectDir: String

    func run() throws {
      let projectRoot = URL(fileURLWithPath: projectDir, isDirectory: true)

      let manifest: TibsManifest
      do {
        manifest = try TibsManifest.load(projectRoot: projectRoot)
      } catch {
        throw RuntimeError(errorDescription: "could not read manifest for '\(projectRoot.path)'")
      }

      let cwd = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)

      let toolchain = TibsToolchain(
        swiftc: URL(fileURLWithPath: "/usr/bin/swiftc"),
        clang: URL(fileURLWithPath: "/usr/bin/clang"),
        tibs: Bundle.main.bundleURL.appendingPathComponent("tibs", isDirectory: false))

      let builder: TibsBuilder
      do {
        builder = try TibsBuilder(manifest: manifest, sourceRoot: projectRoot, buildRoot: cwd, toolchain: toolchain)
      } catch {
        throw RuntimeError(errorDescription: "could not resolve project at '\(projectRoot.path)'")
      }

      do {
        try builder.writeBuildFiles()
      } catch {
        throw RuntimeError(errorDescription: "could not write build files")
      }
    }
  }
}

extension Tibs {
  struct MergeDependencies: ParsableCommand {
    static var configuration = CommandConfiguration(commandName: "swift-deps-merge")

    @Argument()
    var output: String

    @Argument()
    var files: [String]

    mutating func validate() throws {
      if files.isEmpty {
        throw ValidationError("No files specified")
      }
    }
    
    func run() throws {
      let allDeps = try files.flatMap { file -> [Substring] in
        guard let makefile = Makefile(path: URL(fileURLWithPath: file)) else {
          throw RuntimeError(errorDescription: "could not read dep file '\(file)'")
        }
        return makefile.outputs.flatMap { $0.deps }
      }

      let uniqueDeps = Set(allDeps).sorted()
      print("\(output) : \(uniqueDeps.joined(separator: " "))")
    }
  }
}

Tibs.main()
