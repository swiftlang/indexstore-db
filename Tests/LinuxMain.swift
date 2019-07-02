import XCTest

import ISDBTibsTests
import IndexStoreDBTests

var tests = [XCTestCaseEntry]()
tests += ISDBTibsTests.__allTests()
tests += IndexStoreDBTests.__allTests()

XCTMain(tests)
