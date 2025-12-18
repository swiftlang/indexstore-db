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

  public var indexOutputPaths: [URL] {
    return targets.flatMap { $0.indexOutputPaths }.map { buildRoot.appendingPathComponent($0, isDirectory: false) }
  }

  public enum Error: Swift.Error {
    case duplicateTarget(String)
    case unknownDependency(String, declaredIn: String)
    case buildFailure(Process.TerminationReason, exitCode: Int32, stdout: String?, stderr: String?)
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

      let swiftFlags = expandMagicVariables(targetDesc.swiftFlags ?? [], sourceRoot.path, buildRoot.path)
      let clangFlags = expandMagicVariables(targetDesc.clangFlags ?? [], sourceRoot.path, buildRoot.path)

      let swiftSources = sources.filter { $0.pathExtension == "swift" }
      let clangSources = sources.filter { $0.pathExtension != "swift" }

      var swiftModule: TibsResolvedTarget.SwiftModule? = nil
      if !swiftSources.isEmpty {
        var outputFileMap = OutputFileMap()
        for source in swiftSources {
          let basename = source.lastPathComponent
          outputFileMap[source.path] = OutputFileMap.Entry(
            object: "\(name)-\(basename).o",
            swiftmodule: "\(name)-\(basename).swiftmodule~partial",
            swiftdoc: "\(name)-\(basename).swiftdoc~partial",
            dependencies: "\(name)-\(basename).d"
          )
        }

        swiftModule = TibsResolvedTarget.SwiftModule(
          name: name,
          extraArgs: swiftFlags + clangFlags.flatMap { ["-Xcc", $0] },
          sources: swiftSources,
          emitModulePath: "\(name).swiftmodule",
          emitHeaderPath: clangSources.isEmpty ? nil : "\(name)-Swift.h",
          outputFileMap: outputFileMap,
          bridgingHeader: bridgingHeader,
          moduleDeps: targetDesc.dependencies?.map { "\($0).swiftmodule" } ?? [],
          sdk: TibsBuilder.defaultSDKPath
        )
      }

      var clangTUs: [TibsResolvedTarget.ClangTU] = []
      for source in clangSources {
        let cu = TibsResolvedTarget.ClangTU(
          extraArgs: clangFlags,
          source: source,
          importPaths: [ /*buildRoot*/".", sourceRoot.path],
          // FIXME: this should be the -Swift.h file, but ninja doesn't support
          // having multiple output files when using gcc-style dependencies, so
          // use the .swiftmodule.
          generatedHeaderDep: swiftSources.isEmpty ? nil : "\(name).swiftmodule",
          outputPath: "\(name)-\(source.lastPathComponent).o"
        )
        clangTUs.append(cu)
      }

      let target = TibsResolvedTarget(
        name: name,
        swiftModule: swiftModule,
        clangTUs: clangTUs,
        dependencies: targetDesc.dependencies ?? []
      )

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

  /// Build, optionally specifying specific targets.
  public func build(targets: [String] = []) throws {
    try buildImpl(targets: targets)
  }

  /// Build the dependencies of the given targets without building these targets themselves.
  public func build(dependenciesOfTargets targets: [String]) throws {
    let deps = Array(Set(targets.flatMap { self.targetsByName[$0]!.dependencies }))
    if !deps.isEmpty {
      try build(targets: deps)
    }
  }

  /// *For Testing* Build and collect a list of commands that were (re)built.
  public func _buildTest(targets: [String] = []) throws -> Set<String> {
    let out = try buildImpl(targets: targets)
    var rest = out.startIndex..<out.endIndex

    var result = Set<String>()
    while let range = out.range(of: "] Indexing ", range: rest) {
      let srcEnd = out[range.upperBound...].firstIndex(where: { $0.isNewline }) ?? rest.upperBound
      let target = String(out[range.upperBound..<srcEnd])
      result.insert(target.trimmingCharacters(in: CharacterSet(charactersIn: "'\"")))
      rest = srcEnd..<rest.upperBound
    }

    return result
  }

  /// *For Testing* Build and collect a list of commands that were (re)built.
  public func _buildTest(dependenciesOfTargets targets: [String]) throws -> Set<String> {
    let deps = Array(Set(targets.flatMap { self.targetsByName[$0]!.dependencies }))
    if !deps.isEmpty {
      return try _buildTest(targets: deps)
    }
    return []
  }

  @discardableResult
  func buildImpl(targets: [String]) throws -> String {
    guard let ninja = toolchain.ninja?.path else {
      throw Error.noNinjaBinaryConfigured
    }

    do {
      return try Process.tibs_checkNonZeroExit(arguments: [ninja, "-C", buildRoot.path] + targets)
    } catch Process.TibsProcessError.nonZeroExit(let reason, let code, let stdout, let stderr) {
      throw Error.buildFailure(reason, exitCode: code, stdout: stdout, stderr: stderr)
    }
  }
}

func expandMagicVariables(_ arguments: [String], _ sourceRoot: String, _ buildRoot: String) -> [String] {
  return arguments.map { arg in
    var expanded = arg
    expanded = expanded.replacingOccurrences(of: "$SRC_DIR", with: sourceRoot)
    expanded = expanded.replacingOccurrences(of: "$BUILD_DIR", with: buildRoot)
    return expanded
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
          "-module-cache-path", "ModuleCache",
          "-c",
        ]
        args +=
          module.emitHeaderPath.map {
            [
              "-emit-objc-header",
              "-emit-objc-header-path", $0,
            ]
          } ?? []
        args += module.bridgingHeader.map { ["-import-objc-header", $0.path] } ?? []
        args += module.sdk.map { ["-sdk", $0] } ?? []
        args += module.extraArgs

        // FIXME: handle via 'directory' field?
        args += ["-working-directory", buildRoot.path]

        module.sources.forEach { sourceFile in
          commands.append(
            JSONCompilationDatabase.Command(
              directory: buildRoot.path,
              file: sourceFile.path,
              arguments: args
            )
          )
        }
      }

      for tu in target.clangTUs {
        var args = [
          toolchain.clang.path,
          "-fsyntax-only",
          tu.source.path,
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

        commands.append(
          JSONCompilationDatabase.Command(
            directory: buildRoot.path,
            file: tu.source.path,
            arguments: args
          )
        )
      }
    }

    return JSONCompilationDatabase(commands: commands)
  }

  public func writeBuildFiles() throws {
    try ninja.write(
      to: buildRoot.appendingPathComponent("build.ninja", isDirectory: false),
      atomically: false,
      encoding: .utf8
    )

    let encoder = JSONEncoder()
    if #available(macOS 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
      encoder.outputFormatting = .sortedKeys
    }

    let compdb = try encoder.encode(compilationDatabase)
    try compdb.write(
      to: buildRoot.appendingPathComponent("compile_commands.json", isDirectory: false)
    )
    for target in targets {
      if let module = target.swiftModule {
        let ofm = try encoder.encode(module.outputFileMap)
        try ofm.writeIfChanged(
          to: buildRoot.appendingPathComponent(module.outputFileMapPath, isDirectory: false)
        )
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
    stream.write(
      """
      # Generated by tibs. DO NOT EDIT!
      ninja_required_version = 1.5
      """
    )
  }

  public func writeNinjaRules<Output: TextOutputStream>(to stream: inout Output) {
    #if os(Windows)
    let callCmd = "cmd.exe /C "
    let copyCmd = "copy NUL $out"
    #else
    let callCmd = ""
    let copyCmd = "touch $out"
    #endif
    // FIXME: rdar://83355591 avoid -c, since we don't want to spend time writing .o files.
    let swiftIndexCommand =
      callCmd + """
        \(escapeCommand([toolchain.swiftc.path])) $in $IMPORT_PATHS -module-name $MODULE_NAME \
         -index-store-path index -index-ignore-system-modules \
         -output-file-map $OUTPUT_FILE_MAP \
        -emit-module -emit-module-path $MODULE_PATH -emit-dependencies \
        -pch-output-dir pch -module-cache-path ModuleCache \
        -c \
        $EMIT_HEADER $BRIDGING_HEADER $SDK $EXTRA_ARGS \
        && \(toolchain.tibs.path) swift-deps-merge $out $DEP_FILES > $out.d
        """

    let ccIndexCommand =
      callCmd + """
        \(escapeCommand([toolchain.clang.path])) -fsyntax-only $in $IMPORT_PATHS -index-store-path index \
        -index-ignore-system-symbols -fmodules -fmodules-cache-path=ModuleCache \
        -MMD -MF $OUTPUT_NAME.d -o $out $EXTRA_ARGS && \(copyCmd)
        """
    stream.write(
      """
      rule swiftc_index
        description = Indexing Swift Module $MODULE_NAME
        command = \(swiftIndexCommand)
        depfile = $out.d
        deps = gcc
        restat = 1 # Swift doesn't rewrite modules that haven't changed

      rule cc_index
        description = Indexing $in
        command = \(ccIndexCommand)
        depfile = $out.d
        deps = gcc
      """
    )
  }

  public func writeNinjaSnippet<Output: TextOutputStream>(for target: TibsResolvedTarget, to stream: inout Output) {
    var outputs: [String] = []
    if let module = target.swiftModule {
      let out = writeNinjaSnippet(for: module, to: &stream)
      outputs.append(contentsOf: out)
      stream.write("\n\n")
    }
    for tu in target.clangTUs {
      let out = writeNinjaSnippet(for: tu, to: &stream)
      outputs.append(contentsOf: out)
      stream.write("\n\n")
    }
    stream.write(
      """
      build \(target.name): phony \(escapePath(path: outputs.joined(separator: " ")))


      """
    )
  }

  /// - Returns: the list of outputs.
  public func writeNinjaSnippet<Output: TextOutputStream>(
    for module: TibsResolvedTarget.SwiftModule,
    to stream: inout Output
  ) -> [String] {
    // FIXME: the generated -Swift.h header should be considered an output, but ninja does not
    // support multiple outputs when using gcc-style .d files.
    let outputs = [module.emitModulePath /*module.emitHeaderPath*/]
    // FIXME: some of these are deleted by the compiler!?
    // outputs += target.outputFileMap.allOutputs

    var deps = module.moduleDeps
    deps.append(module.outputFileMapPath)
    deps.append(toolchain.swiftc.path)
    if let bridgingHeader = module.bridgingHeader {
      deps.append(bridgingHeader.path)
    }

    stream.write(
      """
      build \(escapePath(path: outputs.joined(separator: " "))) : \
      swiftc_index \(module.sources.map { escapePath(path: $0.path) }.joined(separator: " ")) \
      | \(escapePath(path: deps.joined(separator: " ")))
        MODULE_NAME = \(module.name)
        MODULE_PATH = \(module.emitModulePath)
        IMPORT_PATHS = \(module.importPaths.map { "-I \($0)" }.joined(separator: " "))
        BRIDGING_HEADER = \(module.bridgingHeader.map { "-import-objc-header \($0.path)" } ?? "")
        EMIT_HEADER = \(module.emitHeaderPath.map { "-emit-objc-header -emit-objc-header-path \($0)" } ?? "")
        EXTRA_ARGS = \(module.extraArgs.joined(separator: " "))
        DEP_FILES = \(module.outputFileMap.values.compactMap { $0.dependencies }.joined(separator: " "))
        OUTPUT_FILE_MAP = \(module.outputFileMapPath)
        SDK = \(module.sdk.map { "-sdk \($0)" } ?? "")
      """
    )

    return outputs
  }

  /// - Returns: the list of outputs.
  public func writeNinjaSnippet<Output: TextOutputStream>(
    for tu: TibsResolvedTarget.ClangTU,
    to stream: inout Output
  ) -> [String] {

    stream.write(
      """
      build \(escapePath(path: tu.outputPath)): \
      cc_index \(escapePath(path: tu.source.path)) | \(escapePath(path: toolchain.clang.path)) \(tu.generatedHeaderDep ?? "")
        IMPORT_PATHS = \(tu.importPaths.map { "-I \($0)" }.joined(separator: " "))
        OUTPUT_NAME = \(tu.outputPath)
        EXTRA_ARGS = \(tu.extraArgs.joined(separator: " "))
      """
    )

    return [tu.outputPath]
  }
}

extension TibsBuilder {

  /// The default sdk path to use.
  public static var defaultSDKPath: String? = {
    #if os(macOS)
    return xcrunSDKPath()
    #else
    return ProcessInfo.processInfo.environment["SDKROOT"]
    #endif
  }()
}

func xcrunSDKPath() -> String {
  var path = try! Process.tibs_checkNonZeroExit(arguments: ["/usr/bin/xcrun", "--show-sdk-path", "--sdk", "macosx"])
  if path.last == "\n" {
    path = String(path.dropLast())
  }
  return path
}

#if os(Windows)
func quoteWindowsCommandLine(_ commandLine: [String]) -> String {
  func quoteWindowsCommandArg(arg: String) -> String {
    // Windows escaping, adapted from Daniel Colascione's "Everyone quotes
    // command line arguments the wrong way" - Microsoft Developer Blog
    if !arg.contains(where: { " \t\n\"".contains($0) }) {
      return arg
    }

    // To escape the command line, we surround the argument with quotes. However
    // the complication comes due to how the Windows command line parser treats
    // backslashes (\) and quotes (")
    //
    // - \ is normally treated as a literal backslash
    //     - e.g. foo\bar\baz => foo\bar\baz
    // - However, the sequence \" is treated as a literal "
    //     - e.g. foo\"bar => foo"bar
    //
    // But then what if we are given a path that ends with a \? Surrounding
    // foo\bar\ with " would be "foo\bar\" which would be an unterminated string

    // since it ends on a literal quote. To allow this case the parser treats:
    //
    // - \\" as \ followed by the " metachar
    // - \\\" as \ followed by a literal "
    // - In general:
    //     - 2n \ followed by " => n \ followed by the " metachar
    //     - 2n+1 \ followed by " => n \ followed by a literal "
    var quoted = "\""
    var unquoted = arg.unicodeScalars

    while !unquoted.isEmpty {
      guard let firstNonBackslash = unquoted.firstIndex(where: { $0 != "\\" }) else {
        // String ends with a backslash e.g. foo\bar\, escape all the backslashes
        // then add the metachar " below
        let backslashCount = unquoted.count
        quoted.append(String(repeating: "\\", count: backslashCount * 2))
        break
      }
      let backslashCount = unquoted.distance(from: unquoted.startIndex, to: firstNonBackslash)
      if unquoted[firstNonBackslash] == "\"" {
        // This is  a string of \ followed by a " e.g. foo\"bar. Escape the
        // backslashes and the quote
        quoted.append(String(repeating: "\\", count: backslashCount * 2 + 1))
        quoted.append(String(unquoted[firstNonBackslash]))
      } else {
        // These are just literal backslashes
        quoted.append(String(repeating: "\\", count: backslashCount))
        quoted.append(String(unquoted[firstNonBackslash]))
      }
      // Drop the backslashes and the following character
      unquoted.removeFirst(backslashCount + 1)
    }
    quoted.append("\"")
    return quoted
  }
  return commandLine.map(quoteWindowsCommandArg).joined(separator: " ")
}
#endif

func escapeCommand(_ args: [String]) -> String {
  let escaped: String
  #if os(Windows)
  escaped = quoteWindowsCommandLine(args)
  #else
  escaped = args.joined(separator: " ")
  #endif
  return escapePath(path: escaped)
}

func escapePath(path: String) -> String {
  // Ninja escapes using $, this only matters during build lines
  // since those are terminated by a :
  return path.replacingOccurrences(of: ":", with: "$:")
}
