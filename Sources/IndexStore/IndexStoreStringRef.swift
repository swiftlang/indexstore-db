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

public import Foundation
public import IndexStoreDB_CIndexStoreDB

/// A potentially stack-allocated string yielded by the Index Store.
///
/// `IndexStore` uses this string type instead of `Swift.String` for two reasons:
///  - Its reference of the stack-allocated memory is more efficient than copying the strings contents into heap memory
///    that backs `Swift.String`.
///  - In contrast to `Swift.String`, this type does not validate its contents for valid Unicode, saving additional
///    performance overhead.
public struct IndexStoreStringRef: ~Escapable, Sendable {
  /// It would be nice if this was a `Span<UInt8>` but indexstore-db talks in terms of `Span<CChar>` (aka. `Span<Int8>`)
  /// and converting between different spans of different element types is not possible.
  /// https://github.com/swiftlang/swift/issues/85763
  public let span: RawSpan

  @_lifetime(borrow span)
  public init(_ span: RawSpan) {
    self.span = span
  }

  @usableFromInline
  init(_ stringRef: borrowing indexstore_string_ref_t) {
    // We cannot use optional binding here due to https://github.com/swiftlang/swift/issues/85753
    if stringRef.data != nil {
      self.span = RawSpan(_unsafeStart: stringRef.data!, count: stringRef.length)
    } else {
      self.span = RawSpan()
    }
  }

  @available(macOS 26, *)
  public init(_ bytes: borrowing [UInt8]) {
    self.span = bytes.span.bytes
  }

  @available(macOS 26, *)
  public init(_ string: borrowing String) {
    self.span = string.utf8Span.span.bytes
  }

  /// Generate an `IndexStoreStringRef` to a Swift `String`.
  ///
  /// When deploying against macOS 26 or higher, the initializer that borrows a `String` should be preferred.
  public static func withStringRef<T>(_ string: String, body: (IndexStoreStringRef) throws -> T) rethrows -> T {
    var string = string
    return try string.withUTF8 { buffer in
      if let baseAddress = buffer.baseAddress {
        let span = RawSpan(_unsafeStart: baseAddress, count: buffer.count)
        return try body(IndexStoreStringRef(span))
      }
      let span = RawSpan()
      return try body(IndexStoreStringRef(span))
    }
  }

  /// The string as a native Swift String.
  public var string: String {
    return span.withUnsafeBytes { String(decoding: $0, as: UTF8.self) }
  }

  /// The contents of the string as Foundation's `Data` type.
  public var data: Data {
    return span.withUnsafeBytes { buffer in Data(buffer) }
  }

  /// The contents of the string as an array of bytes.
  public var byteArray: [UInt8] {
    return span.withUnsafeBytes { Array($0.assumingMemoryBound(to: UInt8.self)) }
  }

  /// Execute the closure with an pointer to a null-terminated version of this string.
  func withCString<T>(_ body: (UnsafePointer<UInt8>) throws -> T) rethrows -> T {
    return try withUnsafeTemporaryAllocation(of: UInt8.self, capacity: span.byteCount + 1) { cString in
      let index = span.withUnsafeBytes { cString.initialize(fromContentsOf: $0) }
      cString.initializeElement(at: index, to: 0)
      return try body(UnsafePointer(cString.baseAddress!))
    }
  }
}
