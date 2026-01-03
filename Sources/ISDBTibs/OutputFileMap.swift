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

import struct Foundation.URL

/// A swiftc-compatible output file map, describing the set of auxiliary output files for a Swift
/// compilation.
public struct OutputFileMap {

  /// A single entry in the OutputFileMap.
  public struct Entry: Hashable, Codable {
    public var object: String?
    public var swiftmodule: String?
    public var swiftdoc: String?
    public var dependencies: String?

    public init(
      object: String? = nil,
      swiftmodule: String? = nil,
      swiftdoc: String? = nil,
      dependencies: String? = nil
    ) {
      self.object = object
      self.swiftmodule = swiftmodule
      self.swiftdoc = swiftdoc
      self.dependencies = dependencies
    }
  }

  var impl: [String: Entry] = [:]
  var order: [String] = []

  public init() {}

  public subscript(file: String) -> Entry? {
    get { impl[file] }

    set {
      precondition(newValue != nil, "OutputFileMap does not support removal")
      if impl.updateValue(newValue!, forKey: file) == nil {
        // New entry.
        order.append(file)
      }
    }
  }

  /// All of the entries, in the order they were added.
  public var values: LazyMapSequence<[String], Entry> {
    order.lazy.map { self.impl[$0]! }
  }
}

extension OutputFileMap: Codable {

  private struct StringKey: CodingKey {
    var stringValue: String

    init(stringValue: String) {
      self.stringValue = stringValue
    }

    var intValue: Int? { nil }
    init?(intValue: Int) { return nil }
  }

  public init(from decoder: Decoder) throws {
    let container = try decoder.container(keyedBy: StringKey.self)
    // Note: allKeys is not in any guaranteed order, so we cannot round trip the order of entries.
    for key in container.allKeys {
      self[key.stringValue] = try container.decode(Entry.self, forKey: key)
    }
  }

  public func encode(to encoder: Encoder) throws {
    var container = encoder.container(keyedBy: StringKey.self)
    // Note: the underlying encoder may not preserve the order of values.
    for file in order {
      try container.encode(impl[file]!, forKey: StringKey(stringValue: file))
    }
  }
}

extension OutputFileMap: Equatable {
  public static func == (a: OutputFileMap, b: OutputFileMap) -> Bool {
    return a.order == b.order && a.values.elementsEqual(b.values)
  }
}
