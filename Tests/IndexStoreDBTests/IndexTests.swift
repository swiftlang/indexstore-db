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
import ISDBTestSupport
import XCTest

final class IndexTests: XCTestCase {

  func testBasic() throws {
    guard let ws = try staticTibsTestWorkspace(name: "proj1") else { return }
    let index = ws.index

    let usr = "s:4main1cyyF"
    let getOccs = { index.occurrences(ofUSR: usr, roles: [.reference, .definition]) }

    XCTAssertEqual(0, getOccs().count)

    try ws.buildAndIndex()

    let csym = Symbol(usr: usr, name: "c()", kind: .function)
    let asym = Symbol(usr: "s:4main1ayyF", name: "a()", kind: .function)

    let ccanon = SymbolOccurrence(
      symbol: csym,
      location: SymbolLocation(ws.testLoc("c")),
      roles: [.definition, .canonical],
      relations: [])

    let ccall = SymbolOccurrence(
      symbol: csym,
      location: SymbolLocation(ws.testLoc("c:call")),
      roles: [.reference, .call, .calledBy, .containedBy],
      relations: [
        .init(symbol: asym, roles: [.calledBy, .containedBy])
    ])

    checkOccurrences(getOccs(), ignoreRelations: false, allowAdditionalRoles: false, expected: [
      ccanon,
      ccall,
    ])

    checkOccurrences(index.canonicalOccurrences(ofName: "c()"), ignoreRelations: false, expected: [
      ccanon,
    ])

    checkOccurrences(index.canonicalOccurrences(ofName: "c"), ignoreRelations: false, expected: [])

    checkOccurrences(index.canonicalOccurrences(containing: "c",
      anchorStart: true, anchorEnd: false, subsequence: false,
      ignoreCase: false), ignoreRelations: false, expected: [ccanon])

    checkOccurrences(index.canonicalOccurrences(containing: "c",
      anchorStart: true, anchorEnd: true, subsequence: false,
      ignoreCase: false), ignoreRelations: false, expected: [])

    checkOccurrences(index.canonicalOccurrences(containing: "C",
      anchorStart: true, anchorEnd: false, subsequence: false,
      ignoreCase: true), ignoreRelations: false, expected: [ccanon])

    checkOccurrences(index.canonicalOccurrences(containing: "C",
      anchorStart: true, anchorEnd: false, subsequence: false,
      ignoreCase: false), ignoreRelations: false, expected: [])

    checkOccurrences(
      index.occurrences(relatedToUSR: "s:4main1ayyF", roles: .calledBy),
      ignoreRelations: false,
      expected:
    [
      ccall,
      SymbolOccurrence(
        symbol: Symbol(usr: "s:4main1byyF", name: "b()", kind: .function),
        location: SymbolLocation(ws.testLoc("b:call")),
        roles: [.reference, .call, .calledBy, .containedBy],
        relations: [
          .init(symbol: asym, roles: [.calledBy, .containedBy])
      ])
    ])
  }

  func testMixedLangTarget() throws {
    guard let ws = try staticTibsTestWorkspace(name: "MixedLangTarget") else { return }
    try ws.buildAndIndex()
    let index = ws.index

  #if os(macOS)

    let cdecl = Symbol(usr: "c:objc(cs)C", name: "C", kind: .class)
    let cdeclOccs = index.occurrences(ofUSR: cdecl.usr, roles: .all)
    checkOccurrences(cdeclOccs, expected: [
      cdecl.at(ws.testLoc("C:decl"), roles: [.declaration, .canonical]),
      cdecl.at(ws.testLoc("C:def"), roles: .definition),
      cdecl.at(ws.testLoc("C:ref:swift"), roles: .reference),
      cdecl.at(ws.testLoc("C:ref:e.mm"), roles: .reference),
    ])

    let cmethod = Symbol(usr: "c:objc(cs)C(im)method", name: "method", kind: .instanceMethod)
    let cmethodOccs = index.occurrences(ofUSR: cmethod.usr, roles: .all)
    checkOccurrences(cmethodOccs, expected: [
      cmethod.with(name: "method()").at(ws.testLoc("C.method:call:swift"), roles: [.call, .dynamic]),
      cmethod.at(ws.testLoc("C.method:decl"), roles: .declaration),
      cmethod.at(ws.testLoc("C.method:def"), roles: .definition),
      cmethod.at(ws.testLoc("C.method:call:e.mm"), roles: [.call, .dynamic]),
    ])
  #endif

    let ddecl = Symbol(usr: "c:@S@D", name: "D", kind: .class)
    let dOccs = index.occurrences(ofUSR: ddecl.usr, roles: .all)
    checkOccurrences(dOccs, expected: [
      ddecl.at(ws.testLoc("D:def"), roles: .definition),
      ddecl.at(ws.testLoc("D:ref"), roles: .reference),
      ddecl.at(ws.testLoc("D:ref:e.mm"), roles: .reference),
    ])

    let bhdecl = Symbol(usr: "c:@F@bridgingHeader", name: "bridgingHeader", kind: .function)
    let bridgingHeaderOccs = index.occurrences(ofUSR: bhdecl.usr, roles: .all)
    checkOccurrences(bridgingHeaderOccs, expected: [
      bhdecl.at(ws.testLoc("bridgingHeader:decl"), roles: .declaration),
      bhdecl.with(name: "bridgingHeader()").at(ws.testLoc("bridgingHeader:call"), roles: .call),
    ])
  }

  func testSwiftModules() throws {
    guard let ws = try staticTibsTestWorkspace(name: "SwiftModules") else { return }
    try ws.buildAndIndex()

    let aaa = Symbol(usr: "s:1A3aaayyF", name: "aaa()", kind: .function)
    checkOccurrences(ws.index.occurrences(ofUSR: aaa.usr, roles: .all), expected: [
      aaa.at(ws.testLoc("aaa:def"), roles: .definition),
      aaa.at(ws.testLoc("aaa:call"), roles: .call),
      aaa.at(ws.testLoc("aaa:call:c"), roles: .call),
    ])
  }

  func testEditsSimple() throws {
    guard let ws = try mutableTibsTestWorkspace(name: "proj1") else { return }
    try ws.buildAndIndex()

    let cdecl = Symbol(usr: "s:4main1cyyF", name: "c()", kind: .function)
    let roles: SymbolRole = [.reference, .definition, .declaration]

    checkOccurrences(ws.index.occurrences(ofUSR: cdecl.usr, roles: .all), expected: [
      cdecl.at(ws.testLoc("c"), roles: .definition),
      cdecl.at(ws.testLoc("c:call"), roles: .call),
    ])

    try ws.edit(rebuild: true) { editor, files in
      let url = ws.testLoc("c:call").url
      let new = try files.get(url).appending("""

        func anotherOne() {
          /*c:anotherOne*/c()
        }
        """)

      editor.write(new, to: url)
    }

    checkOccurrences(ws.index.occurrences(ofUSR: cdecl.usr, roles: .all), expected: [
      cdecl.at(ws.testLoc("c"), roles: .definition),
      cdecl.at(ws.testLoc("c:call"), roles: .call),
      cdecl.at(ws.testLoc("c:anotherOne"), roles: .call),
    ])

    XCTAssertNotEqual(ws.testLoc("c").url, ws.testLoc("a:def").url)

    try ws.edit(rebuild: true) { editor, files in
      editor.write("", to: ws.testLoc("c").url)
      let new = try files.get(ws.testLoc("a:def").url).appending("\nfunc /*c*/c() -> Int { 0 }")
      editor.write(new, to: ws.testLoc("a:def").url)
    }

    XCTAssertEqual(ws.testLoc("c").url, ws.testLoc("a:def").url)

    checkOccurrences(ws.index.occurrences(ofUSR: cdecl.usr, roles: roles), expected: [])

    let newDecl = cdecl.with(usr: "s:4main1cSiyF")
    checkOccurrences(ws.index.occurrences(ofUSR: newDecl.usr, roles: .all), expected: [
      newDecl.at(ws.testLoc("c"), roles: .definition),
      newDecl.at(ws.testLoc("c:call"), roles: .call),
      newDecl.at(ws.testLoc("c:anotherOne"), roles: .call),
    ])
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
      listenToUnitEvents: true)

    let csym = Symbol(usr: "s:4main1cyyF", name: "c()", kind: .function)
    let waitOccs = indexWait.occurrences(ofUSR: csym.usr, roles: [.reference, .definition])

    checkOccurrences(waitOccs, expected: [
      SymbolOccurrence(
       symbol: csym,
        location: SymbolLocation(ws.testLoc("c")),
       roles: [.definition, .canonical]),
      SymbolOccurrence(
        symbol: csym,
        location: SymbolLocation(ws.testLoc("c:call")),
        roles: [.reference, .call, .calledBy, .containedBy]),
    ])
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

    let mainFiles = { (_ path: String) -> Set<String> in
      Set(index.mainFilesContainingFile(path: path))
    }

    XCTAssertEqual(mainFiles(mainSwift), [mainSwift])
    XCTAssertEqual(mainFiles(main1), [main1])
    XCTAssertEqual(mainFiles(main2), [main2])
    XCTAssertEqual(mainFiles(uniq1), [main1])
    XCTAssertEqual(mainFiles(shared), [main1, main2])
    XCTAssertEqual(mainFiles(unknown), [])
  }
}
