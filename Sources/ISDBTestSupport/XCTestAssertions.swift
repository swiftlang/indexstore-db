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

import IndexStoreDB
import XCTest

/// An XCTest assertion that the given symbol occurrences match the expected values, ignoring the
/// order of results, and optionally ignoring relations. By default, the given occurrences also are
/// allowed to have additional SymbolRoles.
///
/// If the occurrences to not match, signals a test failure using XCTFail or one of the other
/// XCTest assertions.
///
/// * parameters:
///   * actual: The set of occurrences to check, in any order.
///   * ignoreRelations:
///   * allowAdditionalRoles:
///   * expected: The set of expected occurrences to check against, in any order.
public func checkOccurrences(
  _ actual: [SymbolOccurrence],
  ignoreRelations: Bool = true,
  allowAdditionalRoles: Bool = true,
  expected: [SymbolOccurrence],
  file: StaticString = #file,
  line: UInt = #line)
{
  var expected: [SymbolOccurrence] = expected
  var actual: [SymbolOccurrence] = actual

  if ignoreRelations {
    for i in expected.indices {
      expected[i].relations = []
    }
    for i in actual.indices {
      actual[i].relations = []
    }
  }

  expected.sort()
  actual.sort()

  var ai = actual.startIndex
  let aend = actual.endIndex
  var ei = expected.startIndex
  let eend = expected.endIndex

  func compare(actual: SymbolOccurrence, expected: SymbolOccurrence) -> ComparisonResult {
    var actual = actual
    if allowAdditionalRoles {
      actual.roles.formIntersection(expected.roles)
    }

    if actual == expected { return .orderedSame }
    if actual < expected { return .orderedAscending }
    return .orderedDescending
  }

  while ai != aend && ei != eend {
    switch compare(actual: actual[ai], expected: expected[ei]) {
    case .orderedSame:
      actual.formIndex(after: &ai)
      expected.formIndex(after: &ei)
    case .orderedAscending:
      XCTFail("unexpected symbol occurrence \(actual[ai])", file: file, line: line)
      actual.formIndex(after: &ai)
    case .orderedDescending:
      XCTFail("missing expected symbol occurrence \(expected[ei])", file: file, line: line)
      expected.formIndex(after: &ei)
    }
  }

  while ai != aend {
    XCTFail("unexpected symbol occurrence \(actual[ai])", file: file, line: line)
    actual.formIndex(after: &ai)
  }

  while ei != eend {
    XCTFail("missing expected symbol occurrence \(expected[ei])", file: file, line: line)
    expected.formIndex(after: &ei)
  }
}
