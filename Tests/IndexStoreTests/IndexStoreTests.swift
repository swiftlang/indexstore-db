import Foundation
import ISDBTibs
import IndexStore
import Testing

@Suite
struct IndexStoreTests {
  @Test func swiftFileWithSingleSymbol() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      func foo() async {}
      """
    ])
    try await project.withIndexStore { indexStore in
      let unitNames = indexStore.unitNames(sorted: false).map { $0.string }
      let unitName = try #require(unitNames.only)
      #expect(unitName.hasPrefix("test.o"))

      let unit = try IndexStoreStringRef.withStringRef(unitName) { try indexStore.unit(named: $0) }
      #expect(unit.isSystemUnit == false)
      #expect(unit.isModuleUnit == false)
      #expect(unit.isDebugCompilation == true)
      #expect(unit.hasMainFile == true)
      #expect(unit.providerIdentifier.string == "swift")
      #expect(unit.providerVersion.string == "")
      // Unit should have been created in the last 5 minutes or something is really off
      #expect(abs(unit.modificationDate.timeIntervalSinceNow) < 5 * 60)
      #expect(unit.mainFile.string.hasSuffix("test.swift"))
      #expect(unit.moduleName.string == "test")
      #expect(unit.workingDirectory.string != "")
      #expect(unit.outputFile.string.hasSuffix("test.o"))
      #expect(unit.sysrootPath.string != "")
      #expect(unit.target.string != "")

      let recordNames = unit.dependencies.compactMap { (dependency) -> String? in
        guard dependency.kind == .record else {
          return nil
        }
        let isSystem = dependency.isSystem
        #expect(!isSystem)
        #expect(dependency.filePath.string.hasSuffix("test.swift"))
        #expect(dependency.moduleName.string == "")
        return dependency.name.string
      }

      let recordName = try #require(recordNames.only)

      let record = try IndexStoreStringRef.withStringRef(recordName) { try indexStore.record(named: $0) }
      record.symbols.forEach { symbol in
        #expect(symbol.language == .swift)
        #expect(symbol.kind == .function)
        #expect(symbol.subkind == .none)
        #expect(symbol.properties == [.swiftAsync])
        #expect(symbol.roles == [.definition])
        #expect(symbol.relatedRoles == [])
        #expect(symbol.name.string == "foo()")
        #expect(symbol.usr.string == "s:4test3fooyyYaF")
        #expect(symbol.codegenName.string == "")
        return .continue
      }

      record.occurrences.forEach { occurrence in
        #expect(occurrence.symbol.usr.string == "s:4test3fooyyYaF")
        #expect(occurrence.roles == [.definition])
        #expect(occurrence.position == (1, 6))

        let relationsCount = occurrence.relations.map { _ in () }.count
        #expect(relationsCount == 0)
        return .continue
      }
    }
  }

  @Test
  func swiftFileWithMultipleSymbols() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      func first() {}
      func second() {}
      """
    ])
    try await project.withIndexStore { indexStore in
      let record = try indexStore.onlyRecord
      let symbolNames = record.occurrences.map { $0.symbol.name.string }
      #expect(symbolNames == ["first()", "second()"])
    }
  }

  @Test
  func symbolsMatching() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      func testMe() {}
      func testSomethingElse() {}
      """
    ])
    try await project.withIndexStore { indexStore in
      let record = try indexStore.onlyRecord
      let allSymbolNames = record.symbols(matching: { $0.name.string.contains("test") }).map { $0.name.string }
      #expect(allSymbolNames == ["testMe()", "testSomethingElse()"])

      let somethingElseSymbolNames = record.symbols(matching: { $0.name.string.contains("SomethingElse") }).map {
        $0.name.string
      }
      #expect(somethingElseSymbolNames == ["testSomethingElse()"])
    }
  }

  @Test
  func occurrencesInLineRange() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      func line1() {}
      func line2() {}
      func line3() {}
      """
    ])
    try await project.withIndexStore { indexStore in
      let record = try indexStore.onlyRecord
      let symbols = record.occurrences(inLineRange: 1...2).map { $0.symbol.name.string }
      #expect(symbols == ["line1()", "line2()"])
    }
  }

  @Test
  func symbolWithRelation() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      struct Foo {
        func bar() {}
      }
      """
    ])
    try await project.withIndexStore { indexStore in
      let record = try indexStore.onlyRecord
      record.occurrences.forEach { occurrences in
        guard occurrences.symbol.name.string == "bar()" else {
          return .continue
        }
        occurrences.relations.forEach { relation in
          #expect(relation.symbol.name.string == "Foo")
          #expect(relation.roles == [.childOf])
          return .continue
        }
        return .continue
      }
    }
  }

  @Test
  func openInvalidDylib() async throws {
    await #expect(throws: (any Error).self) {
      try await IndexStoreLibrary.at(dylibPath: URL(filePath: "/does/not/exist"))
    }
  }

  @Test
  func openInvalidUnit() async throws {
    let project = TestProject(swiftFiles: ["test.swift": ""])
    try await project.withIndexStore { indexStore in
      #expect(throws: (any Error).self) {
        try IndexStoreStringRef.withStringRef("does-not-exist") { name in
          try indexStore.unit(named: name)
        }
      }
    }
    await #expect(throws: (any Error).self) {
      try await IndexStoreLibrary.at(dylibPath: URL(filePath: "/does/not/exist"))
    }
  }

  @Test
  func throwErrorDuringSequenceIteration() async throws {
    let project = TestProject(swiftFiles: ["test.swift": ""])
    try await project.withIndexStore { indexStore in
      struct MyError: Error {}
      #expect(throws: MyError.self) {
        try indexStore.unitNames(sorted: false).forEach { _ in
          throw MyError()
        }
      }
    }
  }
}
