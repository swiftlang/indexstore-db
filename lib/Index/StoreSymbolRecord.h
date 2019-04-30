//===--- StoreSymbolRecord.h ------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef INDEXSTOREDB_LIB_INDEX_STORESYMBOLRECORD_H
#define INDEXSTOREDB_LIB_INDEX_STORESYMBOLRECORD_H

#include "IndexStoreDB/Core/Symbol.h"
#include "IndexStoreDB/Index/SymbolDataProvider.h"
#include "IndexStoreDB/Database/IDCode.h"
#include "indexstore/IndexStoreCXX.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace IndexStoreDB {
  class CanonicalFilePathRef;
  class TimestampedPath;

namespace index {
  class StoreSymbolRecord;
  typedef std::shared_ptr<StoreSymbolRecord> StoreSymbolRecordRef;

struct FileAndTarget {
  TimestampedPath Path;
  std::string Target;
};

class StoreSymbolRecord : public SymbolDataProvider {
  indexstore::IndexStoreRef Store;
  std::string RecordName;
  db::IDCode ProviderCode;
  SymbolProviderKind SymProviderKind;
  std::vector<FileAndTarget> FileAndTargetRefs;

public:
  ~StoreSymbolRecord();

  static StoreSymbolRecordRef create(indexstore::IndexStoreRef store,
                                     StringRef recordName, db::IDCode providerCode,
                                     SymbolProviderKind symProviderKind,
                                     ArrayRef<FileAndTarget> fileReferences);

  StringRef getName() const { return RecordName; }

  SymbolProviderKind getProviderKind() const {
    return SymProviderKind;
  }

  ArrayRef<FileAndTarget> getSourceFileReferencesAndTargets() const {
    return FileAndTargetRefs;
  }

  /// \returns true for error.
  bool doForData(function_ref<void(indexstore::IndexRecordReader &)> Action);

  //===--------------------------------------------------------------------===//
  // SymbolDataProvider interface
  //===--------------------------------------------------------------------===//

  virtual std::string getIdentifier() const override { return RecordName; }

  virtual bool isSystem() const override;

  virtual bool foreachCoreSymbolData(function_ref<bool(StringRef USR,
                                                       StringRef Name,
                                                       SymbolInfo Info,
                                                       SymbolRoleSet Roles,
                                                       SymbolRoleSet RelatedRoles)> Receiver) override;

  virtual bool foreachSymbolOccurrenceByUSR(ArrayRef<db::IDCode> USRs,
                                            SymbolRoleSet RoleSet,
               function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) override;

  virtual bool foreachRelatedSymbolOccurrenceByUSR(ArrayRef<db::IDCode> USRs,
                                            SymbolRoleSet RoleSet,
               function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) override;

  virtual bool foreachUnitTestSymbolOccurrence(
               function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) override;

};

} // namespace index
} // namespace IndexStoreDB

#endif
