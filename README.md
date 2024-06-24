# IndexStoreDB

IndexStoreDB is a source code indexing library. It provides a composable and efficient query API for looking up source code symbols, symbol occurrences, and relations. IndexStoreDB uses the libIndexStore library, which lives in [apple/llvm-project](https://github.com/apple/llvm-project/tree/apple/main/clang/tools/IndexStore), for reading raw index data. Raw index data can be produced by compilers such as Apple Clang and Swift using the `-index-store-path` option. IndexStoreDB enables efficiently querying this data by maintaining acceleration tables in a key-value database built with [LMDB](http://www.lmdb.tech/).

IndexStoreDB's data model is derived from libIndexStore. For more information about libIndexStore, and producing raw indexing data, see the [Indexing While Building](https://docs.google.com/document/d/1cH2sTpgSnJZCkZtJl1aY-rzy4uGPcrI-6RrUpdATO2Q/) whitepaper.

## Building IndexStoreDB

IndexStoreDB is built using the [Swift Package Manager](https://github.com/apple/swift-package-manager).

For a standard debug build and test:

```sh
$ swift build
$ swift test
```

### Building on Linux

The C++ code in the index requires `libdispatch`, but unlike Swift code, it cannot find it automatically on Linux. You can work around this by adding a search path manually.

```sh
$ swift build -Xcxx -I<path_to_swift_toolchain>/usr/lib/swift -Xcxx -I<path_to_swift_toolchain>/usr/lib/swift/Block
```

## Some Example Users

[**Pecker**](https://github.com/woshiccm/Pecker): a tool to detect unused code based on [SwiftSyntax](https://github.com/apple/swift-syntax.git) and [IndexStoreDB](https://github.com/apple/indexstore-db.git).

## Contributing to Swift

Contributions to indexstore-db are welcomed and encouraged! Please see the
[Contributing to Swift guide](https://swift.org/contributing/).

Before submitting the pull request, please make sure you have [tested your
 changes](https://github.com/apple/swift/blob/main/docs/ContinuousIntegration.md)
 and that they follow the Swift project [guidelines for contributing
 code](https://swift.org/contributing/#contributing-code).

To be a truly great community, [Swift.org](https://swift.org/) needs to welcome
developers from all walks of life, with different backgrounds, and with a wide
range of experience. A diverse and friendly community will have more great
ideas, more unique perspectives, and produce more great code. We will work
diligently to make the Swift community welcoming to everyone.

To give clarity of what is expected of our members, Swift has adopted the
code of conduct defined by the Contributor Covenant. This document is used
across many open source communities, and we think it articulates our values
well. For more, see the [Code of Conduct](https://swift.org/code-of-conduct/).

## Development

For more information about developing IndexStoreDB, see [Development](Documentation/Development.md).
