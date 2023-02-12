//===--- ImportTransactionImpl.h --------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_LIB_IMPORTTRANSACTIONIMPL_H
#define INDEXSTOREDB_SKDATABASE_LIB_IMPORTTRANSACTIONIMPL_H

#include "IndexStoreDB/Database/ImportTransaction.h"
#include "IndexStoreDB/Database/UnitInfo.h"
#include "lmdb/lmdb++.h"

namespace IndexStoreDB {
namespace db {

class ImportTransaction::Implementation {
public:
  DatabaseRef DBase;
  lmdb::txn Txn{nullptr};

  explicit Implementation(DatabaseRef dbase);

  IDCode getUnitCode(StringRef unitName);
  IDCode addProviderName(StringRef name, bool *wasInserted);
  // Marks a provider as containing test symbols.
  void setProviderContainsTestSymbols(IDCode provider);
  bool providerContainsTestSymbols(IDCode provider);
  /// \returns a IDCode of the USR.
  IDCode addSymbolInfo(IDCode provider, StringRef USR, StringRef symbolName, SymbolInfo symInfo,
                       SymbolRoleSet roles, SymbolRoleSet relatedRoles);
  IDCode addFilePath(CanonicalFilePathRef canonFilePath);
  IDCode addDirectory(CanonicalFilePathRef directory);
  IDCode addUnitFileIdentifier(StringRef unitFile);
  IDCode addTargetName(StringRef target);
  IDCode addModuleName(StringRef moduleName);
  /// If file is already associated, its timestamp is updated if \c modTime is more recent.
  void addFileAssociationForProvider(IDCode provider, IDCode file, IDCode unit, llvm::sys::TimePoint<> modTime, IDCode module, bool isSystem);
  /// \returns true if there is no remaining file reference, false otherwise.
  bool removeFileAssociationFromProvider(IDCode provider, IDCode file, IDCode unit);

  /// UnitInfo.UnitName will be empty if \c unit was not found. UnitInfo.UnitCode is always filled out.
  UnitInfo getUnitInfo(IDCode unitCode);

  void addUnitInfo(const UnitInfo &unitInfo);
  /// \returns the IDCode for the file path.
  IDCode addUnitFileDependency(IDCode unitCode, CanonicalFilePathRef filePathDep);
  /// \returns the IDCode for the unit name.
  IDCode addUnitUnitDependency(IDCode unitCode, StringRef unitNameDep);

  void removeUnitFileDependency(IDCode unitCode, IDCode pathCode);
  void removeUnitUnitDependency(IDCode unitCode, IDCode unitDepCode);
  void removeUnitData(IDCode unitCode);
  void removeUnitData(StringRef unitName);

  void commit();

private:
  IDCode addFilePath(StringRef filePath);
};

} // namespace db
} // namespace IndexStoreDB

#endif
