//===--- DatabaseError.cpp ------------------------------------------------===//
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

#include "IndexStoreDB/Database/DatabaseError.h"
#include "lmdb/lmdb++.h"
#include "llvm/Support/raw_ostream.h"

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;

void DatabaseError::raise(const char* const origin, const int rc) {
  // Move exceptions from 'lmdb++.h' to 'DatabaseError.h' as needed.
  switch (rc) {
    case MDB_KEYEXIST:         throw lmdb::key_exist_error{origin, rc};
    case MDB_NOTFOUND:         throw lmdb::not_found_error{origin, rc};
    case MDB_CORRUPTED:        throw lmdb::corrupted_error{origin, rc};
    case MDB_PANIC:            throw lmdb::panic_error{origin, rc};
    case MDB_VERSION_MISMATCH: throw lmdb::version_mismatch_error{origin, rc};
    case MDB_MAP_FULL:         throw MapFullError{origin, rc};
#ifdef MDB_BAD_DBI
    case MDB_BAD_DBI:          throw lmdb::bad_dbi_error{origin, rc};
#endif
    default:                   throw lmdb::runtime_error{origin, rc};
  }
}

const char* DatabaseError::what() const noexcept {
  return ::mdb_strerror(code());
}

std::string DatabaseError::description() const noexcept {
  std::string desc;
  llvm::raw_string_ostream OS(desc);
  OS << origin() << ": " << what();
  return OS.str();
}

void MapFullError::_anchor() {}
