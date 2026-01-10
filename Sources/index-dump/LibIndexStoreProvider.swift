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

import Foundation

#if os(Windows)
import WinSDK
#endif

/// Provides utilities to discover and locate libIndexStore in the system.
private enum LibIndexStoreProvider {
  /// Find a tool using xcrun/which/where (copied logic from TibsToolchain.findTool)
  static func findTool(name: String) -> URL? {
    #if os(macOS)
    let cmd = ["/usr/bin/xcrun", "--find", name]
    #elseif os(Windows)
    var buf = [WCHAR](repeating: 0, count: Int(MAX_PATH))
    GetWindowsDirectoryW(&buf, UINT(MAX_PATH))
    var wherePath = String(decodingCString: &buf, as: UTF16.self)
    wherePath = (wherePath as NSString).appendingPathComponent("system32")
    wherePath = (wherePath as NSString).appendingPathComponent("where.exe")
    let cmd = [wherePath, name]
    #else
    let cmd = ["/usr/bin/which", name]
    #endif

    let process = Process()
    process.executableURL = URL(fileURLWithPath: cmd[0])
    process.arguments = Array(cmd.dropFirst())
    let pipe = Pipe()
    process.standardOutput = pipe

    try? process.run()
    process.waitUntilExit()
    guard process.terminationStatus == 0 else { return nil }

    let data = pipe.fileHandleForReading.readDataToEndOfFile()
    var path = String(decoding: data, as: UTF8.self)
    #if os(Windows)
    path = String((path.split { $0.isNewline })[0])
    #endif
    return URL(fileURLWithPath: path.trimmingCharacters(in: .whitespacesAndNewlines))
  }

  /// Infer libIndexStore dylib path from the default toolchain
  static func inferLibPath() -> URL? {
    guard let swiftURL = findTool(name: "swift") else {
      return nil
    }

    let toolchainURL = swiftURL.deletingLastPathComponent().deletingLastPathComponent()

    #if os(macOS)
    let libName = "libIndexStore.dylib"
    #elseif os(Windows)
    let libName = "IndexStore.dll"
    #else
    let libName = "libIndexStore.so"
    #endif

    let libURL = toolchainURL.appending(components: "lib", libName)
    guard FileManager.default.fileExists(atPath: libURL.path) else {
      return nil
    }
    return libURL
  }
}
