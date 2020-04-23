//===--- IDCode.h -----------------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_IDCODE_H
#define INDEXSTOREDB_SKDATABASE_IDCODE_H

#include <functional>
#include <cstdint>

namespace IndexStoreDB {
namespace db {

class IDCode {
  uint64_t Code{};
  explicit IDCode(uint64_t code) : Code(code) {}

public:
  IDCode() = default;

  static IDCode fromValue(uint64_t code) {
    return IDCode(code);
  }

  uint64_t value() const { return Code; }

  friend bool operator ==(IDCode lhs, IDCode rhs) {
    return lhs.Code == rhs.Code;
  }
  friend bool operator !=(IDCode lhs, IDCode rhs) {
    return !(lhs == rhs);
  }

  static int compare(IDCode lhs, IDCode rhs) {
    if (lhs.value() < rhs.value()) return -1;
    if (lhs.value() > rhs.value()) return 1;
    return 0;
  }
};

} // namespace db
} // namespace IndexStoreDB

namespace std {
template <> struct hash<IndexStoreDB::db::IDCode> {
  size_t operator()(const IndexStoreDB::db::IDCode &k) const {
    return k.value();
  }
};
}

#endif
