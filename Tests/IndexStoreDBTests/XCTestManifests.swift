#if !canImport(ObjectiveC)
import XCTest

extension IndexStoreDBTests {
    // DO NOT MODIFY: This is autogenerated, use:
    //   `swift test --generate-linuxmain`
    // to regenerate.
    static let __allTests__IndexStoreDBTests = [
        ("testCreateIndexStoreAndDBDirs", testCreateIndexStoreAndDBDirs),
        ("testErrors", testErrors),
    ]
}

extension IndexTests {
    // DO NOT MODIFY: This is autogenerated, use:
    //   `swift test --generate-linuxmain`
    // to regenerate.
    static let __allTests__IndexTests = [
        ("testAllSymbolNames", testAllSymbolNames),
        ("testBasic", testBasic),
        ("testDelegate", testDelegate),
        ("testEditsSimple", testEditsSimple),
        ("testExplicitOutputUnits", testExplicitOutputUnits),
        ("testMainFilesContainingFile", testMainFilesContainingFile),
        ("testMixedLangTarget", testMixedLangTarget),
        ("testSwiftModules", testSwiftModules),
        ("testWaitUntilDoneInitializing", testWaitUntilDoneInitializing),
    ]
}

extension LocationScannerTests {
    // DO NOT MODIFY: This is autogenerated, use:
    //   `swift test --generate-linuxmain`
    // to regenerate.
    static let __allTests__LocationScannerTests = [
        ("testDirectory", testDirectory),
        ("testDuplicate", testDuplicate),
        ("testLeft", testLeft),
        ("testLocation", testLocation),
        ("testMultiple", testMultiple),
        ("testName", testName),
        ("testNested", testNested),
        ("testSmall", testSmall),
        ("testUnicode", testUnicode),
    ]
}

public func __allTests() -> [XCTestCaseEntry] {
    return [
        testCase(IndexStoreDBTests.__allTests__IndexStoreDBTests),
        testCase(IndexTests.__allTests__IndexTests),
        testCase(LocationScannerTests.__allTests__LocationScannerTests),
    ]
}
#endif
