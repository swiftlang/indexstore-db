//===--- FileVisibilityChecker.h --------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_LIB_INDEX_FILEVISIBILITYCHECKER_H
#define INDEXSTOREDB_LIB_INDEX_FILEVISIBILITYCHECKER_H

#include "IndexStoreDB/Database/IDCode.h"
#include "IndexStoreDB/Support/LLVM.h"
#include "llvm/Support/Mutex.h"
#include <unordered_map>
#include <unordered_set>

namespace IndexStoreDB {
  class CanonicalPathCache;

namespace db {
  class IDCode;
  class ReadTransaction;
  class Database;
  typedef std::shared_ptr<Database> DatabaseRef;
  struct UnitInfo;
}

namespace index {

class FileVisibilityChecker {
  db::DatabaseRef DBase;
  std::shared_ptr<CanonicalPathCache> CanonPathCache;

  mutable llvm::sys::Mutex VisibleCacheMtx;
  std::unordered_set<db::IDCode> VisibleMainFiles;
  std::unordered_map<db::IDCode, unsigned> MainFilesRefCount;
  std::unordered_map<db::IDCode, bool> UnitVisibilityCache;

public:
  FileVisibilityChecker(db::DatabaseRef dbase,
                        std::shared_ptr<CanonicalPathCache> canonPathCache);

  void registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);
  void unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);

  bool isUnitVisible(const db::UnitInfo &unitInfo, db::ReadTransaction &reader);
};

} // namespace index
} // namespace IndexStoreDB

#endif
