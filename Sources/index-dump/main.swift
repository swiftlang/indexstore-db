//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2026 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import ArgumentParser
import Foundation
import IndexStore

#if os(Windows)
  import WinSDK
#endif

@main
struct IndexDump: AsyncParsableCommand {
  static let configuration = CommandConfiguration(
    abstract: "Dumps the content of unit or record files from an IndexStore."
  )

  @Argument(help: "Name of the unit/record or a direct path to the file")
  var nameOrPath: String

  @Option(help: "Path to libIndexStore. Inferred if omitted.")
  var libIndexStore: String?

  @Option(help: "Path to the index store directory. Inferred if omitted.")
  var indexStore: String?

  @Option(help: "Explicitly set mode (unit/record).")
  var mode: Mode?

  enum Mode: String, ExpressibleByArgument {
    case unit, record
  }

  func run() async throws {
    let libURL = try explicitOrInferredLibPath()
    let storeURL = try explicitOrInferredStorePath()

    let lib = try await IndexStoreLibrary.at(dylibPath: libURL)
    let store = try lib.indexStore(at: storeURL)

    let determinedMode = try mode ?? inferMode(from: nameOrPath)
    let cleanName = URL(fileURLWithPath: nameOrPath).lastPathComponent

    switch determinedMode {
    case .unit:
      let unit = try store.unit(named: cleanName)
      print(unit)
    case .record:
      let record = try store.record(named: cleanName)
      print(record)
    }
  }

  // MARK: - Mode Inference

  private func inferMode(from path: String) throws -> Mode {
    let components = URL(fileURLWithPath: path).pathComponents
    if components.contains("units") { return .unit }
    if components.contains("records") { return .record }
    throw ValidationError("Could not infer mode from path. Please specify --mode explicitly.")
  }

  private func explicitOrInferredStorePath() throws -> URL {
    if let explicit = indexStore { return URL(fileURLWithPath: explicit) }

    var url = URL(fileURLWithPath: nameOrPath)
    if url.pathComponents.contains("v5") {
      while url.lastPathComponent != "v5" && url.pathComponents.count > 1 {
        url = url.deletingLastPathComponent()
      }
      return url.deletingLastPathComponent()
    }
    throw ValidationError("Could not infer store path. Please specify --index-store.")
  }

  private func explicitOrInferredLibPath() throws -> URL {
    if let explicit = libIndexStore { return URL(fileURLWithPath: explicit) }

    guard let libURL = LibIndexStoreProvider.inferLibPath() else {
      throw ValidationError(
        "Could not find 'swift' to infer toolchain path. Please specify --lib-index-store explicitly."
      )
    }
    return libURL
  }
}
