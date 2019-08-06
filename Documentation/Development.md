# Development

This document contains notes about development and testing of IndexStoreDB.

## Table of Contents

* [Writing Tests](#writing-tests)
* [Tibs, the "Test Index Build System"](Tibs.md)

## Writing Tests

As much as is practical, all code should be covered by tests. New tests can be added under the `Tests` directory and should use `XCTest`. The rest of this section will describe the additional tools available in the `ISDBTestSupport` module to make it easier to write good and efficient tests.

Most indexer tests follow a pattern:

1. Build and index a test project
2. Perform index queries
3. Compare the results against known locations in the test code
4. (Optional) modify the code and repeat.

### Test Projects (Fixtures)

Index test projects should be put in the `Tests/INPUTS` directory, and use the [Tibs](Tibs.md) build system to define their sources and targets. An example test project might look like:

```
Tests/
  Inputs/
    MyTestProj/
      a.swift
      b.swift
      c.cpp
```

Where `project.json` describes the project's targets, for example

```
{ "sources": ["a.swift", "b.swift", "c.cpp"] }
```

Tibs supports more advanced project configurations, such as multiple swift modules with dependencies, etc. For much more information about Tibs, including what features it supports and how it works, see [Tibs.md](Tibs.md).

### TibsTestWorkspace

The `TibsTestWorkspace` pulls together the various pieces needed for working with tests, including building the project, creating an IndexStoreDB instance, and keeping it up to date after modifying sources.

To create a `TibsTestWorkspace`, use the `staticTibsTestWorkspace` and `mutableTibsTestWorkspace` methods. Tests that do not mutate sources should use `staticTibsTestWorkspace`.

```swift
func testFoo() {

  // Loads an immutable workspace for the test project "MyTestProj" and resolves
  // all of its targets, ready to be indexed.
  guard let ws = try staticTibsTestWorkspace(name: "MyTestProj") else { return }

  // Perform the build and read the produced index data.
  try ws.buildAndIndex()

  // See the results
  ws.index.occurrences(ofUSR: "<some usr>", ...)

  ...
}
```

#### Source Locations

It is common to want to refer to specific locations in the source code of a test project. This is supported using inline comment syntax.

```swift
Test.swift:
func /*myFuncDef*/myFunc() {
  /*myFuncCall*/myFunc()
}
```

In a test, these locations can be referenced by name. The named location is immediately after the comment.

```swift

let loc = ws.testLoc("myFuncDef")
// TestLocation(url: ..., line: 1, column: 19)
```

A common use of a `TestLocation` is to form a `SymbolOccurrence` at that location.

```swift
let occurrence = Symbol(...).at(ws.testLoc("myFuncDef"), roles: .definition)
```

#### Comparing SymbolOccurrences

Rather than attempting to compare `SymbolOccurrences` for equality, prefer using the custom XCTest assertion method `checkOccurrences`. This API avoids accidentally comparing details that are undefined (e.g. the order of occurrences), or likely to change frequently (e.g. `SymbolRole`s are checked to be a superset instead of an exact match, as we often add new roles).

```
checkOccurrences(ws.index.occurrences(ofUSR: sym.usr, roles: .all), expected: [
  sym.at(ws.testLoc("c"), roles: .definition),
  sym.at(ws.testLoc("c_call"), roles: .call),
])
```

#### Mutating Test Sources

In order to test changes to the index, we create a mutable test workspace. The mutable project takes care of copying the sources to a new location. Afterwards, we can mutate the sources using the `TibsTestWorkspace.edit(rebuild: Bool, block: ...)` method.

```
func testMutating() {
  guard let ws = try mutableTibsTestWorkspace(name: "MyTestProj") else { return }
  try ws.buildAndIndex()

  // Check initial state...

  // Perform modifications to "url"!
  ws.edit(rebuild: true) { editor, files in

    // Read the original contents and add a new line.
    let new = try files.get(url).appending("func /*moreCodeDef*/moreCode() {}")

    // Provide the new contents for "url".
    editor.write(new, to: url)
  }
}
```

After we return from `edit()`, the sources are modified and any changes to stored source locations are reflected. We can now `buildAndIndex()` to update the index, or as a convenience we can pass `rebuild: true` to `edit`.
