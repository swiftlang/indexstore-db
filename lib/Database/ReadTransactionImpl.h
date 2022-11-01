//===--- ReadTransactionImpl.h ----------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_LIB_READTRANSACTIONIMPL_H
#define INDEXSTOREDB_SKDATABASE_LIB_READTRANSACTIONIMPL_H

#include "IndexStoreDB/Database/ReadTransaction.h"
#include "DatabaseImpl.h"
#include "lmdb/lmdb++.h"
#include <unordered_set>

namespace IndexStoreDB {
namespace db {

class ReadTransactionGuard {
  DatabaseRef DBase;
public:
  explicit ReadTransactionGuard(DatabaseRef dbase);
  ~ReadTransactionGuard();
};

class ReadTransaction::Implementation {
  DatabaseRef DBase;
  // This needs to be before 'Txn' so that it gets destructed after it.
  ReadTransactionGuard TxnGuard;
  lmdb::txn Txn{nullptr};

public:
  explicit Implementation(DatabaseRef dbase);

  bool lookupProvidersForUSR(StringRef USR, SymbolRoleSet roles, SymbolRoleSet relatedRoles,
                             llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver);
  bool lookupProvidersForUSR(IDCode usrCode, SymbolRoleSet roles, SymbolRoleSet relatedRoles,
                             llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver);

  IDCode getUSRCode(StringRef USR);
  IDCode getProviderCode(StringRef providerName);
  StringRef getProviderName(IDCode provider);
  StringRef getTargetName(IDCode target);
  StringRef getModuleName(IDCode moduleName);
  bool getProviderFileReferences(IDCode provider,
                                 llvm::function_ref<bool(TimestampedPath path)> receiver);
  /// `unitFilter` returns `true` if the unit should be included, `false` if it should be ignored.
  bool getProviderFileCodeReferences(IDCode provider,
    function_ref<bool(IDCode unitCode)> unitFilter,
    function_ref<bool(IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver);
  /// `unitFilter` returns `true` if the unit should be included, `false` if it should be ignored.
  bool foreachProviderAndFileCodeReference(function_ref<bool(IDCode unitCode)> unitFilter,
    function_ref<bool(IDCode provider, IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver);

  bool foreachProviderContainingTestSymbols(function_ref<bool(IDCode provider)> receiver);

  bool foreachUSROfGlobalSymbolKind(SymbolKind symKind, llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver);
  bool foreachUSROfGlobalUnitTestSymbol(llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver);
  bool foreachUSROfGlobalSymbolKind(GlobalSymbolKind globalSymKind, function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver);

  bool findUSRsWithNameContaining(StringRef pattern,
                                  bool anchorStart, bool anchorEnd,
                                  bool subsequence, bool ignoreCase,
                                  llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver);
  bool foreachUSRBySymbolName(StringRef name, llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver);

  /// The memory that \c filePath points to may not live beyond the receiver function invocation.
  bool findFilenamesContaining(StringRef pattern,
                               bool anchorStart, bool anchorEnd,
                               bool subsequence, bool ignoreCase,
                               llvm::function_ref<bool(CanonicalFilePathRef filePath)> receiver);

  /// Returns all the recorded symbol names along with their associated USRs.
  bool foreachSymbolName(function_ref<bool(StringRef name)> receiver);

  /// Returns true if it was found, false otherwise.
  bool getFullFilePathFromCode(IDCode filePathCode, raw_ostream &OS);
  /// Returns empty path if it was not found.
  CanonicalFilePath getFullFilePathFromCode(IDCode filePathCode);
  CanonicalFilePathRef getDirectoryFromCode(IDCode dirCode);
  /// Returns empty path if it was not found. This should only be used for the unit path since it is not treated as
  ///  a canonicalized path.
  StringRef getUnitFileIdentifierFromCode(IDCode fileCode);
  bool foreachDirPath(llvm::function_ref<bool(CanonicalFilePathRef dirPath)> receiver);
  bool findFilePathsWithParentPaths(ArrayRef<CanonicalFilePathRef> parentPaths,
                                    llvm::function_ref<bool(IDCode pathCode, CanonicalFilePathRef filePath)> receiver);
  IDCode getFilePathCode(CanonicalFilePathRef filePath);
  IDCode getUnitPathCode(StringRef filePath);

  /// UnitInfo.UnitName will be empty if \c unit was not found. UnitInfo.UnitCode is always filled out.
  UnitInfo getUnitInfo(IDCode unitCode);
  /// UnitInfo.UnitName will be empty if \c unit was not found. UnitInfo.UnitCode is always filled out.
  UnitInfo getUnitInfo(StringRef unitName);
  bool foreachUnitContainingFile(IDCode filePathCode,
                                 llvm::function_ref<bool(ArrayRef<IDCode> unitCodes)> receiver);
  bool foreachUnitContainingUnit(IDCode unitCode,
                                 llvm::function_ref<bool(ArrayRef<IDCode> unitCodes)> receiver);
  bool foreachRootUnitOfFile(IDCode filePathCode,
                             function_ref<bool(const UnitInfo &unitInfo)> receiver);
  bool foreachRootUnitOfUnit(IDCode unitCode,
                             function_ref<bool(const UnitInfo &unitInfo)> receiver);
  void getDirectDependentUnits(IDCode unitCode, SmallVectorImpl<IDCode> &units);

  // Get info about whole databases.
  void dumpUnitByFilePair();

private:
  std::pair<IDCode, StringRef> decomposeFilePathValue(lmdb::val &filePathValue);
  bool getFilePathFromValue(lmdb::val &filePathValue, raw_ostream &OS);
  CanonicalFilePath getFilePathFromValue(lmdb::val &filePathValue);
  void collectRootUnits(IDCode unitCode,
                        SmallVectorImpl<UnitInfo> &rootUnits,
                        std::unordered_set<IDCode> &visited);
};

} // namespace db
} // namespace IndexStoreDB

#endif
