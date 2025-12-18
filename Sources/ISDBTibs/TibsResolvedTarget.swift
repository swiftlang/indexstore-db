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

/// A tibs target with all its compilation units resolved and ready to build.
///
/// The resolved target contains all the information needed to index/build a single target in a tibs
/// build graph, assuming its dependencies have been built. It should be possible to generate a
/// command line invocation or ninja build description of each compilation unit in the resolved
/// target without additional information.
public final class TibsResolvedTarget {

  public struct SwiftModule: Equatable {
    public var name: String
    public var extraArgs: [String]
    public var sources: [URL]
    public var emitModulePath: String
    public var emitHeaderPath: String?
    public var outputFileMap: OutputFileMap
    public var outputFileMapPath: String { "\(name)-output-file-map.json" }
    public var bridgingHeader: URL?
    public var moduleDeps: [String]
    public var importPaths: [String] { moduleDeps.isEmpty ? [] : ["."] }
    public var sdk: String?

    public var indexOutputPaths: [String] {
      return outputFileMap.values.compactMap { $0.object ?? $0.swiftmodule }
    }

    public init(
      name: String,
      extraArgs: [String] = [],
      sources: [URL] = [],
      emitModulePath: String,
      emitHeaderPath: String? = nil,
      outputFileMap: OutputFileMap,
      bridgingHeader: URL? = nil,
      moduleDeps: [String] = [],
      sdk: String? = nil
    ) {
      self.name = name
      self.extraArgs = extraArgs
      self.sources = sources
      self.emitModulePath = emitModulePath
      self.emitHeaderPath = emitHeaderPath
      self.outputFileMap = outputFileMap
      self.bridgingHeader = bridgingHeader
      self.moduleDeps = moduleDeps
      self.sdk = sdk
    }
  }

  public struct ClangTU: Equatable {
    public var extraArgs: [String]
    public var source: URL
    public var importPaths: [String]
    public var generatedHeaderDep: String?
    public var outputPath: String

    public init(
      extraArgs: [String] = [],
      source: URL,
      importPaths: [String] = [],
      generatedHeaderDep: String? = nil,
      outputPath: String
    ) {
      self.extraArgs = extraArgs
      self.source = source
      self.importPaths = importPaths
      self.generatedHeaderDep = generatedHeaderDep
      self.outputPath = outputPath
    }
  }

  public var name: String
  public var swiftModule: SwiftModule?
  public var clangTUs: [ClangTU]
  public var dependencies: [String]

  public var indexOutputPaths: [String] {
    return clangTUs.map { $0.outputPath } + (swiftModule?.indexOutputPaths ?? [])
  }

  public init(name: String, swiftModule: SwiftModule?, clangTUs: [ClangTU], dependencies: [String]) {
    self.name = name
    self.swiftModule = swiftModule
    self.clangTUs = clangTUs
    self.dependencies = dependencies
  }
}

extension TibsResolvedTarget: Equatable {
  public static func == (a: TibsResolvedTarget, b: TibsResolvedTarget) -> Bool {
    if a === b {
      return true
    }
    return (a.name, a.swiftModule, a.clangTUs, a.dependencies)
      == (b.name, b.swiftModule, b.clangTUs, b.dependencies)
  }
}
