# Tibs

Tibs ("Test Index Build System") is a simple and flexible build system designed for test projects of IndexStoreDB and SourceKit-LSP. Tibs can incrementally build or rebuild the index data and generated module files for a Swift and/or C-family language test project. Tibs can also dump the compiler arguments to a clang-compatible JSON compilation database. It is *not designed to compile to executable code*.

Tibs is implemented using [Ninja](https://ninja-build.org), which introduces a new dependency in IndexStoreDB when running tests.

Tibs projects are described by a `project.json` file containing one or more targets. Typically, a test case will use a project fixture located in the `INPUTS` directory.

## Project

A Tibs project is described by `project.json` manifest file. The top-level entity is a target, which may depend on other targets.

Example:

```
{
  "targets": [
    {
      "name": "mytarget",
      "swift_flags": ["-warnings-as-errors"],
      "sources": ["a.swift", "b.swift"],
      "dependencies": ["dep1"],
    },
    {
      "name": "dep1",
      "sources": ["dep1.swift"],
    },
  ]
}
```

As a convenience, if the project consists of a single target, it can be written at the top level and the name can be omitted, e.g.

```
{ "sources": ["main.swift"] }
```

### Targets

Targets have the following fields:
    
* "name": The name of the target. If omitted, it will be "main".
* "sources": Array of source file paths.
* "bridging_header": Optional path to a Swift bridging header.
* "dependencies": Optional array of target names that this target depends on. This is useful for Swift module dependencies.
* "swift_flags": Optional array of extra compiler arguments for Swift.
* "clang_flags": Optional array of extra compiler arguments for Clang.

The directory containing the `project.json` manifest file is considered the project's root directory and any relative paths are relative to that.

## Building Tibs Projects

Most tests using Tibs will use the APIs provided by the `TibsTestWorkspace` class in the `ISDBTestSupport` module, which provides a high-level API for working with a complete Tibs project. The following information is for anyone needing to work using the lower-level APIs from `ISDBTibs` or the `tibs` command-line tool.

At a high level, both the library and command-line interfaces for Tibs work similarly: given a project and toolchain (containing executables clang, swiftc, ninja, etc.), produce a Ninja build description file and a compilation database.  When built using `ninja`, all of the generated Swift modules are created and the raw index data is produced.

### ISDBTibs Library

The `TibsBuilder` class provides a library interface for building Tibs projects. It is responsible for fully resolving all the dependencies, build outputs, and compiler arguments. It also has APIs for executing the build using the `ninja` command-line tool and outputting a compilation database (`compile_commands.json`).

#### Example

The inputs to the build process are a toolchain, project, and build directory.

```
import ISDBTibs

let toolchain = TibsToolchain(...)
let projectDir: URL = ...
let buildDir: URL = ...
```

We load the `project.json` manifest from the project directory.

```
let manifest = try TibsManifest.load(projectRoot: projectDir)
```

From a manifest, we can create a `TibsBuilder` and examine the fully resolved targets.

```
let builder = try TibsBuilder(
  manifest: manifest,
  sourceRoot: projectDir,
  buildRoot: buildDir,
  toolchain: toolchain)

for target in builder.targets {
  ...
}
```

Finally, we can write the build files and build, or incrementally rebuild the project's index data and modules.

```
try builder.writeBuildFiles()
try builder.build()
// edit sources
try builder.build()
```

### tibs Command-line Tool

As a convenience, we also provide a command-line interface `tibs`. It uses the `ISDBTibs` library to write out build files, which can then be built using `ninja`.

#### Example

Running `tibs` will write out the necessary Ninja build file and compilation database.

```
mkdir build
cd build
tibs <path to project directory, containing project.json>
```

To execute the build, use the `ninja` command

```
ninja
```

The build directory will be populated with the generated swift modules and index data.

## FAQ

### Why not use the Swift Package Manager?

The primary reason not to use the Swift Package Manager (SwiftPM) for our test projects is that SwiftPM's model is stricter than other build systems we need to support, and stricter than we want for our testing support. For example, we want to be able to test mixed language targets (using bridging headers), and to perform only the module-generation and indexing parts of the build without emitting object code. We need to be able to add features to our test support that would break the clean model that SwiftPM provides.
