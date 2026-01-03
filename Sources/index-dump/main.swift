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

        let determinedMode = mode ?? inferMode(from: nameOrPath)
        let cleanName = URL(fileURLWithPath: nameOrPath).lastPathComponent
        
        switch determinedMode {
        case .unit:
            let unit = try store.unit(named: cleanName)
            print(dumpUnit(unit, name: cleanName))
        case .record:
            let record = try store.record(named: cleanName)
            print(dumpRecord(record, name: cleanName))
        }
    }

    // MARK: - Logic Helpers

    private func inferMode(from path: String) -> Mode {
        let components = URL(fileURLWithPath: path).pathComponents
        if components.contains("units") { return .unit }
        if components.contains("records") { return .record }
        return .unit
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

        guard let swiftURL = findTool(name: "swift") else {
            throw ValidationError("Could not find 'swift' to infer toolchain path.")
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
            throw ValidationError("Found toolchain but \(libName) is missing at \(libURL.path).")
        }
        return libURL
    }

    private func findTool(name: String) -> URL? {
        #if os(macOS)
        let cmd = ["/usr/bin/xcrun", "--find", name]
        #elseif os(Windows)
        var buf = [WCHAR](repeating: 0, count: Int(MAX_PATH))
        GetWindowsDirectoryW(&buf, UINT(MAX_PATH))
        let wherePath = String(decodingCString: &buf, as: UTF16.self)
            .appendingPathComponent("system32")
            .appendingPathComponent("where.exe")
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

    // MARK: - Formatting

    private func dumpUnit(_ unit: IndexStoreUnit, name: String) -> String {
        var output = "Unit Name: \(name)\n"
        output += "Module: \(unit.moduleName.string)\n"
        output += "Has Main File: \(unit.hasMainFile)\n"
        output += "Main File: \(unit.mainFile.string)\n"
        output += "Output File: \(unit.outputFile.string)\n"
        output += "Target: \(unit.target.string)\n"
        output += "Sysroot: \(unit.sysrootPath.string)\n"
        output += "Working Directory: \(unit.workingDirectory.string)\n"
        output += "Is System: \(unit.isSystemUnit)\n"
        output += "Is Module: \(unit.isModuleUnit)\n"
        output += "Is Debug: \(unit.isDebugCompilation)\n"
        output += "Provider Identifier: \(unit.providerIdentifier.string)\n"
        output += "Provider Version: \(unit.providerVersion.string)\n"
        output += "Mod Date: \(unit.modificationDate)\n"

        output += "\nDEPENDENCIES START\n"
        unit.dependencies.forEach { dep in
            output += "\(String(describing: dep.kind).capitalized) | \(dep.name.string)\n"
            return .continue
        }
        output += "DEPENDENCIES END\n"
        return output
    }

    private func dumpRecord(_ record: IndexStoreRecord, name: String) -> String {
        var output = "Record: \(name)\n\nSYMBOLS START\n"
        record.symbols.forEach { sym in
            output += "\(sym.kind) | \(sym.name.string) | USR: \(sym.usr.string)\n"
            return .continue
        }
        output += "SYMBOLS END\n\nOCCURRENCES START\n"
        record.occurrences.forEach { occ in
            output += "\(occ.symbol.kind) | \(occ.symbol.name.string) | \(occ.position.line):\(occ.position.column) | Roles: \(occ.roles)\n"
            return .continue
        }
        output += "OCCURRENCES END\n"
        return output
    }
}

extension String {
    func appendingPathComponent(_ component: String) -> String {
        return (self as NSString).appendingPathComponent(component)
    }
}