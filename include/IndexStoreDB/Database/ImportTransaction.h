//===--- ImportTransaction.h ------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_IMPORTTRANSACTION_H
#define INDEXSTOREDB_SKDATABASE_IMPORTTRANSACTION_H

#include "IndexStoreDB/Core/Symbol.h"
#include "IndexStoreDB/Database/UnitInfo.h"
#include "IndexStoreDB/Support/Path.h"
#include <memory>
#include <unordered_set>

namespace IndexStoreDB {
namespace db {
  class Database;
  typedef std::shared_ptr<Database> DatabaseRef;

class INDEXSTOREDB_EXPORT ImportTransaction {
public:
  explicit ImportTransaction(DatabaseRef dbase);
  ~ImportTransaction();

  IDCode getUnitCode(StringRef unitName);
  IDCode addProviderName(StringRef name, bool *wasInserted = nullptr);
  // Marks a provider as containing test symbols.
  void setProviderContainsTestSymbols(IDCode provider);
  bool providerContainsTestSymbols(IDCode provider);
  /// \returns a IDCode of the USR.
  IDCode addSymbolInfo(IDCode provider,
                       StringRef USR, StringRef symbolName, SymbolInfo symInfo,
                       SymbolRoleSet roles, SymbolRoleSet relatedRoles);
  IDCode addFilePath(CanonicalFilePathRef filePath);
  IDCode addUnitFileIdentifier(StringRef unitFile);

  void removeUnitData(IDCode unitCode);
  void removeUnitData(StringRef unitName);

  void commit();

  class Implementation;
  Implementation *_impl() const { return Impl.get(); }
private:
  std::unique_ptr<Implementation> Impl;
};

class INDEXSTOREDB_EXPORT UnitDataImport {
  ImportTransaction &Import;
  std::string UnitName;
  CanonicalFilePath MainFile;
  StringRef OutFile;
  CanonicalFilePath Sysroot;
  llvm::sys::TimePoint<> ModTime;
  Optional<bool> IsSystem;
  Optional<bool> HasTestSymbols;
  Optional<SymbolProviderKind> SymProviderKind;
  std::string Target;

  IDCode UnitCode;
  bool IsMissing;
  bool IsUpToDate;
  IDCode PrevMainFileCode;
  IDCode PrevOutFileCode;
  IDCode PrevSysrootCode;
  IDCode PrevTargetCode;
  std::unordered_set<IDCode> PrevCombinedFileDepends; // Combines record and non-record file dependencies.
  std::unordered_set<IDCode> PrevUnitDepends;
  std::unordered_set<UnitInfo::Provider> PrevProviderDepends;

  std::vector<IDCode> FileDepends;
  std::vector<IDCode> UnitDepends;
  std::vector<UnitInfo::Provider> ProviderDepends;

public:
  UnitDataImport(ImportTransaction &import, StringRef unitName, llvm::sys::TimePoint<> modTime);
  ~UnitDataImport();

  IDCode getUnitCode() const { return UnitCode; }

  bool isMissing() const { return IsMissing; }
  bool isUpToDate() const { return IsUpToDate; }
  Optional<bool> getIsSystem() const { return IsSystem; }
  Optional<bool> getHasTestSymbols() const { return HasTestSymbols; }
  Optional<SymbolProviderKind> getSymbolProviderKind() const { return SymProviderKind; }

  IDCode getPrevMainFileCode() const {
    assert(!IsMissing);
    return PrevMainFileCode;
  }
  IDCode getPrevOutFileCode() const {
    assert(!IsMissing);
    return PrevOutFileCode;
  }

  void setMainFile(CanonicalFilePathRef mainFile);
  void setOutFile(StringRef outFile);
  void setSysroot(CanonicalFilePathRef sysroot);
  void setIsSystemUnit(bool isSystem);
  void setSymbolProviderKind(SymbolProviderKind K);
  void setTarget(StringRef target);

  IDCode addFileDependency(CanonicalFilePathRef filePathDep);
  IDCode addUnitDependency(StringRef unitNameDep);
  /// \returns the provider code.
  IDCode addProviderDependency(StringRef providerName, CanonicalFilePathRef filePathDep, StringRef moduleName, bool isSystem, bool *isNewProvider = nullptr);

  void commit();
};

} // namespace db
} // namespace IndexStoreDB

#endif
