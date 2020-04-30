//===--- Path.h -------------------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SUPPORT_PATH_H
#define INDEXSTOREDB_SUPPORT_PATH_H

#include "IndexStoreDB/Support/LLVM.h"
#include "IndexStoreDB/Support/Visibility.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Path.h"

namespace IndexStoreDB {
  class CanonicalFilePathRef;

class CanonicalFilePath {
  std::string Path;

public:
  CanonicalFilePath() = default;
  inline CanonicalFilePath(CanonicalFilePathRef CanonPath);

  const std::string &getPath() const { return Path; }
  bool empty() const { return Path.empty(); }

  friend bool operator==(CanonicalFilePath LHS, CanonicalFilePath RHS) {
    return LHS.Path == RHS.Path;
  }
  friend bool operator!=(CanonicalFilePath LHS, CanonicalFilePath RHS) {
    return !(LHS == RHS);
  }
  friend bool operator<(CanonicalFilePath LHS, CanonicalFilePath RHS) {
    return LHS.Path < RHS.Path;
  }
};

class CanonicalFilePathRef {
  StringRef Path;

public:
  CanonicalFilePathRef() = default;
  CanonicalFilePathRef(const CanonicalFilePath &CanonPath)
    : Path(CanonPath.getPath()) {}

  static CanonicalFilePathRef getAsCanonicalPath(StringRef Path) {
    CanonicalFilePathRef CanonPath;
    CanonPath.Path = Path;
    return CanonPath;
  }

  StringRef getPath() const { return Path; }
  bool empty() const { return Path.empty(); }
  size_t size() const { return Path.size(); }

  friend bool operator==(CanonicalFilePathRef LHS, CanonicalFilePathRef RHS) {
    return LHS.Path == RHS.Path;
  }
  friend bool operator!=(CanonicalFilePathRef LHS, CanonicalFilePathRef RHS) {
    return !(LHS == RHS);
  }

  bool contains(CanonicalFilePathRef other) {
    if (empty() || !other.Path.startswith(Path))
      return false;
    auto rest = other.Path.drop_front(size());
    return !rest.empty() && llvm::sys::path::is_separator(rest.front());
  }
};

inline CanonicalFilePath::CanonicalFilePath(CanonicalFilePathRef CanonPath)
  : Path(CanonPath.getPath()) {}

class INDEXSTOREDB_EXPORT CanonicalPathCache {
  void *Impl;

public:
  CanonicalPathCache();
  ~CanonicalPathCache();

  CanonicalFilePath getCanonicalPath(StringRef Path,
                                     StringRef WorkingDir = StringRef());
};

} // namespace IndexStoreDB

#endif
