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

import Foundation
import IndexStoreDB

/// A source location (file:line:column) in a test project, for use with the TestLocationScanner.
public struct TestLocation: Hashable {

  /// The path/url of the source file.
  public var url: URL

  /// The one-based line number.
  public var line: Int

  /// The one-based column number.
  ///
  /// FIXME: define utf8 vs. utf16 column index.
  public var column: Int

  public init(url: URL, line: Int, column: Int) {
    self.url = url
    self.line = line
    self.column = column
  }
}

extension TestLocation: Comparable {
  public static func <(a: TestLocation, b: TestLocation) -> Bool {
    return (a.url.path, a.line, a.column) < (b.url.path, b.line, b.column)
  }
}

extension SymbolLocation {

  /// Constructs a SymbolLocation from a TestLocation, using a non-system path by default.
  public init(_ loc: TestLocation, isSystem: Bool = false) {
    self.init(
      path: loc.url.path,
      isSystem: isSystem,
      line: loc.line,
      utf8Column: loc.column)
  }
}

extension Symbol {

  /// Returns a SymbolOccurrence with the given location and roles.
  public func at(_ location: TestLocation, roles: SymbolRole) -> SymbolOccurrence {
    return self.at(SymbolLocation(location), roles: roles)
  }
}

extension TestLocation: CustomStringConvertible {
  public var description: String { "\(url.path):\(line):\(column)" }
}
