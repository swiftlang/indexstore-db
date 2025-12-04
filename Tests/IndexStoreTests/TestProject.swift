import Foundation
import ISDBTibs
import IndexStore
import Testing

struct TestProject {
  let swiftFiles: [String: String]

  func withIndexStore(body: (IndexStore) async throws -> Void) async throws {
    try await withTestScratchDir { scratchDir in
      let swiftc = try #require(findTool(name: "swiftc\(TibsToolchain.execExt)"))
      let libIndexStore =
        swiftc
        .deletingLastPathComponent()
        .deletingLastPathComponent()
        .appending(components: "lib", "libIndexStore.dylib")

      let sourceDir = scratchDir.appending(component: "sources")
      try FileManager.default.createDirectory(at: sourceDir, withIntermediateDirectories: true)
      let indexDir = scratchDir.appending(component: "index")
      for (fileName, contents) in swiftFiles {
        let fileUrl = sourceDir.appending(component: fileName)
        try contents.write(to: fileUrl, atomically: true, encoding: .utf8)
        try indexSwiftFile(at: fileUrl, indexDir: indexDir)
      }
      let library = try await IndexStoreLibrary.at(dylibPath: libIndexStore)
      let indexStore = try library.indexStore(at: indexDir.filePath)
      try await body(indexStore)
    }
  }

  private func indexSwiftFile(at fileUrl: URL, indexDir: URL) throws {
    let swiftc = try #require(findTool(name: "swiftc\(TibsToolchain.execExt)"))
    var compilerArguments = try [
      swiftc.filePath,
      "-index-file",
      fileUrl.filePath,
      "-index-store-path", indexDir.filePath,
      "-o", fileUrl.deletingPathExtension().appendingPathExtension("o").filePath,
      "-index-ignore-system-modules",
    ]
    if let sdk = defaultSDKPath {
      compilerArguments += ["-sdk", sdk]
    }

    _ = try Process.tibs_checkNonZeroExit(arguments: compilerArguments)
  }
}
