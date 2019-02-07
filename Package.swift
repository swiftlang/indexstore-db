// swift-tools-version:4.2

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
  ],
  dependencies: [],
  targets: [

    // MARK: Swift interface

    .target(
      name: "IndexStoreDB",
      dependencies: ["IndexStoreDB_CIndexStoreDB"]),

    .testTarget(
      name: "IndexStoreDBTests",
      dependencies: ["IndexStoreDB"]),

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
      path: "lib/Database"),

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

  cxxLanguageStandard: .cxx11
)
