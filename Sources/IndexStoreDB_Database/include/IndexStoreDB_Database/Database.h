//===--- Database.h ---------------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_DATABASE_H
#define INDEXSTOREDB_SKDATABASE_DATABASE_H

#include <IndexStoreDB_Database/IDCode.h>
#include <IndexStoreDB_Support/LLVM.h>
#include <IndexStoreDB_Support/Visibility.h>
#include <memory>
#include <string>

namespace IndexStoreDB {
namespace db {
  class Database;
  typedef std::shared_ptr<Database> DatabaseRef;

class INDEXSTOREDB_EXPORT Database {
public:
  static DatabaseRef create(StringRef dbPath, bool readonly, Optional<size_t> initialDBSize, std::string &error);
  ~Database();

  void increaseMapSize();

  void printStats(raw_ostream &OS);

  class Implementation;
private:
  std::shared_ptr<Implementation> Impl;

public:
  // This is public for easier access from underlying code.
  Implementation &impl() { return *Impl; }

  // This is public for testing.
  static const unsigned DATABASE_FORMAT_VERSION;
};

INDEXSTOREDB_EXPORT IDCode makeIDCodeFromString(StringRef name);

} // namespace db
} // namespace IndexStoreDB

#endif
