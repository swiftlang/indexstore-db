// swift-tools-version: 6.2

import Foundation
import PackageDescription

func hasEnvironmentVariable(_ name: String) -> Bool {
  return ProcessInfo.processInfo.environment[name] != nil
}

/// Assume that all the package dependencies are checked out next to indexstore-db and use that instead of fetching a
/// remote dependency.
var useLocalDependencies: Bool { hasEnvironmentVariable("SWIFTCI_USE_LOCAL_DEPS") }

var dependencies: [Package.Dependency] {
  if useLocalDependencies {
    return [
      .package(path: "../swift-lmdb")
    ]
  } else {
    return [
      .package(url: "https://github.com/swiftlang/swift-lmdb.git", branch: "main")
    ]
  }
}

let package = Package(
  name: "IndexStoreDB",
  platforms: [.macOS(.v14)],
  products: [
    .library(
      name: "IndexStoreDB",
      targets: ["IndexStoreDB"]
    ),
    .library(
      name: "IndexStoreDB_CXX",
      targets: ["IndexStoreDB_Index"]
    ),
    .library(
      name: "ISDBTestSupport",
      targets: ["ISDBTestSupport"]
    ),
    .executable(
      name: "tibs",
      targets: ["tibs"]
    ),
    .library(
      name: "IndexStore",
      targets: ["IndexStore"]
    ),
  ],
  dependencies: dependencies,
  targets: [
    // MARK: Swift interface to read a raw index store

    .target(
      name: "IndexStore",
      dependencies: ["IndexStoreDB_CIndexStoreDB"],
      swiftSettings: [
        .enableUpcomingFeature("ExistentialAny"),
        .enableUpcomingFeature("InternalImportsByDefault"),
        .enableUpcomingFeature("MemberImportVisibility"),
        .enableUpcomingFeature("InferIsolatedConformances"),
        .enableUpcomingFeature("NonisolatedNonsendingByDefault"),
        .enableExperimentalFeature("Lifetimes"),
        .swiftLanguageMode(.v6),
      ]
    ),

    .testTarget(
      name: "IndexStoreTests",
      dependencies: [
        "IndexStore",
        "ISDBTibs",
      ]
    ),

    // MARK: Swift interface

    .target(
      name: "IndexStoreDB",
      dependencies: ["IndexStoreDB_CIndexStoreDB"],
      exclude: ["CMakeLists.txt"]
    ),

    .testTarget(
      name: "IndexStoreDBTests",
      dependencies: ["IndexStoreDB", "ISDBTestSupport"],
      linkerSettings: [.linkedLibrary("execinfo", .when(platforms: [.custom("freebsd")]))]
    ),

    // MARK: Swift Test Infrastructure

    // The Test Index Build System (tibs) library.
    .target(
      name: "ISDBTibs",
      dependencies: []
    ),

    .testTarget(
      name: "ISDBTibsTests",
      dependencies: ["ISDBTibs", "ISDBTestSupport"]
    ),

    // Commandline tool for working with tibs projects.
    .executableTarget(
      name: "tibs",
      dependencies: ["ISDBTibs"]
    ),

    // Test support library, built on top of tibs.
    .target(
      name: "ISDBTestSupport",
      dependencies: ["IndexStoreDB", "ISDBTibs", "tibs"],
      resources: [
        .copy("INPUTS")
      ],
      linkerSettings: [
        .linkedFramework("XCTest", .when(platforms: [.iOS, .macOS, .tvOS, .watchOS]))
      ]
    ),

    // MARK: C++ interface

    // Primary C++ interface.
    .target(
      name: "IndexStoreDB_Index",
      dependencies: ["IndexStoreDB_Database"],
      exclude: [
        "CMakeLists.txt",
        "indexstore_functions.def",
      ]
    ),

    // C wrapper for IndexStoreDB_Index.
    .target(
      name: "IndexStoreDB_CIndexStoreDB",
      dependencies: ["IndexStoreDB_Index"],
      exclude: ["CMakeLists.txt"]
    ),

    // The lmdb database layer.
    .target(
      name: "IndexStoreDB_Database",
      dependencies: [
        "IndexStoreDB_Core",
        .product(name: "CLMDB", package: "swift-lmdb"),
      ],
      exclude: [
        "CMakeLists.txt"
      ]
    ),

    // Core index types.
    .target(
      name: "IndexStoreDB_Core",
      dependencies: ["IndexStoreDB_Support"],
      exclude: ["CMakeLists.txt"]
    ),

    // Support code that is generally useful to the C++ implementation.
    .target(
      name: "IndexStoreDB_Support",
      dependencies: ["IndexStoreDB_LLVMSupport"],
      exclude: ["CMakeLists.txt"]
    ),

    // Copy of a subset of llvm's ADT and Support libraries.
    .target(
      name: "IndexStoreDB_LLVMSupport",
      dependencies: [],
      exclude: [
        "LICENSE.TXT",
        "CMakeLists.txt",
        // *.inc, *.def
        "include/IndexStoreDB_LLVMSupport/llvm_Support_AArch64TargetParser.def",
        "include/IndexStoreDB_LLVMSupport/llvm_Support_ARMTargetParser.def",
        "include/IndexStoreDB_LLVMSupport/llvm_Support_X86TargetParser.def",
        "Unix/Host.inc",
        "Unix/Memory.inc",
        "Unix/Mutex.inc",
        "Unix/Path.inc",
        "Unix/Process.inc",
        "Unix/Program.inc",
        "Unix/Signals.inc",
        "Unix/Threading.inc",
        "Unix/Watchdog.inc",
        "Windows/Host.inc",
        "Windows/Memory.inc",
        "Windows/Mutex.inc",
        "Windows/Path.inc",
        "Windows/Process.inc",
        "Windows/Program.inc",
        "Windows/Signals.inc",
        "Windows/Threading.inc",
        "Windows/Watchdog.inc",
      ]
    ),
  ],
  swiftLanguageModes: [.v5],
  cxxLanguageStandard: .cxx17
)
