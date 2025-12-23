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

#if os(Windows)
import WinSDK
#elseif canImport(Android)
import Android
#endif

/// The set of commandline tools used to build a tibs project.
public final class TibsToolchain {
  public let swiftc: URL
  public let clang: URL
  public let libIndexStore: URL
  public let tibs: URL
  public let ninja: URL?

  public init(swiftc: URL, clang: URL, libIndexStore: URL? = nil, tibs: URL, ninja: URL? = nil) {
    self.swiftc = swiftc
    self.clang = clang

    #if os(Windows)
    let dylibFolder = "bin"
    #else
    let dylibFolder = "lib"
    #endif

    self.libIndexStore =
      libIndexStore
      ?? swiftc
      .deletingLastPathComponent()
      .deletingLastPathComponent()
      .appendingPathComponent("\(dylibFolder)/libIndexStore\(TibsToolchain.dylibExt)", isDirectory: false)

    self.tibs = tibs
    self.ninja = ninja
  }

  #if os(macOS)
  public static let dylibExt = ".dylib"
  public static let execExt = ""
  #elseif os(Windows)
  public static let dylibExt = ".dll"
  public static let execExt = ".exe"
  #else
  public static let dylibExt = ".so"
  public static let execExt = ""
  #endif

  public private(set) lazy var clangHasIndexSupport: Bool = {
    // Check clang -help for index store support. It would be better to check
    // that `clang -index-store-path /dev/null --version` does not produce an
    // error, but unfortunately older versions of clang accepted
    // `-index-store-path` due to the existence of a `-i` option that has since
    // been removed. While we could check a full compile command, I have not
    // found a robust command that detects index support without also doing I/O
    // in the index directory when it succeeds. To avoid I/O, we check -help.

    let cmd = [clang.path, "-help"]
    do {
      let output = try Process.tibs_checkNonZeroExit(arguments: cmd)
      if output.contains("-index-store-path") {
        return true
      } else {
        return false
      }
    } catch {
      assertionFailure("unexpected error executing \(cmd.joined(separator: " ")): \(error)")
      return false
    }
  }()

  public private(set) lazy var ninjaVersion: (Int, Int, Int) = {
    precondition(ninja != nil, "expected non-nil ninja in ninjaVersion")
    var out = try! Process.tibs_checkNonZeroExit(arguments: [ninja!.path, "--version"])
    out = out.trimmingCharacters(in: .whitespacesAndNewlines)
    let components = out.split(separator: ".", maxSplits: 3)
    guard let maj = Int(String(components[0])),
      let min = Int(String(components[1])),
      let patch = components.count > 2 ? Int(String(components[2])) : 0
    else {
      fatalError("could not parsed ninja --version '\(out)'")
    }
    return (maj, min, patch)
  }()

  /// Sleep long enough for file system timestamp to change. For example, older versions of ninja
  /// use 1 second timestamps.
  public func sleepForTimestamp() {
    // FIXME: this method is very incomplete. If we're running on a filesystem that doesn't support
    // high resolution time stamps, we'll need to detect that here. This should only be done for
    // testing.

    // Empirically, a little over 10 ms resolution is seen on some systems.
    let minimum: UInt32 = 20_000
    var usec: UInt32 = minimum
    var warning: String? = nil

    if ninjaVersion < (1, 9, 0) {
      usec = 1_000_000
      warning = "upgrade to ninja >= 1.9.0 for high precision timestamp support"
    }

    if usec > 0 {
      if let warning = warning {
        let fsec = Float(usec) / 1_000_000
        fputs(
          "warning: waiting \(fsec) second\(fsec == 1.0 ? "" : "s") to ensure file timestamp "
            + "differs; \(warning)\n",
          stderr
        )
      }

      #if os(Windows)
      Sleep(usec / 1000)
      #else
      usleep(usec)
      #endif
    }
  }
}

extension TibsToolchain {

  /// A toolchain suitable for using `tibs` for testing. Will `fatalError()` if we cannot determine
  /// any components.
  public static var testDefault: TibsToolchain {
    var swiftc: URL? = nil
    var clang: URL? = nil
    var tibs: URL? = nil
    var ninja: URL? = nil

    let fm = FileManager.default

    let envVar = "INDEXSTOREDB_TOOLCHAIN_BIN_PATH"
    if let path = ProcessInfo.processInfo.environment[envVar] {
      let bin = URL(fileURLWithPath: "\(path)/bin", isDirectory: true)
      swiftc = bin.appendingPathComponent("swiftc\(TibsToolchain.execExt)", isDirectory: false)
      clang = bin.appendingPathComponent("clang\(TibsToolchain.execExt)", isDirectory: false)

      if !fm.fileExists(atPath: swiftc!.path) {
        fatalError("toolchain must contain 'swiftc\(TibsToolchain.execExt)' \(envVar)=\(path)")
      }
      if !fm.fileExists(atPath: clang!.path) {
        clang = nil  // try to find by PATH
      }
    }

    if let ninjaPath = ProcessInfo.processInfo.environment["NINJA_BIN"] {
      ninja = URL(fileURLWithPath: ninjaPath, isDirectory: false)
    }

    var buildURL: URL? = nil
    #if os(macOS)
    // If we are running under xctest, the build directory is the .xctest bundle.
    for bundle in Bundle.allBundles {
      if bundle.bundlePath.hasSuffix(".xctest") {
        buildURL = bundle.bundleURL.deletingLastPathComponent()
        break
      }
    }
    // Otherwise, assume it is the main bundle.
    if buildURL == nil {
      buildURL = Bundle.main.bundleURL
    }
    #else
    buildURL = Bundle.main.bundleURL
    #endif

    if let buildURL = buildURL {
      tibs = buildURL.appendingPathComponent("tibs\(TibsToolchain.execExt)", isDirectory: false)
      if !fm.fileExists(atPath: tibs!.path) {
        tibs = nil  // try to find by PATH
      }
    }

    swiftc = swiftc ?? findTool(name: "swiftc\(TibsToolchain.execExt)")
    clang = clang ?? findTool(name: "clang\(TibsToolchain.execExt)")
    tibs = tibs ?? findTool(name: "tibs\(TibsToolchain.execExt)")
    ninja = ninja ?? findTool(name: "ninja\(TibsToolchain.execExt)")

    guard swiftc != nil, clang != nil, tibs != nil, ninja != nil else {
      fatalError(
        """
        missing TibsToolchain component; had
          swiftc = \(swiftc?.path ?? "nil")
          clang = \(clang?.path ?? "nil")
          tibs = \(tibs?.path ?? "nil")
          ninja = \(ninja?.path ?? "nil")
        """
      )
    }

    return TibsToolchain(swiftc: swiftc!, clang: clang!, tibs: tibs!, ninja: ninja)
  }
}

/// Returns the path to the given tool, as found by `xcrun --find` on macOS, or `which` on Linux.
public func findTool(name: String) -> URL? {
  #if os(macOS)
  let cmd = ["/usr/bin/xcrun", "--find", name]
  #elseif os(Windows)
  var buf = [WCHAR](repeating: 0, count: Int(MAX_PATH))
  GetWindowsDirectoryW(&buf, DWORD(MAX_PATH))
  var wherePath = String(decodingCString: &buf, as: UTF16.self)
    .appendingPathComponent("system32")
    .appendingPathComponent("where.exe")
  let cmd = [wherePath, name]
  #elseif os(Android)
  let cmd = ["/system/bin/which", name]
  #else
  let cmd = ["/usr/bin/which", name]
  #endif

  guard var path = try? Process.tibs_checkNonZeroExit(arguments: cmd) else {
    return nil
  }
  #if os(Windows)
  path = String((path.split { $0.isNewline })[0])
  #endif
  path = path.trimmingCharacters(in: .whitespacesAndNewlines)
  return URL(fileURLWithPath: path, isDirectory: false)
}
