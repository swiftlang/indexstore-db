import Foundation
import IndexStore
import ArgumentParser

@main
struct IndexDump: AsyncParsableCommand {
    static let configuration = CommandConfiguration(
        abstract: "Dumps the content of unit or record files from an IndexStore."
    )

    @Argument(help: "Path to the libIndexStore dylib/so file")
    var libPath: String

    @Argument(help: "Path to the index store directory")
    var storePath: String

    @Argument(help: "Name of the unit/record or a direct path to the file")
    var nameOrPath: String

    @Option(help: "Explicitly set mode (unit/record). Inferred from path if omitted.")
    var mode: Mode?

    enum Mode: String, ExpressibleByArgument {
        case unit, record
    }

    func run() async throws {
        let libURL = URL(fileURLWithPath: libPath)
        let lib = try await IndexStoreLibrary.at(dylibPath: libURL)
        let store = try lib.indexStore(at: URL(fileURLWithPath: storePath))

        // Determine mode: explicit flag > path search > default to unit
        let determinedMode: Mode
        if let explicitMode = mode {
            determinedMode = explicitMode
        } else if nameOrPath.contains("/units/") {
            determinedMode = .unit
        } else if nameOrPath.contains("/records/") {
            determinedMode = .record
        } else {
            determinedMode = .unit 
        }

        let cleanName = URL(fileURLWithPath: nameOrPath).lastPathComponent
        var output = ""

        if determinedMode == .unit {
            let unit = try store.unit(named: cleanName)
            output += "Unit Name: \(cleanName)\n"
            output += "Module: \(unit.moduleName.string)\n"
            output += "Main File: \(unit.mainFile.string)\n"
            output += "Output File: \(unit.outputFile.string)\n"
            
            output += "DEPEND START\n"
            unit.dependencies.forEach { dep in
                let kind: String
                switch dep.kind {
                case .unit: kind = "Unit"
                case .record: kind = "Record"
                case .file: kind = "File"
                default: kind = "Unknown(\(dep.kind))"
                }
                output += "\(kind) | \(dep.name.string)\n"
                return .continue
            }
            output += "DEPEND END\n"

        } else {
            let record = try store.record(named: cleanName)
            output += "Record: \(cleanName)\n"
            output += "SYMBOLS START\n"
            record.occurrences.forEach { occ in
                let sym = occ.symbol
                output += "\(sym.kind) | \(sym.name.string)\n"
                return .continue
            }
            output += "SYMBOLS END\n"
        }

        print(output)
    }
}