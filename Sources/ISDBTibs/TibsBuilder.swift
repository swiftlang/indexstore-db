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

/// Given a `TibsManifest`, resolves all of its `TibsResolvedTarget`s and provides API to build the
/// project.
public final class TibsBuilder {

  public private(set) var targets: [TibsResolvedTarget] = []
  public private(set) var targetsByName: [String: TibsResolvedTarget] = [:]
  public private(set) var toolchain: TibsToolchain
  public private(set) var buildRoot: URL

  public var indexstore: URL { buildRoot.appendingPathComponent("index", isDirectory: true) }

  public enum Error: Swift.Error {
    case duplicateTarget(String)
    case unknownDependency(String, declaredIn: String)
    case buildFailure(Process.TerminationReason, exitCode: Int32)
    case noNinjaBinaryConfigured
  }

  public init(manifest: TibsManifest, sourceRoot: URL, buildRoot: URL, toolchain: TibsToolchain) throws {
    self.toolchain = toolchain
    self.buildRoot = buildRoot

    for targetDesc in manifest.targets {
      let name = targetDesc.name ?? "main"
      let sources = targetDesc.sources.map {
        sourceRoot.appendingPathComponent($0, isDirectory: false)
      }
      let bridgingHeader = targetDesc.bridgingHeader.map {
        sourceRoot.appendingPathComponent($0, isDirectory: false)
      }

      let swiftFlags = targetDesc.swiftFlags ?? []
      let clangFlags = targetDesc.clangFlags ?? []

      let swiftSources = sources.filter { $0.pathExtension == "swift" }
      let clangSources = sources.filter { $0.pathExtension != "swift" }

      var swiftModule: TibsResolvedTarget.SwiftModule? = nil
      if !swiftSources.isEmpty {
        var outputFileMap = OutputFileMap()
        for source in swiftSources {
          let basename = source.lastPathComponent
          outputFileMap[source.path] = OutputFileMap.Entry(
            swiftmodule: "\(name)-\(basename).swiftmodule~partial",
            swiftdoc: "\(name)-\(basename).swiftdoc~partial",
            dependencies: "\(name)-\(basename).d")
        }

        swiftModule = TibsResolvedTarget.SwiftModule(
          name: name,
          extraArgs: swiftFlags + clangFlags.flatMap { ["-Xcc", $0] },
          sources: swiftSources,
          emitModulePath: "\(name).swiftmodule",
          emitHeaderPath: clangSources.isEmpty ? nil : "\(name)-Swift.h",
          outputFileMap: outputFileMap,
          bridgingHeader: bridgingHeader,
          moduleDeps: targetDesc.dependencies?.map { "\($0).swiftmodule" } ?? [])
      }

      var clangTUs: [TibsResolvedTarget.ClangTU] = []
      for source in clangSources {
        let cu = TibsResolvedTarget.ClangTU(
          extraArgs: clangFlags,
          source: source,
          importPaths: [/*buildRoot*/".", sourceRoot.path],
          // FIXME: this should be the -Swift.h file, but ninja doesn't support
          // having multiple output files when using gcc-style dependencies, so
          // use the .swiftmodule.
          generatedHeaderDep: swiftSources.isEmpty ? nil : "\(name).swiftmodule",
          outputPath: "\(name)-\(source.lastPathComponent).o")
        clangTUs.append(cu)
      }

      let target = TibsResolvedTarget(
        name: name,
        swiftModule: swiftModule,
        clangTUs: clangTUs,
        dependencies: targetDesc.dependencies ?? [])

      targets.append(target)
      if targetsByName.updateValue(target, forKey: name) != nil {
        throw Error.duplicateTarget(name)
      }
    }

    for target in targets {
      for dep in target.dependencies {
        if targetsByName[dep] == nil {
          throw Error.unknownDependency(dep, declaredIn: target.name)
        }
      }
    }
  }
}

extension TibsBuilder {

  // MARK: Building

  public func build() throws {
    try buildImpl()
  }

  /// *For Testing* Build and collect a list of commands that were (re)built.
  public func _buildTest() throws -> Set<String> {
    let pipe = Pipe()

    do {
      try buildImpl { process in
        process.standardOutput = pipe
      }
    } catch TibsBuilder.Error.buildFailure(let tc, let ts) {
      let out = String(data: pipe.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8)!
      print(out)
      throw TibsBuilder.Error.buildFailure(tc, exitCode: ts)
    }

    let out = String(data: pipe.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8)!
    var rest = out.startIndex..<out.endIndex

    var result = Set<String>()
    while let range = out.range(of: "] Indexing ", range: rest) {
      let srcEnd = out[range.upperBound...].index(of: "\n") ?? rest.upperBound
      let target = String(out[range.upperBound ..< srcEnd])
      result.insert(target.trimmingCharacters(in: CharacterSet(charactersIn: "'\"")))
      rest = srcEnd..<rest.upperBound
    }

    return result
  }

  func buildImpl(processCustomization: (Process) -> () = { _ in }) throws {
    guard let ninja = toolchain.ninja?.path else {
      throw Error.noNinjaBinaryConfigured
    }

    let p = Process()
    p.launchPath = ninja
    p.arguments = ["-C", buildRoot.path]

    processCustomization(p)

    p.launch()
    p.waitUntilExit()
    if p.terminationReason != .exit || p.terminationStatus != 0 {
      throw Error.buildFailure(p.terminationReason, exitCode: p.terminationStatus)
    }
  }
}

extension TibsBuilder {

  // MARK: Serialization

  public var compilationDatabase: JSONCompilationDatabase {
    var commands = [JSONCompilationDatabase.Command]()
    for target in targets {
      if let module = target.swiftModule {
        var args = [toolchain.swiftc.path]
        args += module.sources.map { $0.path }
        args += module.importPaths.flatMap { ["-I", $0] }
        args += [
          "-module-name", module.name,
          "-index-store-path", indexstore.path,
          "-index-ignore-system-modules",
          "-output-file-map", module.outputFileMapPath,
          "-emit-module",
          "-emit-module-path", module.emitModulePath,
          "-emit-dependencies",
          "-pch-output-dir", "pch",
          "-module-cache-path", "ModuleCache"
        ]
        args += module.emitHeaderPath.map { [
          "-emit-objc-header",
          "-emit-objc-header-path", $0
          ] } ?? []
        args += module.bridgingHeader.map { ["-import-objc-header", $0.path] } ?? []
        args += module.extraArgs

        // FIXME: handle via 'directory' field?
        args += ["-working-directory", buildRoot.path]

        module.sources.forEach { sourceFile in
          commands.append(JSONCompilationDatabase.Command(
            directory: buildRoot.path,
            file: sourceFile.path,
            arguments: args))
        }
      }

      for tu in target.clangTUs {
        var args = [
          toolchain.clang.path,
          "-fsyntax-only",
          tu.source.path
        ]
        args += tu.importPaths.flatMap { ["-I", $0] }
        args += [
          "-index-store-path", "index",
          "-index-ignore-system-symbols",
          "-fmodules",
          "-fmodules-cache-path=ModuleCache",
          "-MMD", "-MF", "\(tu.outputPath).d",
          "-o", tu.outputPath,
        ]
        args += tu.extraArgs

        commands.append(JSONCompilationDatabase.Command(
          directory: buildRoot.path,
          file: tu.source.path,
          arguments: args))
      }
    }

    return JSONCompilationDatabase(commands: commands)
  }

  public func writeBuildFiles() throws {
    try ninja.write(
      to: buildRoot.appendingPathComponent("build.ninja", isDirectory: false),
      atomically: false,
      encoding: .utf8)

    let encoder = JSONEncoder()
    if #available(macOS 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
      encoder.outputFormatting = .sortedKeys
    }

    let compdb = try encoder.encode(compilationDatabase)
    try compdb.write(
      to: buildRoot.appendingPathComponent("compile_commands.json", isDirectory: false))
    for target in targets {
      if let module = target.swiftModule {
        let ofm = try encoder.encode(module.outputFileMap)
        try ofm.writeIfChanged(
          to: buildRoot.appendingPathComponent(module.outputFileMapPath, isDirectory: false))
      }
    }
  }

  public var ninja: String {
    var result = ""
    writeNinja(to: &result)
    return result
  }

  public func writeNinja<Output: TextOutputStream>(to stream: inout Output) {
    writeNinjaHeader(to: &stream)
    stream.write("\n\n")
    writeNinjaRules(to: &stream)
    stream.write("\n\n")
    for target in targets {
      writeNinjaSnippet(for: target, to: &stream)
      stream.write("\n\n")
    }
  }

  public func writeNinjaHeader<Output: TextOutputStream>(to stream: inout Output) {
    stream.write("""
      # Generated by tibs. DO NOT EDIT!
      ninja_required_version = 1.5
      """)
  }

  public func writeNinjaRules<Output: TextOutputStream>(to stream: inout Output) {
    stream.write("""
      rule swiftc_index
        description = Indexing Swift Module $MODULE_NAME
        command = \(toolchain.swiftc.path) $in $IMPORT_PATHS -module-name $MODULE_NAME \
          -index-store-path index -index-ignore-system-modules \
          -output-file-map $OUTPUT_FILE_MAP \
          -emit-module -emit-module-path $MODULE_PATH -emit-dependencies \
          -pch-output-dir pch -module-cache-path ModuleCache \
          $EMIT_HEADER $BRIDGING_HEADER $EXTRA_ARGS \
          && \(toolchain.tibs.path) swift-deps-merge $out $DEP_FILES > $out.d
        depfile = $out.d
        deps = gcc
        restat = 1 # Swift doesn't rewrite modules that haven't changed

      rule cc_index
        description = Indexing $in
        command = \(toolchain.clang.path) -fsyntax-only $in $IMPORT_PATHS -index-store-path index -index-ignore-system-symbols -fmodules -fmodules-cache-path=ModuleCache -MMD -MF $OUTPUT_NAME.d -o $out $EXTRA_ARGS && touch $out
        depfile = $out.d
        deps = gcc
      """)
  }

  public func writeNinjaSnippet<Output: TextOutputStream>(for target: TibsResolvedTarget, to stream: inout Output) {
    if let module = target.swiftModule {
      writeNinjaSnippet(for: module, to: &stream)
      stream.write("\n\n")
    }
    for tu in target.clangTUs {
      writeNinjaSnippet(for: tu, to: &stream)
      stream.write("\n\n")
    }
  }

  public func writeNinjaSnippet<Output: TextOutputStream>(for module: TibsResolvedTarget.SwiftModule, to stream: inout Output) {
    // FIXME: the generated -Swift.h header should be considered an output, but ninja does not
    // support multiple outputs when using gcc-style .d files.
    let outputs = [module.emitModulePath, /*module.emitHeaderPath*/]
    // FIXME: some of these are deleted by the compiler!?
    // outputs += target.outputFileMap.allOutputs

    var deps = module.moduleDeps
    deps.append(module.outputFileMapPath)
    deps.append(toolchain.swiftc.path)
    if let bridgingHeader = module.bridgingHeader {
      deps.append(bridgingHeader.path)
    }

    stream.write("""
      build \(outputs.joined(separator: " ")) : \
      swiftc_index \(module.sources.map { $0.path }.joined(separator: " ")) \
      | \(deps.joined(separator: " "))
        MODULE_NAME = \(module.name)
        MODULE_PATH = \(module.emitModulePath)
        IMPORT_PATHS = \(module.importPaths.map { "-I \($0)" }.joined(separator: " "))
        BRIDGING_HEADER = \(module.bridgingHeader.map { "-import-objc-header \($0.path)" } ?? "")
        EMIT_HEADER = \(module.emitHeaderPath.map { "-emit-objc-header -emit-objc-header-path \($0)" } ?? "")
        EXTRA_ARGS = \(module.extraArgs.joined(separator: " "))
        DEP_FILES = \(module.outputFileMap.values.compactMap { $0.dependencies }.joined(separator: " "))
        OUTPUT_FILE_MAP = \(module.outputFileMapPath)
      """)
  }

  public func writeNinjaSnippet<Output: TextOutputStream>(for tu: TibsResolvedTarget.ClangTU, to stream: inout Output) {

    stream.write("""
      build \(tu.outputPath): \
      cc_index \(tu.source.path) | \(toolchain.clang.path) \(tu.generatedHeaderDep ?? "")
        IMPORT_PATHS = \(tu.importPaths.map { "-I \($0)" }.joined(separator: " "))
        OUTPUT_NAME = \(tu.outputPath)
        EXTRA_ARGS = \(tu.extraArgs.joined(separator: " "))
      """)
  }
}
