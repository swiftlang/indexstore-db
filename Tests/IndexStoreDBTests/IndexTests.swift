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

import ISDBTestSupport
import IndexStoreDB
import XCTest

let defaultTimeout: TimeInterval = 30

final class IndexTests: XCTestCase {

  @discardableResult
  func expectation(for block: @escaping () -> Bool) -> XCTestExpectation {
    self.expectation(
      for: NSPredicate(block: { (_, _) -> Bool in
        return block()
      }),
      evaluatedWith: nil
    )
  }

  func waitForBlock(timeout: TimeInterval = defaultTimeout, _ block: @escaping () -> Bool) {
    let expect = self.expectation(for: block)
    self.wait(for: [expect], timeout: timeout)
  }

  func testBasic() throws {
    guard let ws = try staticTibsTestWorkspace(name: "proj1") else { return }
    let index = ws.index

    let usr = "s:4main1cyyF"
    let getOccs = { index.occurrences(ofUSR: usr, roles: [.reference, .definition]) }

    XCTAssertEqual(0, getOccs().count)

    try ws.buildAndIndex()

    let csym = Symbol(usr: usr, name: "c()", kind: .function, language: .swift)
    let asym = Symbol(usr: "s:4main1ayyF", name: "a()", kind: .function, language: .swift)

    let ccanon = SymbolOccurrence(
      symbol: csym,
      location: SymbolLocation(ws.testLoc("c"), moduleName: "main"),
      roles: [.definition, .canonical],
      symbolProvider: .clang,
      relations: []
    )

    let ccall = SymbolOccurrence(
      symbol: csym,
      location: SymbolLocation(ws.testLoc("c:call")),
      roles: [.reference, .call, .calledBy, .containedBy],
      symbolProvider: .clang,
      relations: [
        .init(symbol: asym, roles: [.calledBy, .containedBy])
      ]
    )

    checkOccurrences(
      getOccs(),
      ignoreRelations: false,
      allowAdditionalRoles: false,
      expected: [
        ccanon,
        ccall,
      ]
    )

    checkOccurrences(
      index.canonicalOccurrences(ofName: "c()"),
      ignoreRelations: false,
      expected: [
        ccanon
      ]
    )

    checkOccurrences(index.canonicalOccurrences(ofName: "c"), ignoreRelations: false, expected: [])

    checkOccurrences(
      index.canonicalOccurrences(
        containing: "c",
        anchorStart: true,
        anchorEnd: false,
        subsequence: false,
        ignoreCase: false
      ),
      ignoreRelations: false,
      expected: [ccanon]
    )

    checkOccurrences(
      index.canonicalOccurrences(
        containing: "c",
        anchorStart: true,
        anchorEnd: true,
        subsequence: false,
        ignoreCase: false
      ),
      ignoreRelations: false,
      expected: []
    )

    checkOccurrences(
      index.canonicalOccurrences(
        containing: "C",
        anchorStart: true,
        anchorEnd: false,
        subsequence: false,
        ignoreCase: true
      ),
      ignoreRelations: false,
      expected: [ccanon]
    )

    checkOccurrences(
      index.canonicalOccurrences(
        containing: "C",
        anchorStart: true,
        anchorEnd: false,
        subsequence: false,
        ignoreCase: false
      ),
      ignoreRelations: false,
      expected: []
    )

    checkOccurrences(
      index.occurrences(relatedToUSR: "s:4main1ayyF", roles: .calledBy),
      ignoreRelations: false,
      expected: [
        ccall,
        SymbolOccurrence(
          symbol: Symbol(usr: "s:4main1byyF", name: "b()", kind: .function, language: .swift),
          location: SymbolLocation(ws.testLoc("b:call")),
          roles: [.reference, .call, .calledBy, .containedBy],
          symbolProvider: .swift,
          relations: [
            .init(symbol: asym, roles: [.calledBy, .containedBy])
          ]
        ),
      ]
    )
  }

  func testMixedLangTarget() throws {
    guard let ws = try staticTibsTestWorkspace(name: "MixedLangTarget") else { return }
    try ws.buildAndIndex()
    let index = ws.index

    #if os(macOS)

    let cdecl = Symbol(usr: "c:objc(cs)C", name: "C", kind: .class, language: .objc)
    let cdeclOccs = index.occurrences(ofUSR: cdecl.usr, roles: .all)
    checkOccurrences(
      cdeclOccs,
      expected: [
        cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical], symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:def"), roles: .definition, symbolProvider: .clang),
        cdecl.with(language: .swift).at(ws.testLoc("C:ref:swift"), roles: .reference, symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:ref:e.mm"), roles: .reference, symbolProvider: .clang),
      ]
    )

    let cmethod = Symbol(usr: "c:objc(cs)C(im)method", name: "method", kind: .instanceMethod, language: .objc)
    let cmethodOccs = index.occurrences(ofUSR: cmethod.usr, roles: .all)
    checkOccurrences(
      cmethodOccs,
      expected: [
        cmethod.with(name: "method()", language: .swift).at(
          ws.testLoc("C.method:call:swift"),
          roles: [.call, .dynamic],
          symbolProvider: .swift
        ),
        cmethod.at(ws.testLoc("C.method:decl"), roles: .declaration, symbolProvider: .clang),
        cmethod.at(ws.testLoc("C.method:def"), roles: .definition, symbolProvider: .clang),
        cmethod.at(ws.testLoc("C.method:call:e.mm"), roles: [.call, .dynamic], symbolProvider: .clang),
      ]
    )
    #endif

    let ddecl = Symbol(usr: "c:@S@D", name: "D", kind: .class, language: .cxx)
    let dOccs = index.occurrences(ofUSR: ddecl.usr, roles: .all)
    checkOccurrences(
      dOccs,
      expected: [
        ddecl.at(ws.testLoc("D:def"), roles: .definition, symbolProvider: .clang),
        ddecl.at(ws.testLoc("D:ref"), roles: .reference, symbolProvider: .clang),
        ddecl.at(ws.testLoc("D:ref:e.mm"), roles: .reference, symbolProvider: .clang),
      ]
    )

    let bhdecl = Symbol(usr: "c:@F@bridgingHeader", name: "bridgingHeader", kind: .function, language: .c)
    let bridgingHeaderOccs = index.occurrences(ofUSR: bhdecl.usr, roles: .all)
    checkOccurrences(
      bridgingHeaderOccs,
      expected: [
        bhdecl.at(ws.testLoc("bridgingHeader:decl"), roles: .declaration, symbolProvider: .clang),
        bhdecl.with(name: "bridgingHeader()", language: .swift).at(
          ws.testLoc("bridgingHeader:call"),
          roles: .call,
          symbolProvider: .clang
        ),
      ]
    )
  }

  func testHermeticMixedLang() throws {
    guard let ws = try staticTibsTestWorkspace(name: "HermeticMixedLang") else { return }
    // Provide the reverse mappings to the one in the project.json.
    try ws.reinitIndexStore(
      waitUntilDoneInitializing: true,
      prefixMappings: [
        PathMapping(original: "/SRC_ROOT", replacement: ws.sources.rootDirectory.path),
        PathMapping(original: "/BUILD_ROOT", replacement: ws.builder.buildRoot.path),
      ]
    )
    try ws.buildAndIndex()
    let index = ws.index

    #if os(macOS)
    let cdecl = Symbol(usr: "c:objc(cs)C", name: "C", kind: .class, language: .objc)
    let cdeclOccs = index.occurrences(ofUSR: cdecl.usr, roles: .all)
    checkOccurrences(
      cdeclOccs,
      expected: [
        cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical], symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:def"), roles: .definition, symbolProvider: .clang),
        cdecl.with(language: .swift).at(ws.testLoc("C:ref:swift"), roles: .reference, symbolProvider: .swift),
      ]
    )

    let cmethod = Symbol(usr: "c:objc(cs)C(im)method", name: "method", kind: .instanceMethod, language: .objc)
    let cmethodOccs = index.occurrences(ofUSR: cmethod.usr, roles: .all)
    checkOccurrences(
      cmethodOccs,
      expected: [
        cmethod.with(name: "method()", language: .swift).at(
          ws.testLoc("C.method:call:swift"),
          roles: [.call, .dynamic],
          symbolProvider: .clang
        ),
        cmethod.at(ws.testLoc("C.method:decl"), roles: .declaration, symbolProvider: .clang),
        cmethod.at(ws.testLoc("C.method:def"), roles: .definition, symbolProvider: .clang),
      ]
    )
    #endif

    let bhdecl = Symbol(usr: "c:@F@bridgingHeader", name: "bridgingHeader", kind: .function, language: .c)
    let bridgingHeaderOccs = index.occurrences(ofUSR: bhdecl.usr, roles: .all)
    checkOccurrences(
      bridgingHeaderOccs,
      expected: [
        bhdecl.at(ws.testLoc("bridgingHeader:decl"), roles: .declaration, symbolProvider: .clang),
        bhdecl.with(name: "bridgingHeader()", language: .swift).at(
          ws.testLoc("bridgingHeader:call"),
          roles: .call,
          symbolProvider: .swift
        ),
      ]
    )
  }

  func testHermeticExplicitOutputUnits() throws {
    guard let ws = try staticTibsTestWorkspace(name: "HermeticMixedLang", useExplicitOutputUnits: true) else { return }
    // Provide the reverse mappings to the one in the project.json.
    try ws.reinitIndexStore(
      useExplicitOutputUnits: true,
      prefixMappings: [
        PathMapping(original: "/SRC_ROOT", replacement: ws.sources.rootDirectory.path),
        PathMapping(original: "/BUILD_ROOT", replacement: ws.builder.buildRoot.path),
      ]
    )
    try ws.buildAndIndex()
    let index = ws.index

    #if os(macOS)
    let cdecl = Symbol(usr: "c:objc(cs)C", name: "C", kind: .class, language: .objc)
    let getOccs = { index.occurrences(ofUSR: cdecl.usr, roles: .all) }

    // Output units are not set yet.
    XCTAssertEqual(0, getOccs().count)
    #endif

    // We must use the canonical output paths, not the local ones.
    let indexOutputPaths = ws.builder.indexOutputPaths.map {
      $0.path.replacingOccurrences(of: ws.builder.buildRoot.path, with: "/BUILD_ROOT")
    }
    index.addUnitOutFilePaths(indexOutputPaths, waitForProcessing: true)

    // The bridging header is referenced as a PCH unit dependency, make sure we can see the data.
    let bhdecl = Symbol(usr: "c:@F@bridgingHeader", name: "bridgingHeader", kind: .function, language: .c)
    let bridgingHeaderOccs = index.occurrences(ofUSR: bhdecl.usr, roles: .all)
    checkOccurrences(
      bridgingHeaderOccs,
      expected: [
        bhdecl.at(ws.testLoc("bridgingHeader:decl"), roles: .declaration, symbolProvider: .clang),
        bhdecl.with(name: "bridgingHeader()", language: .swift).at(
          ws.testLoc("bridgingHeader:call"),
          roles: .call,
          symbolProvider: .swift
        ),
      ]
    )
    #if os(macOS)
    checkOccurrences(
      getOccs(),
      expected: [
        cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical], symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:def"), roles: .definition, symbolProvider: .clang),
        cdecl.with(language: .swift).at(ws.testLoc("C:ref:swift"), roles: .reference, symbolProvider: .swift),
      ]
    )

    let outUnitASwift = try XCTUnwrap(indexOutputPaths.first { $0.hasSuffix("-a.swift.o") })
    index.removeUnitOutFilePaths([outUnitASwift], waitForProcessing: true)
    checkOccurrences(
      getOccs(),
      expected: [
        cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical], symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:def"), roles: .definition, symbolProvider: .clang),
      ]
    )
    #endif
  }

  /// Same as `testHermeticExplicitOutputUnits` but with remappings to make all paths relative.
  func testHermeticRelativePathExplicitOutputUnits() throws {
    guard let ws = try staticTibsTestWorkspace(name: "HermeticMixedLangRelativePaths", useExplicitOutputUnits: true)
    else { return }
    // Provide the reverse mappings to the one in the project.json.
    try ws.reinitIndexStore(
      useExplicitOutputUnits: true,
      prefixMappings: [
        PathMapping(original: ".", replacement: ws.sources.rootDirectory.path)
      ]
    )
    try ws.buildAndIndex()
    let index = ws.index

    #if os(macOS)
    let cdecl = Symbol(usr: "c:objc(cs)C", name: "C", kind: .class, language: .objc)
    let getOccs = { index.occurrences(ofUSR: cdecl.usr, roles: .all) }

    // Output units are not set yet.
    XCTAssertEqual(0, getOccs().count)
    #endif

    // We must use the canonical output paths, not the local ones.
    let indexOutputPaths = ws.builder.indexOutputPaths.map {
      $0.path.replacingOccurrences(of: ws.builder.buildRoot.path, with: ".")
    }
    index.addUnitOutFilePaths(indexOutputPaths, waitForProcessing: true)

    // The bridging header is referenced as a PCH unit dependency, make sure we can see the data.
    let bhdecl = Symbol(usr: "c:@F@bridgingHeader", name: "bridgingHeader", kind: .function, language: .c)
    let bridgingHeaderOccs = index.occurrences(ofUSR: bhdecl.usr, roles: .all)
    checkOccurrences(
      bridgingHeaderOccs,
      expected: [
        bhdecl.at(ws.testLoc("bridgingHeader:decl"), roles: .declaration, symbolProvider: .clang),
        bhdecl.with(name: "bridgingHeader()", language: .swift).at(
          ws.testLoc("bridgingHeader:call"),
          roles: .call,
          symbolProvider: .clang
        ),
      ]
    )
    #if os(macOS)
    checkOccurrences(
      getOccs(),
      expected: [
        cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical], symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:def"), roles: .definition, symbolProvider: .clang),
        cdecl.with(language: .swift).at(ws.testLoc("C:ref:swift"), roles: .reference, symbolProvider: .swift),
      ]
    )

    let outUnitASwift = try XCTUnwrap(indexOutputPaths.first { $0.hasSuffix("-a.swift.o") })
    index.removeUnitOutFilePaths([outUnitASwift], waitForProcessing: true)
    checkOccurrences(
      getOccs(),
      expected: [
        cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical], symbolProvider: .clang),
        cdecl.at(ws.testLoc("C:def"), roles: .definition, symbolProvider: .clang),
      ]
    )
    #endif
  }

  func testSwiftModules() throws {
    guard let ws = try staticTibsTestWorkspace(name: "SwiftModules") else { return }
    try ws.buildAndIndex()

    let aaa = Symbol(usr: "s:1A3aaayyF", name: "aaa()", kind: .function, language: .swift)
    checkOccurrences(
      ws.index.occurrences(ofUSR: aaa.usr, roles: .all),
      expected: [
        aaa.at(ws.testLoc("aaa:def"), moduleName: "A", roles: .definition, symbolProvider: .swift),
        aaa.at(ws.testLoc("aaa:call"), moduleName: "B", roles: .call, symbolProvider: .swift),
        aaa.at(ws.testLoc("aaa:call:c"), moduleName: "C", roles: .call, symbolProvider: .swift),
      ]
    )
  }

  func testEditsSimple() throws {
    guard let ws = try mutableTibsTestWorkspace(name: "proj1") else { return }
    try ws.buildAndIndex()

    let cdecl = Symbol(usr: "s:4main1cyyF", name: "c()", kind: .function, language: .swift)
    let roles: SymbolRole = [.reference, .definition, .declaration]

    checkOccurrences(
      ws.index.occurrences(ofUSR: cdecl.usr, roles: .all),
      expected: [
        cdecl.at(ws.testLoc("c"), roles: .definition, symbolProvider: .clang),
        cdecl.at(ws.testLoc("c:call"), roles: .call, symbolProvider: .clang),
      ]
    )

    try ws.edit(rebuild: true) { editor, files in
      let url = ws.testLoc("c:call").url
      let new = try files.get(url).appending(
        """

        func anotherOne() {
          /*c:anotherOne*/c()
        }
        """
      )

      editor.write(new, to: url)
    }

    checkOccurrences(
      ws.index.occurrences(ofUSR: cdecl.usr, roles: .all),
      expected: [
        cdecl.at(ws.testLoc("c"), roles: .definition, symbolProvider: .clang),
        cdecl.at(ws.testLoc("c:call"), roles: .call, symbolProvider: .clang),
        cdecl.at(ws.testLoc("c:anotherOne"), roles: .call, symbolProvider: .clang),
      ]
    )

    XCTAssertNotEqual(ws.testLoc("c").url, ws.testLoc("a:def").url)

    try ws.edit(rebuild: true) { editor, files in
      editor.write("", to: ws.testLoc("c").url)
      let new = try files.get(ws.testLoc("a:def").url).appending("\nfunc /*c*/c() -> Int { 0 }")
      editor.write(new, to: ws.testLoc("a:def").url)
    }

    XCTAssertEqual(ws.testLoc("c").url, ws.testLoc("a:def").url)

    checkOccurrences(ws.index.occurrences(ofUSR: cdecl.usr, roles: roles), expected: [])

    let newDecl = cdecl.with(usr: "s:4main1cSiyF")
    checkOccurrences(
      ws.index.occurrences(ofUSR: newDecl.usr, roles: .all),
      expected: [
        newDecl.at(ws.testLoc("c"), roles: .definition, symbolProvider: .clang),
        newDecl.at(ws.testLoc("c:call"), roles: .call, symbolProvider: .clang),
        newDecl.at(ws.testLoc("c:anotherOne"), roles: .call, symbolProvider: .clang),
      ]
    )
  }

  func testWaitUntilDoneInitializing() throws {
    guard let ws = try staticTibsTestWorkspace(name: "proj1") else { return }
    try ws.builder.build()
    let libIndexStore = try IndexStoreLibrary(dylibPath: ws.builder.toolchain.libIndexStore.path)
    let indexWait = try IndexStoreDB(
      storePath: ws.builder.indexstore.path,
      databasePath: ws.tmpDir.appendingPathComponent("wait", isDirectory: true).path,
      library: libIndexStore,
      waitUntilDoneInitializing: true,
      listenToUnitEvents: true
    )

    let csym = Symbol(usr: "s:4main1cyyF", name: "c()", kind: .function, language: .swift)
    let waitOccs = indexWait.occurrences(ofUSR: csym.usr, roles: [.reference, .definition])

    checkOccurrences(
      waitOccs,
      expected: [
        SymbolOccurrence(
          symbol: csym,
          location: SymbolLocation(ws.testLoc("c")),
          roles: [.definition, .canonical],
          symbolProvider: .clang
        ),
        SymbolOccurrence(
          symbol: csym,
          location: SymbolLocation(ws.testLoc("c:call")),
          roles: [.reference, .call, .calledBy, .containedBy],
          symbolProvider: .clang
        ),
      ]
    )
  }

  func testDelegate() throws {
    class Delegate: IndexDelegate {
      let queue: DispatchQueue = DispatchQueue(label: "testDelegate mutex")
      var _added: Int = 0
      var _completed: Int = 0
      var added: Int { queue.sync { _added } }
      var completed: Int { queue.sync { _completed } }

      func processingAddedPending(_ count: Int) {
        queue.sync {
          _added += count
        }
      }
      func processingCompleted(_ count: Int) {
        queue.sync {
          _completed += count
        }
      }
    }

    guard let ws = try mutableTibsTestWorkspace(name: "proj1") else { return }

    let delegate = Delegate()
    ws.delegate = delegate

    ws.index.pollForUnitChangesAndWait()

    XCTAssertEqual(delegate.added, 0)
    XCTAssertEqual(delegate.completed, 0)

    try ws.buildAndIndex()

    XCTAssertEqual(delegate.added, 3)
    XCTAssertEqual(delegate.completed, 3)
  }

  func testOutOfDateEvent() throws {
    struct OutOfDateInfo {
      let unitInfo: StoreUnitInfo
      let outOfDateModTime: UInt64
      let triggerHintFile: String
      let triggerHintDescription: String
      let synchronous: Bool
    }

    class Delegate: IndexDelegate {
      let queue: DispatchQueue = DispatchQueue(label: "testDelegate mutex")
      private var _outOfDateInfo: OutOfDateInfo?
      var outOfDateInfo: OutOfDateInfo? { queue.sync { _outOfDateInfo } }

      func reset() {
        queue.sync {
          _outOfDateInfo = nil
        }
      }

      func processingAddedPending(_ count: Int) {}
      func processingCompleted(_ count: Int) {}

      func unitIsOutOfDate(
        _ unitInfo: StoreUnitInfo,
        outOfDateModTime: UInt64,
        triggerHintFile: String,
        triggerHintDescription: String,
        synchronous: Bool
      ) {
        queue.sync {
          _outOfDateInfo = OutOfDateInfo(
            unitInfo: unitInfo,
            outOfDateModTime: outOfDateModTime,
            triggerHintFile: triggerHintFile,
            triggerHintDescription: triggerHintDescription,
            synchronous: synchronous
          )
        }
      }
    }

    guard let ws = try mutableTibsTestWorkspace(name: "proj1") else { return }
    try ws.buildAndIndex()

    let fileToChange = ws.testLoc("c").url
    // Set the mod-time a day in the future so that it would still be considered more recent than the unit after building.
    let modTime = try XCTUnwrap(Calendar.current.date(byAdding: .day, value: 1, to: Date()))
    try FileManager.default.setAttributes([.modificationDate: modTime], ofItemAtPath: fileToChange.path)
    let delegate = Delegate()
    ws.delegate = delegate
    try ws.reinitIndexStore(
      waitUntilDoneInitializing: true,
      enableOutOfDateFileWatching: true,
      listenToUnitEvents: true
    )

    waitForBlock({ delegate.outOfDateInfo != nil })

    let outOfDateInfo = try XCTUnwrap(delegate.outOfDateInfo)
    XCTAssertEqual(outOfDateInfo.unitInfo.mainFilePath, fileToChange.path)
    XCTAssertEqual(outOfDateInfo.triggerHintFile, fileToChange.path)

    delegate.reset()
    try ws.builder.build()
    // Make sure we didn't get another out-of-date notification just by creating the new unit.
    XCTAssertNil(delegate.outOfDateInfo)
  }

  func testMainFilesContainingFile() throws {
    guard let ws = try staticTibsTestWorkspace(name: "MainFiles") else { return }
    try ws.buildAndIndex()
    let index = ws.index

    let mainSwift = ws.testLoc("main_swift").url.path
    let main1 = ws.testLoc("main1").url.path
    let main2 = ws.testLoc("main2").url.path
    let uniq1 = ws.testLoc("uniq1").url.path
    let shared = ws.testLoc("shared").url.path
    let unknown = ws.testLoc("unknown").url.path

    let mainFiles = { (_ path: String, crossLang: Bool) -> Set<String> in
      Set(index.mainFilesContainingFile(path: path, crossLanguage: crossLang))
    }

    XCTAssertEqual(mainFiles(mainSwift, true), [mainSwift])
    XCTAssertEqual(mainFiles(mainSwift, false), [mainSwift])
    XCTAssertEqual(mainFiles(main1, false), [main1])
    XCTAssertEqual(mainFiles(main2, false), [main2])
    XCTAssertEqual(mainFiles(uniq1, false), [main1])
    XCTAssertEqual(mainFiles(shared, false), [main1, main2])
    XCTAssertEqual(mainFiles(shared, true), [main1, main2, mainSwift])
    XCTAssertEqual(mainFiles(unknown, true), [])
    XCTAssertEqual(mainFiles(unknown, false), [])
  }

  func testUnitIncludes() throws {
    guard let ws = try staticTibsTestWorkspace(name: "MainFiles") else { return }
    try ws.buildAndIndex()
    let index = ws.index

    let main1 = ws.testLoc("main1").url.path
    let uniq1 = ws.testLoc("uniq1").url.path
    let shared = ws.testLoc("shared").url.path

    let units = index.unitNamesContainingFile(path: main1)
    XCTAssertEqual(units.count, 1)
    let main1Unit = try XCTUnwrap(units.first)

    let includes = index.includesOfUnit(unitName: main1Unit)
    XCTAssertEqual(
      includes,
      [
        IndexStoreDB.UnitIncludeEntry(
          sourcePath: main1,
          targetPath: shared,
          line: ws.testLoc("include_main1_shared").line
        ),
        IndexStoreDB.UnitIncludeEntry(
          sourcePath: main1,
          targetPath: uniq1,
          line: ws.testLoc("include_main1_uniq1").line
        ),
      ]
    )
  }

  func testFilesIncludes() throws {
    guard let ws = try staticTibsTestWorkspace(name: "MainFiles") else { return }
    try ws.buildAndIndex()
    let index = ws.index

    let main1 = ws.testLoc("main1").url.path
    let main2 = ws.testLoc("main2").url.path
    let uniq1 = ws.testLoc("uniq1").url.path
    let shared = ws.testLoc("shared").url.path

    let includedFiles = index.filesIncludedByFile(path: main1).sorted()
    XCTAssertEqual(includedFiles, [shared, uniq1])

    let includingFiles = index.filesIncludingFile(path: shared).sorted()
    XCTAssertEqual(includingFiles, [main1, main2])
  }

  func testAllSymbolNames() throws {
    guard let ws = try staticTibsTestWorkspace(name: "proj1") else { return }
    try ws.buildAndIndex()
    let index = ws.index

    let expectedSymbolNames = ["a()", "b()", "c()"]

    XCTAssertEqual(index.allSymbolNames(), expectedSymbolNames)
  }

  func testExplicitOutputUnits() throws {
    guard let ws = try staticTibsTestWorkspace(name: "MixedLangTarget", useExplicitOutputUnits: true) else { return }
    try ws.buildAndIndex()
    let index = ws.index

    let ddecl = Symbol(usr: "c:@S@D", name: "D", kind: .class, language: .cxx)
    let getOccs = { index.occurrences(ofUSR: ddecl.usr, roles: .all) }

    // Output units are not set yet.
    XCTAssertEqual(0, getOccs().count)

    let indexOutputPaths = ws.builder.indexOutputPaths.map { $0.path }
    index.addUnitOutFilePaths(indexOutputPaths, waitForProcessing: true)
    checkOccurrences(
      getOccs(),
      expected: [
        ddecl.at(ws.testLoc("D:def"), roles: .definition, symbolProvider: .clang),
        ddecl.at(ws.testLoc("D:ref"), roles: .reference, symbolProvider: .clang),
        ddecl.at(ws.testLoc("D:ref:e.mm"), roles: .reference, symbolProvider: .clang),
      ]
    )

    let outUnitEMM = try XCTUnwrap(indexOutputPaths.first { $0.hasSuffix("-e.mm.o") })
    index.removeUnitOutFilePaths([outUnitEMM], waitForProcessing: true)
    checkOccurrences(
      getOccs(),
      expected: [
        ddecl.at(ws.testLoc("D:def"), roles: .definition, symbolProvider: .clang),
        ddecl.at(ws.testLoc("D:ref"), roles: .reference, symbolProvider: .clang),
      ]
    )

    // The bridging header is referenced as a PCH unit dependency, make sure we can see the data.
    let bhdecl = Symbol(usr: "c:@F@bridgingHeader", name: "bridgingHeader", kind: .function, language: .c)
    let bridgingHeaderOccs = index.occurrences(ofUSR: bhdecl.usr, roles: .all)
    checkOccurrences(
      bridgingHeaderOccs,
      expected: [
        bhdecl.at(ws.testLoc("bridgingHeader:decl"), roles: .declaration, symbolProvider: .clang),
        bhdecl.with(name: "bridgingHeader()", language: .swift).at(
          ws.testLoc("bridgingHeader:call"),
          roles: .call,
          symbolProvider: .swift
        ),
      ]
    )
  }

  func testSymbolsInFileC() throws {
    guard let ws = try staticTibsTestWorkspace(name: "CProject") else { return }
    try ws.buildAndIndex()

    let inputs = [
      (
        path: ws.testLoc("a_function:def").url.path,
        expectedSymbolNames: ["a_function"]
      ),
      (
        path: ws.testLoc("a_function:decl").url.path,
        expectedSymbolNames: ["a_function"]
      ),
      (
        path: ws.testLoc("some_other_function:def").url.path,
        expectedSymbolNames: ["some_other_function"]
      ),
    ]

    testSymbolsInFilePath(with: inputs, usingIndex: ws.index)
  }

  func testSymbolsInFileSwift() throws {
    guard let ws = try staticTibsTestWorkspace(name: "SwiftModules") else { return }
    try ws.buildAndIndex()

    let inputs = [
      (
        path: ws.testLoc("aaa:def").url.path,
        expectedSymbolNames: ["aaa()"]
      ),
      (
        path: ws.testLoc("bbb:def").url.path,
        expectedSymbolNames: ["bbb()"]
      ),
      (
        path: ws.testLoc("DDD:testMethod:def").url.path,
        expectedSymbolNames: ["DDD", "ccc()", "testMethod()", "init()"]
      ),
    ]

    testSymbolsInFilePath(with: inputs, usingIndex: ws.index)
  }

  func testSymbolsInFilePath(
    with inputs: [(path: String, expectedSymbolNames: [String])],
    usingIndex subject: IndexStoreDB
  ) {
    for (path, expectedSymbolNames) in inputs {
      let actualSymbolNames = subject.symbols(inFilePath: path).map(\.name)
      XCTAssertEqual(actualSymbolNames.sorted(), expectedSymbolNames.sorted())
    }
  }

  func testProperties() throws {
    guard let ws = try staticTibsTestWorkspace(name: "Properties") else { return }
    let index = ws.index

    try ws.buildAndIndex()

    let asyncFuncSym = Symbol(
      usr: "s:4main9asyncFuncyyYaF",
      name: "asyncFunc()",
      kind: .function,
      properties: .swiftAsync,
      language: .swift
    )
    let asyncFuncOccs = index.occurrences(ofUSR: asyncFuncSym.usr, roles: .definition)
    checkOccurrences(
      asyncFuncOccs,
      expected: [
        asyncFuncSym.at(ws.testLoc("asyncFunc:def"), roles: .definition, symbolProvider: .swift)
      ]
    )

    let asyncMethSym = Symbol(
      usr: "s:4main8MyStructV11asyncMethodyyYaF",
      name: "asyncMethod()",
      kind: .instanceMethod,
      properties: .swiftAsync,
      language: .swift
    )
    let asyncMethOccs = index.occurrences(ofUSR: asyncMethSym.usr, roles: .definition)
    checkOccurrences(
      asyncMethOccs,
      expected: [
        asyncMethSym.at(ws.testLoc("asyncMethod:def"), roles: .definition, symbolProvider: .swift)
      ]
    )

    let testMeSym = Symbol(
      usr: "s:4main10MyTestCaseC6testMeyyF",
      name: "testMe()",
      kind: .instanceMethod,
      properties: .unitTest,
      language: .swift
    )
    let testMeOccs = index.occurrences(ofUSR: testMeSym.usr, roles: .definition)
    checkOccurrences(
      testMeOccs,
      expected: [
        testMeSym.at(ws.testLoc("testMe:def"), roles: .definition, symbolProvider: .swift)
      ]
    )

    let testMeAsyncSym = Symbol(
      usr: "s:4main10MyTestCaseC11testMeAsyncyyYaF",
      name: "testMeAsync()",
      kind: .instanceMethod,
      properties: [.unitTest, .swiftAsync],
      language: .swift
    )
    let testMeAsyncOccs = index.occurrences(ofUSR: testMeAsyncSym.usr, roles: .definition)
    checkOccurrences(
      testMeAsyncOccs,
      expected: [
        testMeAsyncSym.at(ws.testLoc("testMeAsync:def"), roles: .definition, symbolProvider: .swift)
      ]
    )
  }

  func testConcepts() throws {
    guard let ws = try staticTibsTestWorkspace(name: "CxxLangFeatures") else { return }
    try XCTSkipIf(ws.libIndexStore.version < IndexStoreLibrary.Version(major: 0, minor: 14))

    try ws.buildAndIndex()

    let largeType = Symbol(usr: "c:@CT@LargeType", name: "LargeType", kind: .concept, language: .c)
    let largeTypeOccs = ws.index.occurrences(ofUSR: largeType.usr, roles: .all)
    checkOccurrences(
      largeTypeOccs,
      expected: [
        largeType.at(ws.testLoc("LargeType:def"), roles: .definition, symbolProvider: .swift),
        largeType.at(ws.testLoc("LargeType:ref"), roles: .reference, symbolProvider: .swift),
      ]
    )
  }
}
