//===--- UnitInfo.h ---------------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SKDATABASE_UNITINFO_H
#define INDEXSTOREDB_SKDATABASE_UNITINFO_H

#include "IndexStoreDB/Database/IDCode.h"
#include "IndexStoreDB/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/TimeValue.h"

namespace IndexStoreDB {
namespace db {

struct UnitInfo {
  struct Provider {
    IDCode ProviderCode;
    IDCode FileCode;

    friend bool operator ==(const Provider &lhs, const Provider &rhs) {
      return lhs.ProviderCode == rhs.ProviderCode && lhs.FileCode == rhs.FileCode;
    }
    friend bool operator !=(const Provider &lhs, const Provider &rhs) {
      return !(lhs == rhs);
    }
  };

  StringRef UnitName;
  IDCode UnitCode;
  llvm::sys::TimeValue ModTime;
  IDCode OutFileCode;
  IDCode MainFileCode;
  IDCode SysrootCode;
  IDCode TargetCode;
  bool HasMainFile;
  bool HasSysroot;
  bool IsSystem;
  SymbolProviderKind SymProviderKind;
  ArrayRef<IDCode> FileDepends;
  ArrayRef<IDCode> UnitDepends;
  ArrayRef<Provider> ProviderDepends;

  bool isInvalid() const { return UnitName.empty(); }
  bool isValid() const { return !isInvalid(); }
};

} // namespace db
} // namespace IndexStoreDB

namespace std {
template <> struct hash<IndexStoreDB::db::UnitInfo::Provider> {
  size_t operator()(const IndexStoreDB::db::UnitInfo::Provider &k) const {
    return llvm::hash_combine(k.FileCode.value(), k.ProviderCode.value());
  }
};
}

#endif
