import IndexStore

extension IndexStore {
  enum OnlyRecordError: Error {
    case multipleOrNoUnits
    case multipleOrNoRecord
  }

  var onlyRecord: IndexStoreRecord {
    get throws {
      guard let unitName = self.unitNames(sorted: false).map({ $0.string }).only else {
        throw OnlyRecordError.multipleOrNoUnits
      }
      let unit = try IndexStoreStringRef.withStringRef(unitName) { unitName in
        try self.unit(named: unitName)
      }
      let records = unit.dependencies.compactMap { (dependency) -> String? in
        guard dependency.kind == .record else {
          return nil
        }
        return dependency.name.string
      }
      guard let recordName = records.only else {
        throw OnlyRecordError.multipleOrNoRecord
      }
      return try IndexStoreStringRef.withStringRef(recordName) { recordName in
        try self.record(named: recordName)
      }
    }
  }
}
