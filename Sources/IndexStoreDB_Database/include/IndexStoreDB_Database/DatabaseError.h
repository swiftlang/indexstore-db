//===--- DatabaseError.h ----------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_DATABASEERROR_H
#define INDEXSTOREDB_SKDATABASE_DATABASEERROR_H

#include <IndexStoreDB_Support/Visibility.h>
#include <string>
#include <stdexcept>

namespace IndexStoreDB {
namespace db {

class INDEXSTOREDB_EXPORT DatabaseError : public std::runtime_error {
protected:
  const int _code;

public:
  /// Throws an error based on the given return code.
  [[noreturn]] static void raise(const char* origin, int rc);

  DatabaseError(const char* const origin, const int rc) noexcept
    : runtime_error{origin}, _code{rc} {}

  /// Returns the underlying error code.
  int code() const noexcept {
    return _code;
  }

  /// Returns the origin of the error.
  const char* origin() const noexcept {
    return runtime_error::what();
  }

  /// Returns the underlying error message.
  virtual const char* what() const noexcept override;

  std::string description() const noexcept;
};

/// Exception class for `MDB_MAP_FULL` errors.
///
/// @see http://symas.com/mdb/doc/group__errors.html#ga0a83370402a060c9175100d4bbfb9f25
///
class INDEXSTOREDB_EXPORT MapFullError final : public DatabaseError {
  virtual void _anchor();
public:
  using DatabaseError::DatabaseError;
};

} // namespace db
} // namespace IndexStoreDB

#endif
