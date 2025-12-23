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

extension FileHandle: TextOutputStream {
  public func write(_ string: String) {
    guard let data = string.data(using: .utf8) else {
      fatalError("failed to get data from string '\(string)'")
    }
    self.write(data)
  }
}

var stderr = FileHandle.standardError

func swiftDepsMerge(output: String, _ files: [String]) {
  var allDeps: Set<Substring> = []

  for file in files {
    guard let makefile = Makefile(path: URL(fileURLWithPath: file)) else {
      print("error: could not read dep file '\(file)'", to: &stderr)
      exit(1)
    }

    let allOutputs = makefile.outputs.flatMap { $0.deps }
    allDeps.formUnion(allOutputs)
  }

  print("\(output) : \(allDeps.sorted().joined(separator: " "))")
}

func main(arguments: [String]) {

  if arguments.count < 2 {
    print("usage: tibs <project-dir>", to: &stderr)
    exit(1)
  }

  if arguments[1] == "swift-deps-merge" {
    if arguments.count < 4 {
      print("usage: tibs swift-deps-merge <output> <deps1.d> [...]", to: &stderr)
      exit(1)
    }
    swiftDepsMerge(output: arguments[2], Array(arguments.dropFirst(3)))
    return
  }

  let projectDir = URL(fileURLWithPath: arguments.last!, isDirectory: true)

  let manifest: TibsManifest
  do {
    manifest = try TibsManifest.load(projectRoot: projectDir)
  } catch {
    print("error: could not read manifest for '\(projectDir.path)': \(error)", to: &stderr)
    exit(1)
  }

  let cwd = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)

  let toolchain = TibsToolchain(
    swiftc: URL(fileURLWithPath: "/usr/bin/swiftc"),
    clang: URL(fileURLWithPath: "/usr/bin/clang"),
    tibs: Bundle.main.bundleURL.appendingPathComponent("tibs", isDirectory: false)
  )

  let builder: TibsBuilder
  do {
    builder = try TibsBuilder(manifest: manifest, sourceRoot: projectDir, buildRoot: cwd, toolchain: toolchain)
  } catch {
    print("error: could not resolve project at '\(projectDir.path)': \(error)", to: &stderr)
    exit(1)
  }

  do {
    try builder.writeBuildFiles()
  } catch {
    print("error: could not write build files: \(error)", to: &stderr)
    exit(1)
  }
}

main(arguments: CommandLine.arguments)
