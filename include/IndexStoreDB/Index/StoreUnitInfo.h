//===--- StoreUnitInfo.h ----------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_INDEX_STOREUNITINFO_H
#define INDEXSTOREDB_INDEX_STOREUNITINFO_H

#include "IndexStoreDB/Support/Path.h"
#include "llvm/Support/Chrono.h"
#include <string>

namespace IndexStoreDB {
namespace index {

struct StoreUnitInfo {
  std::string UnitName;
  CanonicalFilePath MainFilePath;
  std::string OutFileIdentifier;
  bool HasTestSymbols = false;
  llvm::sys::TimePoint<> ModTime;

  StoreUnitInfo() = default;
  StoreUnitInfo(std::string unitName, CanonicalFilePath mainFilePath,
                StringRef outFileIdentifier, bool hasTestSymbols,
                llvm::sys::TimePoint<> modTime)
      : UnitName(unitName),
        MainFilePath(mainFilePath),
        OutFileIdentifier(outFileIdentifier),
        HasTestSymbols(hasTestSymbols),
        ModTime(modTime) {}
};

} // namespace index
} // namespace IndexStoreDB

#endif
