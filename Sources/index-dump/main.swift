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
import IndexStore
import ArgumentParser
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
    var libPath: String?

    @Option(help: "Path to the index store directory. Inferred if omitted.")
    var storePath: String?

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

    // MARK: - Logic Helpers

    private func inferMode(from path: String) throws -> Mode {
        let components = URL(fileURLWithPath: path).pathComponents
        if components.contains("units") { return .unit }
        if components.contains("records") { return .record }
        throw ValidationError("Could not infer mode from path. Please specify --mode explicitly.")
    }

    private func explicitOrInferredStorePath() throws -> URL {
        if let explicit = storePath { return URL(fileURLWithPath: explicit) }

        var url = URL(fileURLWithPath: nameOrPath)
        if url.pathComponents.contains("v5") {
            while url.lastPathComponent != "v5" && url.pathComponents.count > 1 {
                url = url.deletingLastPathComponent()
            }
            return url.deletingLastPathComponent()
        }
        throw ValidationError("Could not infer store path. Please specify --store-path.")
    }

    private func explicitOrInferredLibPath() throws -> URL {
        if let explicit = libPath { return URL(fileURLWithPath: explicit) }

        guard let libURL = Self.inferLibIndexStorePath() else {
            throw ValidationError("Could not find 'swift' to infer toolchain path. Please specify --lib-path explicitly.")
        }
        return libURL
    }
    
    // MARK: - Toolchain Discovery
    
    /// Find a tool using xcrun/which/where (copied logic from TibsToolchain.findTool)
    private static func findTool(name: String) -> URL? {
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
    private static func inferLibIndexStorePath() -> URL? {
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

        let libURL = toolchainURL.appendingPathComponent("lib").appendingPathComponent(libName)
        guard FileManager.default.fileExists(atPath: libURL.path) else {
            return nil
        }
        return libURL
    }
}