//===--- SymbolIndex.h ------------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_INDEX_SYMBOLINDEX_H
#define INDEXSTOREDB_INDEX_SYMBOLINDEX_H

#include "IndexStoreDB/Support/LLVM.h"
#include "llvm/ADT/OptionSet.h"
#include <memory>

namespace indexstore {
  class IndexStore;
  typedef std::shared_ptr<IndexStore> IndexStoreRef;
}

namespace IndexStoreDB {
  class SymbolOccurrence;
  enum class SymbolRole : uint64_t;
  enum class SymbolKind : uint8_t;
  typedef std::shared_ptr<SymbolOccurrence> SymbolOccurrenceRef;
  typedef llvm::OptionSet<SymbolRole> SymbolRoleSet;

namespace db {
  class Database;
  typedef std::shared_ptr<Database> DatabaseRef;
  class ImportTransaction;
}

namespace index {
  class FileVisibilityChecker;
  class SymbolDataProvider;
  typedef std::shared_ptr<SymbolDataProvider> SymbolDataProviderRef;

class SymbolIndex {
public:
  SymbolIndex(db::DatabaseRef dbase, indexstore::IndexStoreRef indexStore,
              std::shared_ptr<FileVisibilityChecker> visibilityChecker);
  ~SymbolIndex();

  db::DatabaseRef getDBase() const;

  void importSymbols(db::ImportTransaction &Import, SymbolDataProviderRef Provider);

  void printStats(raw_ostream &OS);

  void dumpProviderFileAssociations(raw_ostream &OS);

  //===--------------------------------------------------------------------===//
  // Queries
  //===--------------------------------------------------------------------===//

  bool foreachSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachRelatedSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                bool AnchorStart,
                                                bool AnchorEnd,
                                                bool Subsequence,
                                                bool IgnoreCase,
                              function_ref<bool(SymbolOccurrenceRef)> Receiver);

  bool foreachCanonicalSymbolOccurrenceByName(StringRef name,
                        function_ref<bool(SymbolOccurrenceRef Occur)> receiver);

  bool foreachSymbolName(function_ref<bool(StringRef name)> receiver);

  bool foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                        function_ref<bool(SymbolOccurrenceRef occur)> receiver);

  size_t countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly);
  bool foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                                              function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachUnitTestSymbolReferencedByOutputPaths(ArrayRef<CanonicalFilePath> FilePaths,
      function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

private:
  void *Impl; // A SymbolIndexImpl.
};

typedef std::shared_ptr<SymbolIndex> SymbolIndexRef;

} // namespace index
} // namespace IndexStoreDB

#endif
