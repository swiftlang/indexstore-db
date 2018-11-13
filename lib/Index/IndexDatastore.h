//===--- IndexDatastore.h ---------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_LIB_INDEX_INDEXDATASTORE_H
#define INDEXSTOREDB_LIB_INDEX_INDEXDATASTORE_H

#include "IndexStoreDB/Core/Symbol.h"
#include "llvm/ADT/OptionSet.h"
#include <memory>
#include <string>
#include <vector>

namespace indexstore {
  class IndexStore;
  typedef std::shared_ptr<IndexStore> IndexStoreRef;
}

namespace IndexStoreDB {
  class CanonicalPathCache;

namespace index {
  class IndexSystemDelegate;
  class SymbolIndex;
  typedef std::shared_ptr<SymbolIndex> SymbolIndexRef;

class IndexDatastore {
public:
  ~IndexDatastore();

  static std::unique_ptr<IndexDatastore> create(indexstore::IndexStoreRef idxStore,
                                                SymbolIndexRef SymIndex,
                                                std::shared_ptr<IndexSystemDelegate> Delegate,
                                                std::shared_ptr<CanonicalPathCache> CanonPathCache,
                                                bool readonly,
                                                std::string &Error);

  void waitUntilDoneInitializing();

  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimeValue outOfDateModTime);

  /// Check whether any unit(s) containing \p file are out of date and if so,
  /// *synchronously* notify the delegate.
  void checkUnitContainingFileIsOutOfDate(StringRef file);

  void purgeStaleData();

private:
  IndexDatastore(void *Impl) : Impl(Impl) {}

  void *Impl; // An IndexDatastoreImpl.
};

} // namespace index
} // namespace IndexStoreDB

#endif
