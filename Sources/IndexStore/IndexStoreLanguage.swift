//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

public import IndexStoreDB_CIndexStoreDB

/// The language of a source file that was processed by the Index Store.
public struct IndexStoreLanguage: RawRepresentable, Hashable, Sendable {
  public let rawValue: UInt8

  /// C
  public static let c = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_C)

  /// Objective-C
  public static let objectiveC = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_OBJC)

  /// C++
  public static let cxx = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_CXX)

  /// Swift
  public static let swift = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_SWIFT)

  @inlinable
  public init(rawValue: UInt8) {
    self.rawValue = rawValue
  }

  @usableFromInline
  init(_ language: indexstore_symbol_language_t) {
    self.rawValue = UInt8(language.rawValue)
  }
}
