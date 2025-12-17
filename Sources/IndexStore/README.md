# IndexStore Swift library

The `IndexStore` Swift library is a wrapper around `libIndexStore.dylib` to iterate through an Index Store. An understanding of the Index Store’s structure is assumed to use this library. Read [Index Store.md](Index%20Store.md) for an overview.

One thing to note is that the `IndexStore` library does not provide efficient query access into the Index Store, its purpose is to iterate through an Index Store.

Furthermore, the `IndexStore` is designed to provide type-safe access to an Index Store with the least to no overhead over the C API provided by `libIndexStore.dylib`. Speed is generally favored over the most convenient APIs, which in particular, manifests in non-Escapable types and the existence of `forEach` methods to iterate over collections.

## Example

The following iterates through all records inside an Index Store to print the definitions within them.

```swift
// Get a reference to the IndexStore library that is used to read Index Stores
let indexStoreLibrary = try await IndexStoreLibrary.at(dylibPath: URL(filePath: "/path/to/usr/lib/libIndexStore.dylib"))

// Open an Index Store
let indexStore = try indexStoreLibrary.indexStore(at: "/path/to/index/store")

// Iterate through all unit names and retrieve the record names within the Index Store
let recordNames = try indexStore.unitNames(sorted: false).map { unit in
  let unit = try indexStore.unit(named: unit)
  return unit.dependencies.compactMap { dependency in
    if dependency.kind == .record {
      return dependency.name.string
    }
    return nil
  }
}.flatMap(\.self)

// Iterate through all record names and print the definitions within them.
for recordName in recordNames {
  let record = try indexStore.record(named: recordName)
  record.occurrences.forEach { occurrence in
    guard occurrence.roles.contains(.definition) else {
      return .continue
    }
    print("\(occurrence.position.line):\(occurrence.position.column): \(occurrence.symbol.name.string)")
    return .continue
  }
}
```
