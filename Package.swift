// swift-tools-version:5.3

import PackageDescription

let package = Package(
  name: "IndexStoreDB",
  products: [
    .library(
      name: "IndexStoreDB",
      targets: ["IndexStoreDB"]),
    .library(
      name: "IndexStoreDB_CXX",
      targets: ["IndexStoreDB_Index"]),
    .library(
      name: "ISDBTestSupport",
      targets: ["ISDBTestSupport"]),
    .executable(
      name: "tibs",
      targets: ["tibs"])
  ],
  dependencies: [],
  targets: [

    // MARK: Swift interface

    .target(
      name: "IndexStoreDB",
      dependencies: ["IndexStoreDB_CIndexStoreDB"]),

    .testTarget(
      name: "IndexStoreDBTests",
      dependencies: ["IndexStoreDB", "ISDBTestSupport"]),

    // MARK: Swift Test Infrastructure

    // The Test Index Build System (tibs) library.
    .target(
      name: "ISDBTibs",
      dependencies: []),

    .testTarget(
      name: "ISDBTibsTests",
      dependencies: ["ISDBTibs", "ISDBTestSupport"]),

    // Commandline tool for working with tibs projects.
    .target(
      name: "tibs",
      dependencies: ["ISDBTibs"]),

    // Test support library, built on top of tibs.
    .target(
      name: "ISDBTestSupport",
      dependencies: ["IndexStoreDB", "ISDBTibs", "tibs"],
      resources: [
        .copy("INPUTS")
      ]),

    // MARK: C++ interface

    // Primary C++ interface.
    .target(
      name: "IndexStoreDB_Index",
      dependencies: ["IndexStoreDB_Database"],
      path: "lib/Index"),

    // C wrapper for IndexStoreDB_Index.
    .target(
      name: "IndexStoreDB_CIndexStoreDB",
      dependencies: ["IndexStoreDB_Index"],
      path: "lib/CIndexStoreDB"),

    // The lmdb database layer.
    .target(
      name: "IndexStoreDB_Database",
      dependencies: ["IndexStoreDB_Core"],
      path: "lib/Database",
      cSettings: [
        .define("MDB_USE_POSIX_MUTEX", to: "1",
                // Windows does not use POSIX mutex
                .when(platforms: [.linux, .macOS])),
        .define("MDB_USE_ROBUST", to: "0"),
      ]),

    // Core index types.
    .target(
      name: "IndexStoreDB_Core",
      dependencies: ["IndexStoreDB_Support"],
      path: "lib/Core"),

    // Support code that is generally useful to the C++ implementation.
    .target(
      name: "IndexStoreDB_Support",
      dependencies: ["IndexStoreDB_LLVMSupport"],
      path: "lib/Support"),

    // Copy of a subset of llvm's ADT and Support libraries.
    .target(
      name: "IndexStoreDB_LLVMSupport",
      dependencies: [],
      path: "lib/LLVMSupport"),
  ],

  cxxLanguageStandard: .cxx14
)
