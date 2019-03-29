//===--- ReadTransaction.h --------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_READTRANSACTION_H
#define INDEXSTOREDB_SKDATABASE_READTRANSACTION_H

#include "IndexStoreDB/Core/Symbol.h"
#include "IndexStoreDB/Database/UnitInfo.h"
#include "llvm/ADT/STLExtras.h"
#include <memory>

namespace IndexStoreDB {
  class CanonicalFilePath;
  class CanonicalFilePathRef;

namespace db {
  class Database;
  typedef std::shared_ptr<Database> DatabaseRef;

class INDEXSTOREDB_EXPORT ReadTransaction {
public:
  explicit ReadTransaction(DatabaseRef dbase);
  ~ReadTransaction();

  /// Returns providers containing the USR with any of the roles.
  /// If both \c roles and \c relatedRoles are given then both any roles and any related roles should be satisfied.
  /// If both \c roles and \c relatedRoles are empty then all providers are returned.
  bool lookupProvidersForUSR(StringRef USR, SymbolRoleSet roles, SymbolRoleSet relatedRoles,
                             llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver);
  bool lookupProvidersForUSR(IDCode usrCode, SymbolRoleSet roles, SymbolRoleSet relatedRoles,
                             llvm::function_ref<bool(IDCode provider, SymbolRoleSet roles, SymbolRoleSet relatedRoles)> receiver);

  StringRef getProviderName(IDCode provider);
  StringRef getTargetName(IDCode target);
  StringRef getModuleName(IDCode moduleName);
  bool getProviderFileReferences(IDCode provider,
                                 llvm::function_ref<bool(TimestampedPath path)> receiver);
  bool getProviderFileCodeReferences(IDCode provider,
                                     llvm::function_ref<bool(IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver);
  /// Returns all provider-file associations. Intended for debugging purposes.
  bool foreachProviderAndFileCodeReference(llvm::function_ref<bool(IDCode provider, IDCode pathCode, IDCode unitCode, llvm::sys::TimePoint<> modTime, IDCode moduleNameCode, bool isSystem)> receiver);

  /// Returns USR codes in batches.
  bool foreachUSROfGlobalSymbolKind(SymbolKind symKind, llvm::function_ref<bool(ArrayRef<IDCode> usrCodes)> receiver);

  /// Returns USR codes in batches.
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
  CanonicalFilePath getFullFilePathFromCode(IDCode filePathCode);
  CanonicalFilePathRef getDirectoryFromCode(IDCode dirCode);

  bool foreachDirPath(llvm::function_ref<bool(CanonicalFilePathRef dirPath)> receiver);

  /// The memory that \c filePath points to may not live beyond the receiver function invocation.
  bool findFilePathsWithParentPaths(ArrayRef<CanonicalFilePathRef> parentPaths,
                                    llvm::function_ref<bool(IDCode pathCode, CanonicalFilePathRef filePath)> receiver);

  IDCode getFilePathCode(CanonicalFilePathRef filePath);

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

  class Implementation;
private:
  std::unique_ptr<Implementation> Impl;
};

} // namespace db
} // namespace IndexStoreDB

#endif
