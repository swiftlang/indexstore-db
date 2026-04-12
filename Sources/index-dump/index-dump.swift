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
    abstract: "Dumps the content of unit or record files from an Index Store."
  )

  @Argument(help: "Name of the unit/record to print")
  var unitOrRecordName: String

  @Option(
    name: .customLong("lib-index-store"),
    help: "Path to libIndexStore. Inferred from the default toolchain if omitted."
  )
  var libIndexStore: String?

  @Option(
    name: .customLong("index-store"),
    help:
      "Path to the index store directory. This should be the directory containing the v5 directory. Inferred from the current working directory if omitted."
  )
  var indexStore: String?

  enum Mode: String, ExpressibleByArgument {
    case unit, record
  }

  var indexStorePath: URL {
    get throws {
      if let explicit = indexStore {
        return URL(fileURLWithPath: explicit)
      }

      var indexStorePath = URL(filePath: FileManager.default.currentDirectoryPath)
      while !indexStorePath.isRoot {
        if FileManager.default.fileExists(atPath: indexStorePath.appending(component: "v5").filePath) {
          return indexStorePath
        }
        indexStorePath.deleteLastPathComponent()
      }

      throw ValidationError(
        "Could not infer store path. Please change the working directory to a have a parent directory named v5 or explicitly specify --index-store."
      )
    }
  }

  var libIndexStorePath: URL {
    get throws {
      if let explicit = libIndexStore { return URL(fileURLWithPath: explicit) }

      guard let libURL = LibIndexStoreProvider.inferLibPath() else {
        throw ValidationError(
          "Could not find 'swift' to infer toolchain path. Please specify --lib-index-store explicitly."
        )
      }
      return libURL
    }
  }

  func run() async throws {
    let lib = try await IndexStoreLibrary.at(dylibPath: libIndexStorePath)
    let indexStorePath = try self.indexStorePath
    let store = try lib.indexStore(at: indexStorePath)

    if let unit = try? store.unit(named: unitOrRecordName) {
      print(unit)
    } else if let record = try? store.record(named: unitOrRecordName) {
      print(record)
    } else {
      throw ValidationError("Could not find unit or record named \(unitOrRecordName) in \(indexStorePath)")
    }
  }
}

extension URL {
  package var filePath: String {
    return self.withUnsafeFileSystemRepresentation { filePathPtr in
      return String(cString: filePathPtr!)
    }
  }

  /// Assuming this URL is a file URL, checks if it looks like a root path. This is a string check, ie. the return
  /// value for a path of `"/foo/.."` would be `false`. An error will be thrown is this is a non-file URL.
  package var isRoot: Bool {
    #if os(Windows)
    return filePath.withCString(encodedAs: UTF16.self, PathCchIsRoot)
    #else
    return filePath == "/"
    #endif
  }

}
