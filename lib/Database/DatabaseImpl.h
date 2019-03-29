//===--- DatabaseImpl.h -----------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_LIB_DATABASEIMPL_H
#define INDEXSTOREDB_SKDATABASE_LIB_DATABASEIMPL_H

#include "IndexStoreDB/Database/Database.h"
#include "lmdb/lmdb++.h"
#include <dispatch/dispatch.h>

namespace IndexStoreDB {
  enum class SymbolKind : uint8_t;

namespace db {
  struct UnitInfo;

class Database::Implementation {
  lmdb::env DBEnv{nullptr};
  lmdb::dbi DBISymbolProvidersByUSR{0};
  lmdb::dbi DBISymbolProviderNameByCode{0};
  lmdb::dbi DBIUSRsBySymbolName{0};
  lmdb::dbi DBIUSRsByGlobalSymbolKind{0};
  lmdb::dbi DBIDirNameByCode{0};
  lmdb::dbi DBIFilenameByCode{0};
  lmdb::dbi DBIFilePathCodesByDir{0};
  lmdb::dbi DBITimestampedFilesByProvider{0};
  lmdb::dbi DBIUnitInfoByCode{0};
  lmdb::dbi DBIUnitByFileDependency{0};
  lmdb::dbi DBIUnitByUnitDependency{0};
  lmdb::dbi DBITargetNameByCode{0};
  lmdb::dbi DBIModuleNameByCode{0};
  size_t MaxKeySize;
  mdb_size_t MapSize;

  dispatch_group_t ReadTxnGroup;
  dispatch_queue_t TxnSyncQueue;
  dispatch_queue_t DiscardedDBsCleanupQueue;

  bool IsReadOnly;
  std::string VersionedPath;
  std::string SavedPath;
  std::string ProcessPath;

public:
  static std::shared_ptr<Implementation> create(StringRef dbPath, bool readonly, Optional<size_t> initialDBSize, std::string &error);

  Implementation();
  ~Implementation();

  lmdb::env &getDBEnv() { return DBEnv; }
  lmdb::dbi &getDBISymbolProvidersByUSR() { return DBISymbolProvidersByUSR; }
  lmdb::dbi &getDBISymbolProviderNameByCode() { return DBISymbolProviderNameByCode; }
  lmdb::dbi &getDBIUSRsBySymbolName() { return DBIUSRsBySymbolName; }
  lmdb::dbi &getDBIUSRsByGlobalSymbolKind() { return DBIUSRsByGlobalSymbolKind; }
  lmdb::dbi &getDBIDirNameByCode() { return DBIDirNameByCode; }
  lmdb::dbi &getDBIFilenameByCode() { return DBIFilenameByCode; }
  lmdb::dbi &getDBIFilePathCodesByDir() { return DBIFilePathCodesByDir; }
  lmdb::dbi &getDBITimestampedFilesByProvider() { return DBITimestampedFilesByProvider; }
  lmdb::dbi &getDBIUnitInfoByCode() { return DBIUnitInfoByCode; }
  lmdb::dbi &getDBIUnitByFileDependency() { return DBIUnitByFileDependency; }
  lmdb::dbi &getDBIUnitByUnitDependency() { return DBIUnitByUnitDependency; }
  lmdb::dbi &getDBITargetNameByCode() { return DBITargetNameByCode; }
  lmdb::dbi &getDBIModuleNameByCode() { return DBIModuleNameByCode; }
  size_t getMaxKeySize() const { return MaxKeySize; }

  /// UnitInfo.UnitName will be empty if \c unit was not found. UnitInfo.UnitCode is always filled out.
  UnitInfo getUnitInfo(IDCode unitCode, lmdb::txn &Txn);

  void enterReadTransaction();
  void exitReadTransaction();

  void increaseMapSize();

  void cleanupDiscardedDBs();

  void printStats(raw_ostream &OS);
};

enum class GlobalSymbolKind : unsigned {
  Class = 1,
  Protocol = 2,
  Function = 3,
  Struct = 4,
  Union = 5,
  Enum = 6,
  Type = 7,
  GlobalVar = 8,
  TestClassOrExtension = 9,
  TestMethod = 10,
  CommentTag = 11,
};

Optional<GlobalSymbolKind> getGlobalSymbolKind(SymbolKind K);

struct ProviderForUSRData {
  IDCode ProviderCode;
  uint64_t Roles;
  uint64_t RelatedRoles;
};

struct TimestampedFileForProviderData {
  IDCode FileCode;
  IDCode UnitCode;
  IDCode ModuleNameCode;
  uint64_t NanoTime;
  bool IsSystem;
};

struct UnitInfoData {
  struct Provider {
    IDCode ProviderCode;
    IDCode FileCode;
  };

  struct Include {
    IDCode SourceCode;
    unsigned Line;
    IDCode TargetCode;
  };

  IDCode MainFileCode;
  IDCode OutFileCode;
  IDCode SysrootCode;
  IDCode TargetCode;
  int64_t NanoTime;
  uint16_t NameLength;
  uint8_t SymProviderKind;
  bool HasMainFile : 1;
  bool HasSysroot : 1;
  bool IsSystem : 1;
  uint32_t FileDependSize;
  uint32_t UnitDependSize;
  uint32_t ProviderDependSize;

  // Follows:
  //  - Array of IDCodes for file dependencies
  //  - Array of IDCodes for unit dependencies
  //  - Array of UnitInfoData::Provider for provider dependencies
  //  - The name string buffer.
};

} // namespace db
} // namespace IndexStoreDB

#endif
