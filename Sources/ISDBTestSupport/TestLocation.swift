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

  /// Marker string to mark a `SymbolLocation` as containing an unknown module name from a test location.
  public static let unknownModuleName: String = "<UNKNOWN>"

  /// The path/url of the source file.
  public var url: URL

  /// The one-based line number.
  public var line: Int

  /// The one-based UTF-8 column index.
  public var utf8Column: Int

  /// The one-based UTF-16 column index.
  public var utf16Column: Int

  public init(url: URL, line: Int, utf8Column: Int, utf16Column: Int) {
    self.url = url
    self.line = line
    self.utf8Column = utf8Column
    self.utf16Column = utf16Column
  }
}

extension TestLocation: Comparable {
  public static func < (a: TestLocation, b: TestLocation) -> Bool {
    return (a.url.path, a.line, a.utf8Column) < (b.url.path, b.line, b.utf8Column)
  }
}

extension SymbolLocation {

  /// Constructs a SymbolLocation from a TestLocation, using a non-system path by default.
  public init(_ loc: TestLocation, moduleName: String = TestLocation.unknownModuleName, isSystem: Bool = false) {
    self.init(
      path: loc.url.path,
      timestamp: Date(),
      moduleName: moduleName,
      isSystem: isSystem,
      line: loc.line,
      utf8Column: loc.utf8Column
    )
  }
}

extension Symbol {

  /// Returns a SymbolOccurrence with the given location and roles.
  public func at(
    _ location: TestLocation,
    moduleName: String = TestLocation.unknownModuleName,
    roles: SymbolRole,
    symbolProvider: SymbolProviderKind
  ) -> SymbolOccurrence {
    return self.at(SymbolLocation(location, moduleName: moduleName), symbolProvider: symbolProvider, roles: roles)
  }
}

extension TestLocation: CustomStringConvertible {
  public var description: String { "\(url.path):\(line):\(utf8Column)" }
}
