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

/// Manifest of a tibs project, describing each target and its dependencies. By convention, it is
/// located at `$SOURCE_ROOT/project.json`.
///
/// Example:
///
/// ```
/// {
///   "targets": [
///     {
///       "name": "mytarget",
///       "swift_flags": ["-warnings-as-errors"],
///       "sources": ["a.swift", "b.swift"],
///       "dependencies": ["dep1"],
///     },
///     {
///       "name": "dep1",
///       "sources": ["dep1.swift"],
///     },
///   ]
/// }
/// ```
///
/// As a convenience, if the project consists of a single target, it can be written at the top
/// level. Thus, a minimal manifest is just the list of sources:
///
/// ```
/// { "sources": ["main.swift"] }
/// ```
public struct TibsManifest: Equatable {

  /// Description of a target within a TibsManifest.
  public struct Target: Equatable {
    public var name: String? = nil
    public var swiftFlags: [String]? = nil
    public var clangFlags: [String]? = nil
    public var sources: [String]
    public var bridgingHeader: String? = nil
    public var dependencies: [String]? = nil

    public init(
      name: String? = nil,
      swiftFlags: [String]? = nil,
      clangFlags: [String]? = nil,
      sources: [String],
      bridgingHeader: String? = nil,
      dependencies: [String]? = nil
    ) {
      self.name = name
      self.swiftFlags = swiftFlags
      self.clangFlags = clangFlags
      self.sources = sources
      self.bridgingHeader = bridgingHeader
      self.dependencies = dependencies
    }
  }

  public var targets: [Target]

  public init(targets: [Target] = []) {
    self.targets = targets
  }
}

extension TibsManifest.Target: Codable {
  private enum CodingKeys: String, CodingKey {
    case name
    case swiftFlags = "swift_flags"
    case clangFlags = "clang_flags"
    case sources
    case bridgingHeader = "bridging_header"
    case dependencies
  }
}

extension TibsManifest: Codable {
  private enum CodingKeys: String, CodingKey {
    case targets
  }

  public init(from decoder: Decoder) throws {
    let container = try decoder.container(keyedBy: CodingKeys.self)
    if let targets = try container.decodeIfPresent([Target].self, forKey: .targets) {
      self.targets = targets
    } else {
      self.targets = [try Target(from: decoder)]
    }
  }

  public func encode(to encoder: Encoder) throws {
    if self.targets.count == 1 {
      try self.targets[0].encode(to: encoder)
    } else {
      var container = encoder.container(keyedBy: CodingKeys.self)
      try container.encode(targets, forKey: .targets)
    }
  }

  /// The standard path to the manifest file relative to the project's the source root.
  public static let standardPath: String = "project.json"

  /// Load a tibs manifest for the given project directory.
  ///
  /// This is a convenience method that looks for the manifest file in `standardPath` and decodes it
  /// from JSON. The resulting manifest does not reference the project directory, and can
  /// subsequently be used with a different source root, for example after copying sources.
  public static func load(projectRoot: URL) throws -> Self {
    let manifestURL = projectRoot.appendingPathComponent(standardPath, isDirectory: false)
    return try JSONDecoder().decode(Self.self, from: try Data(contentsOf: manifestURL))
  }
}
