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

public struct IndexStoreLanguage: Hashable, Sendable {
  private let rawValue: indexstore_symbol_language_t

  public static let c = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_C)
  public static let objectiveC = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_OBJC)
  public static let cxx = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_CXX)
  public static let swift = IndexStoreLanguage(INDEXSTORE_SYMBOL_LANG_SWIFT)

  @usableFromInline
  init(_ rawValue: indexstore_symbol_language_t) {
    self.rawValue = rawValue
  }
}
