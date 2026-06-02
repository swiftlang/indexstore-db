import Testing

@Suite
struct DescriptionTests {
  @Test func swiftUnitDescription() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      func testFunc() {}
      """
    ])
    try await project.withIndexStore { indexStore in
      let unitName = try #require(indexStore.unitNames(sorted: false).map { $0.string }.only)
      let unit = try indexStore.unit(named: unitName)
      let description = unit.description

      #expect(
        try Regex(
          #"""
          Module: test
          Has Main File: true
          Main File: .*[\/]test.swift
          Output File: .*[\/]test.o
          Target: .*-.*-.*
          Sysroot: .*
          Working Directory: .*[\/]sources
          Is System: false
          Is Module: false
          Is Debug: true
          Provider Identifier: swift
          Provider Version: .*
          Modification Date: 20.*
          Dependencies:
          Unit \| system \| Swift \| .*[\/]Swift.swiftmodule[\/].*swiftinterface
          Record \| user \| .*[\/]test.swift \| test.swift-.*
          """#
        ).wholeMatch(in: description) != nil,
        """
        Description did not match expected pattern. Got:
        \(description)
        """
      )
    }
  }

  @Test func cUnitDescription() async throws {
    let project = TestProject(
      clangFiles: [
        "Test.c": """
        #include "Test.h"
        """
      ],
      supplementaryFiles: ["Test.h": ""]
    )
    try await project.withIndexStore { indexStore in
      let unitName = try #require(indexStore.unitNames(sorted: false).map { $0.string }.only)
      let unit = try indexStore.unit(named: unitName)
      let description = unit.description

      #expect(
        try Regex(
          #"""
          Module: .*
          Has Main File: true
          Main File: .*[\/]Test.c
          Output File: .*[\/]Test.o
          Target: .*-.*-.*
          Sysroot: .*
          Working Directory: .*[\/]sources
          Is System: false
          Is Module: false
          Is Debug: true
          Provider Identifier: clang
          Provider Version: .*
          Modification Date: 20.*
          Dependencies:
          File \| user \| .*[\/]Test.c
          File \| user \| .*[\/]Test.h
          Includes:
          .*[\/]Test.c:1 \| .*[\/]Test.h
          """#
        ).wholeMatch(in: description) != nil,
        """
        Description did not match expected pattern. Got:
        \(description)
        """
      )

    }
  }

  @Test func recordDescription() async throws {
    let project = TestProject(swiftFiles: [
      "test.swift": """
      struct Foo {
        subscript(x: Int) -> Int { x }
        func testFunc() async {}
      }
      """
    ])
    try await project.withIndexStore { indexStore in
      let unitName = try #require(indexStore.unitNames(sorted: false).map { $0.string }.only)
      let unit = try indexStore.unit(named: unitName)

      let recordNames = unit.dependencies.compactMap { dep in
        dep.kind == .record ? dep.name.string : nil
      }
      let recordName = try #require(recordNames.only)
      let record = try indexStore.record(named: recordName)
      let description = record.description

      #expect(
        description == """
          Symbols:
          init() | constructor(internal) | s:4test3FooVACycfc | definition, implicit, childOf
          Foo | struct | s:4test3FooV | definition
          subscript(_:) | instanceProperty.swiftSubscript | s:4test3FooVyS2icip | definition, childOf
          x | parameter | s:4test3FooVyS2icip1xL_Sivp | definition, childOf
          Int | struct | s:Si | reference
          getter:subscript(_:) | instanceMethod.accessorGetter | s:4test3FooVyS2icig | definition, childOf, accessorOf
          testFunc() | instanceMethod(swiftAsync, internal) | s:4test3FooV0A4FuncyyYaF | definition, childOf

          Occurrences:
          1:8 | init() | constructor(internal) | s:4test3FooVACycfc | definition, implicit, childOf
            childOf | s:4test3FooV
          1:8 | Foo | struct | s:4test3FooV | definition
          2:3 | subscript(_:) | instanceProperty.swiftSubscript | s:4test3FooVyS2icip | definition, childOf
            childOf | s:4test3FooV
          2:13 | x | parameter | s:4test3FooVyS2icip1xL_Sivp | definition, childOf
            childOf | s:4test3FooVyS2icip
          2:16 | Int | struct | s:Si | reference
          2:24 | Int | struct | s:Si | reference
          2:28 | getter:subscript(_:) | instanceMethod.accessorGetter | s:4test3FooVyS2icig | definition, childOf, accessorOf
            childOf, accessorOf | s:4test3FooVyS2icip
          3:8 | testFunc() | instanceMethod(swiftAsync, internal) | s:4test3FooV0A4FuncyyYaF | definition, childOf
            childOf | s:4test3FooV
          """
      )
    }
  }
}
