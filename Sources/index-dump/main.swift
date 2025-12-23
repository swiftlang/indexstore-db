import Foundation
import IndexStore

func main() async {
    func printUsage() {
        print("""
        Usage: index-dump <path-to-libIndexStore> <path-to-index-store> <mode> <name>
        
        Modes:
          unit   Dump a Unit file
          record Dump a Record file
        """)
    }

    guard CommandLine.arguments.count == 5 else {
        printUsage()
        exit(1)
    }

    let libPath = CommandLine.arguments[1]
    let storePath = CommandLine.arguments[2]
    let mode = CommandLine.arguments[3]
    let name = CommandLine.arguments[4]

    do {
        let libURL = URL(fileURLWithPath: libPath)
        let lib = try await IndexStoreLibrary.at(dylibPath: libURL)
        let storeURL = URL(fileURLWithPath: storePath)
        let store = try lib.indexStore(at: storeURL)

        if mode == "unit" {
            let unit = try store.unit(named: name)
            
            print("Unit Name: \(name)")
            print("Module: \(unit.moduleName.string)")
            print("Main File: \(unit.mainFile.string)")
            print("Output File: \(unit.outputFile.string)")

            print("DEPEND START")
            unit.dependencies.forEach { dep in
                let kindStr: String
                switch dep.kind {
                case .unit: kindStr = "Unit"
                case .record: kindStr = "Record"
                case .file: kindStr = "File"
                default: kindStr = "Unknown(\(dep.kind))"
                }
                
                print("\(kindStr) | \(dep.name.string)")
                return .continue
            }
            print("DEPEND END")
            
        } else if mode == "record" {
            let record = try store.record(named: name)
            print("Record: \(name)")
            print("SYMBOLS START")
            
            record.occurrences.forEach { occurrence in
                let symbol = occurrence.symbol
                print("\(symbol.kind) | \(symbol.name.string)")
                return .continue
            }
            print("SYMBOLS END")
            
        } else {
            print("Unknown mode: \(mode)")
            exit(1)
        }

    } catch {
        print("Failed: \(error)")
        exit(1)
    }
}

await main()