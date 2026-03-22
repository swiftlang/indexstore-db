import Foundation
import ISDBTibs
import IndexStore
import Testing

struct TestProject {
  /// Files that should be indexed using `swiftc`.
  ///
  /// Keys are the file names, values are the file contents.
  var swiftFiles: [String: String] = [:]

  /// Files that should be indexed using `clang`.
  var clangFiles: [String: String] = [:]

  /// Files that should be written to disk but not indexed by themselves. Useful to write out header files.
  var supplementaryFiles: [String: String] = [:]

  private func writeFilesToDisk(_ files: [String: String], sourceDir: URL) throws -> [URL] {
    var urls: [URL] = []
    for (fileName, contents) in files {
      let fileUrl = sourceDir.appending(component: fileName)
      try contents.write(to: fileUrl, atomically: true, encoding: .utf8)
      urls.append(fileUrl)
    }
    return urls
  }

  func withIndexStore(body: (IndexStore) async throws -> Void) async throws {
    try await withTestScratchDir { scratchDir in
      let libIndexStore = try TibsToolchain.testDefault.libIndexStore

      let sourceDir = scratchDir.appending(component: "sources")
      try FileManager.default.createDirectory(at: sourceDir, withIntermediateDirectories: true)
      let indexDir = scratchDir.appending(component: "index")

      // First write all files to disk so they can reference each other, important in the case of C header
      let swiftFileUrls = try writeFilesToDisk(swiftFiles, sourceDir: sourceDir)
      let clangFileUrls = try writeFilesToDisk(clangFiles, sourceDir: sourceDir)
      _ = try writeFilesToDisk(supplementaryFiles, sourceDir: sourceDir)

      for url in swiftFileUrls {
        try indexSwiftFile(at: url, indexDir: indexDir)
      }
      for url in clangFileUrls {
        try indexClangFile(at: url, indexDir: indexDir)
      }

      let library = try await IndexStoreLibrary.at(dylibPath: libIndexStore)
      let indexStore = try library.indexStore(at: indexDir)
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

  private func indexClangFile(at fileUrl: URL, indexDir: URL) throws {
    let clang = try #require(findTool(name: "clang\(TibsToolchain.execExt)"))
    let compilerArguments = try [
      clang.filePath,
      "-fsyntax-only",
      fileUrl.filePath,
      "-index-store-path", indexDir.filePath,
      "-o", fileUrl.deletingPathExtension().appendingPathExtension("o").filePath,
    ]

    _ = try Process.tibs_checkNonZeroExit(arguments: compilerArguments)
  }
}
