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

import IndexStoreDB
import ISDBTibs
import Foundation
import XCTest

/// Test workspace for a project using the tibs build system, providing convenient access to the
/// sources, source code index, and build system.
///
/// Generally, a test case will construct a TibsTestWorkspace using one of the methods on
/// XCTestCase.
public final class TibsTestWorkspace {

  /// The directory containing the original, unmodified project.
  public let projectDir: URL

  /// A test-specific directory that we can put temporary files into.
  public let tmpDir: URL

  /// Whether to automatically remove `tmpDir` during `deinit`.
  public var removeTmpDir: Bool

  /// Whether the sources can be modified during this test. If this is true, we can call `edit()`.
  public let mutableSources: Bool

  /// The source files used by the test. If `mutableSources == false`, they are located in
  /// `projectDir`. Otherwise, they are copied to a temporary location.
  public let sources: TestSources

  /// The current resolved project and builder.
  public var builder: TibsBuilder

  /// The source code index.
  public var index: IndexStoreDB

  /// The wrapper delegate.
  ///
  /// Use `delegate` to set the underlying delegate to forward to.
  var wrapperDelegate: ForwardingIndexDelegate = ForwardingIndexDelegate()

  /// The (underlying) index delegate.
  public var delegate: IndexDelegate? {
    get { wrapperDelegate.delegate }
    set { wrapperDelegate.delegate = newValue }
  }

  /// Creates a tibs test workspace for a given project with immutable sources and a build directory
  /// that can persist across test runs (typically inside the main project build directory).
  ///
  /// The `edit()` method is disallowed.
  ///
  /// * parameters:
  ///   * immutableProjectDir: The directory containing the project.
  ///   * persistentBuildDir: The directory to build in.
  ///   * tmpDir: A test-specific directory that we can put temporary files into.
  ///   * removeTmpDir: Whether to automatically remove `tmpDir` during `deinit`.
  ///   * toolchain: The toolchain to use for building and indexing.
  ///
  /// * throws: If there are any file system errors.
  public init(
    immutableProjectDir: URL,
    persistentBuildDir: URL,
    tmpDir: URL,
    removeTmpDir: Bool = true,
    useExplicitOutputUnits: Bool = false,
    toolchain: TibsToolchain) throws
  {
    self.projectDir = immutableProjectDir
    self.tmpDir = tmpDir
    self.mutableSources = false
    self.removeTmpDir = removeTmpDir

    let fm = FileManager.default
    _ = try? fm.removeItem(at: tmpDir)

    try fm.tibs_createDirectoryWithIntermediates(at: persistentBuildDir)
    let databaseDir = tmpDir

    self.sources = try TestSources(rootDirectory: projectDir)

    let manifest = try TibsManifest.load(projectRoot: projectDir)
    builder = try TibsBuilder(
      manifest: manifest,
      sourceRoot: projectDir,
      buildRoot: persistentBuildDir,
      toolchain: toolchain)

    try builder.writeBuildFiles()

    let libIndexStore = try IndexStoreLibrary(dylibPath: toolchain.libIndexStore.path)

    self.index = try IndexStoreDB(
      storePath: builder.indexstore.path,
      databasePath: databaseDir.path,
      library: libIndexStore,
      delegate: wrapperDelegate,
      useExplicitOutputUnits: useExplicitOutputUnits,
      listenToUnitEvents: false)
  }

  /// Creates a tibs test workspace and copies the sources to a temporary location so that they can
  /// be modified (using `edit()`) and rebuilt during the test.
  ///
  /// * parameters:
  ///   * projectDir: The directory containing the project. The sources will be copied to a
  ///       temporary location.
  ///   * tmpDir: A test-specific directory that we can put temporary files into.
  ///   * removeTmpDir: Whether to automatically remove `tmpDir` during `deinit`.
  ///   * toolchain: The toolchain to use for building and indexing.
  ///
  /// * throws: If there are any file system errors.
  public init(
    projectDir: URL,
    tmpDir: URL,
    removeTmpDir: Bool = true,
    useExplicitOutputUnits: Bool = false,
    toolchain: TibsToolchain) throws
  {
    self.projectDir = projectDir
    self.tmpDir = tmpDir
    self.mutableSources = true
    self.removeTmpDir = removeTmpDir

    let fm = FileManager.default
    _ = try? fm.removeItem(at: tmpDir)

    let buildDir = tmpDir.appendingPathComponent("build", isDirectory: true)
    try fm.tibs_createDirectoryWithIntermediates(at: buildDir)
    let sourceDir = tmpDir.appendingPathComponent("src", isDirectory: true)
    try fm.copyItem(at: projectDir, to: sourceDir)
    let databaseDir = tmpDir.appendingPathComponent("db", isDirectory: true)

    self.sources = try TestSources(rootDirectory: sourceDir)

    let manifest = try TibsManifest.load(projectRoot: projectDir)
    builder = try TibsBuilder(
      manifest: manifest,
      sourceRoot: sourceDir,
      buildRoot: buildDir,
      toolchain: toolchain)

    try builder.writeBuildFiles()

    let libIndexStore = try IndexStoreLibrary(dylibPath: toolchain.libIndexStore.path)

    self.index = try IndexStoreDB(
      storePath: builder.indexstore.path,
      databasePath: databaseDir.path,
      library: libIndexStore,
      delegate: wrapperDelegate,
      useExplicitOutputUnits: useExplicitOutputUnits,
      listenToUnitEvents: false)
  }

  deinit {
    if removeTmpDir {
      _ = try? FileManager.default.removeItem(atPath: tmpDir.path)
    }
  }

  public func buildAndIndex() throws {
     try builder.build()
     index.pollForUnitChangesAndWait()
   }

  public func testLoc(_ name: String) -> TestLocation { sources.locations[name]! }

  /// Perform a group of edits to the project sources and optionally rebuild.
  public func edit(
    rebuild: Bool = false,
    _ block: (inout TestSources.ChangeBuilder, _ current: SourceFileCache) throws -> ()) throws
  {
    precondition(mutableSources, "tried to edit in immutable workspace")
    builder.toolchain.sleepForTimestamp()

    let cache = sources.sourceCache
    _ = try sources.edit { builder in
      try block(&builder, cache)
    }
    // FIXME: support editing the project.json and update the build settings.
    if rebuild {
      try buildAndIndex()
    }
  }
}

extension XCTestCase {

  /// Constructs an immutable TibsTestWorkspace for the given test from INPUTS.
  ///
  /// The resulting workspace will not allow edits.
  ///
  /// * parameters:
  ///   * name: The name of the test, which is its path relative to INPUTS.
  /// * returns: An immutable TibsTestWorkspace, or nil and prints a warning if toolchain does not
  ///   support this test.
  public func staticTibsTestWorkspace(
    name: String,
    useExplicitOutputUnits: Bool = false,
    testFile: String = #file
  ) throws -> TibsTestWorkspace? {
    let testDirName = testDirectoryName

    let toolchain = TibsToolchain.testDefault

    let workspace = try TibsTestWorkspace(
      immutableProjectDir: inputsDirectory(testFile: testFile)
        .appendingPathComponent(name, isDirectory: true),
      persistentBuildDir: XCTestCase.productsDirectory
        .appendingPathComponent("isdb-tests/\(testDirName)", isDirectory: true),
      tmpDir: URL(fileURLWithPath: NSTemporaryDirectory())
        .appendingPathComponent("isdb-test-data/\(testDirName)", isDirectory: true),
      useExplicitOutputUnits: useExplicitOutputUnits,
      toolchain: toolchain)

    if workspace.builder.targets.contains(where: { target in !target.clangTUs.isEmpty })
      && !toolchain.clangHasIndexSupport {
      fputs("warning: skipping test because '\(toolchain.clang.path)' does not have indexstore " +
            "support; use swift-clang\n", stderr)
      return nil
    }

    return workspace
  }

  /// Constructs a mutable TibsTestWorkspace for the given test from INPUTS.
  ///
  /// The resulting workspace allow edits.
  ///
  /// * parameters:
  ///   * name: The name of the test, which is its path relative to INPUTS.
  /// * returns: An immutable TibsTestWorkspace, or nil and prints a warning if toolchain does not
  ///   support this test.
  public func mutableTibsTestWorkspace(
    name: String,
    testFile: String = #file
  ) throws -> TibsTestWorkspace? {
    let testDirName = testDirectoryName

    let toolchain = TibsToolchain.testDefault

    let workspace = try TibsTestWorkspace(
      projectDir: inputsDirectory(testFile: testFile)
        .appendingPathComponent(name, isDirectory: true),
      tmpDir: URL(fileURLWithPath: NSTemporaryDirectory())
        .appendingPathComponent("isdb-test-data/\(testDirName)", isDirectory: true),
      toolchain: toolchain)

    if workspace.builder.targets.contains(where: { target in !target.clangTUs.isEmpty })
      && !toolchain.clangHasIndexSupport {
      fputs("warning: skipping test because '\(toolchain.clang.path)' does not have indexstore " +
        "support; use swift-clang\n", stderr)
      return nil
    }

    return workspace
  }

  /// The path the the test INPUTS directory.
  public func inputsDirectory(testFile: String = #file) -> URL {
    return URL(fileURLWithPath: testFile)
      .deletingLastPathComponent()
      .deletingLastPathComponent()
      .appendingPathComponent("INPUTS", isDirectory: true)
  }

  /// The path to the built products directory.
  public static var productsDirectory: URL {
    #if os(macOS)
      for bundle in Bundle.allBundles where bundle.bundlePath.hasSuffix(".xctest") {
          return bundle.bundleURL.deletingLastPathComponent()
      }
      fatalError("couldn't find the products directory")
    #else
      return Bundle.main.bundleURL
    #endif
  }

  /// The name of this test, mangled for use as a directory.
  public var testDirectoryName: String {
    guard name.starts(with: "-[") else {
      return name
    }

    let className = name.dropFirst(2).prefix(while: { $0 != " " })
    let methodName = name[className.endIndex...].dropFirst().prefix(while: { $0 != "]"})
    return "\(className).\(methodName)"
  }
}

extension FileManager {

  /// Wrapper for Foundation.createDirectory that works around a spurious EEXIST
  /// if we race to create.
  public func tibs_createDirectoryWithIntermediates(at url: URL) throws
  {
#if os(macOS)
    try self.createDirectory(at: url, withIntermediateDirectories: true, attributes: nil)
#else
    var i = 0
    let maxRetries = 3

    while true {
      do {
        try self.createDirectory(at: url, withIntermediateDirectories: true, attributes: nil)
        return
      } catch let error as NSError where error.code == CocoaError.fileWriteFileExists.rawValue {
        // May be spurious if we raced.
        i += 1
        if i < maxRetries {
          continue
        } else {
          throw error
        }
      }
    }
#endif
  }
}
