import Foundation

package let cleanScratchDirectories =
  (ProcessInfo.processInfo.environment["INDEXSTORE_DB_KEEP_TEST_SCRATCH_DIR"] == nil)

package func testScratchName(testName: String = #function) -> String {
  var uuid = UUID().uuidString[...]
  if let firstDash = uuid.firstIndex(of: "-") {
    uuid = uuid[..<firstDash]
  }

  let testBaseName = testName.prefix(while: \.isLetter)
  return "\(testBaseName)-\(uuid)"
}

/// An empty directory in which a test with `#function` name `testName` can store temporary data.
package func testScratchDir(testName: String = #function) throws -> URL {
  #if os(Windows)
  // Use a shorter test scratch dir name on Windows to not exceed MAX_PATH length
  let testScratchDirsName = "lsp-test"
  #else
  let testScratchDirsName = "indexstore-db-test-scratch"
  #endif

  let url = try FileManager.default.temporaryDirectory.realpath
    .appending(component: testScratchDirsName)
    .appending(component: testScratchName(testName: testName), directoryHint: .isDirectory)

  try? FileManager.default.removeItem(at: url)
  try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
  return url
}

/// Execute `body` with a path to a temporary scratch directory for the given
/// test name.
///
/// The temporary directory will be deleted at the end of `directory` unless the
/// `INDEXSTORE_DB_KEEP_TEST_SCRATCH_DIR` environment variable is set.
package func withTestScratchDir<T>(
  _ body: (URL) async throws -> T,
  testName: String = #function
) async throws -> T {
  let scratchDirectory = try testScratchDir(testName: testName)
  try FileManager.default.createDirectory(at: scratchDirectory, withIntermediateDirectories: true)
  defer {
    if cleanScratchDirectories {
      try? FileManager.default.removeItem(at: scratchDirectory)
    }
  }
  return try await body(scratchDirectory)
}
