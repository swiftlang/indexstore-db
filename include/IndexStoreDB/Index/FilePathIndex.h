//===--- FilePathIndex.h ----------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_INDEX_FILEPATHINDEX_H
#define INDEXSTOREDB_INDEX_FILEPATHINDEX_H

#include "IndexStoreDB/Support/LLVM.h"
#include "llvm/ADT/StringRef.h"
#include <memory>

namespace indexstore {
  class IndexStore;
  typedef std::shared_ptr<IndexStore> IndexStoreRef;
}

namespace IndexStoreDB {
  class CanonicalFilePath;
  class CanonicalFilePathRef;
  class CanonicalPathCache;

namespace db {
  class Database;
  typedef std::shared_ptr<Database> DatabaseRef;
}

namespace index {
  class FileVisibilityChecker;
  struct StoreUnitInfo;

class FilePathIndex {
public:
  FilePathIndex(db::DatabaseRef dbase, indexstore::IndexStoreRef idxStore,
                std::shared_ptr<FileVisibilityChecker> visibilityChecker,
                std::shared_ptr<CanonicalPathCache> canonPathCache);
  ~FilePathIndex();

  CanonicalFilePath getCanonicalPath(StringRef Path,
                                     StringRef WorkingDir = StringRef());

  //===--------------------------------------------------------------------===//
  // Queries
  //===--------------------------------------------------------------------===//

  bool foreachMainUnitContainingFile(CanonicalFilePathRef filePath,
                                 function_ref<bool(const StoreUnitInfo &unitInfo)> Receiver);

  bool isKnownFile(CanonicalFilePathRef filePath);

  bool foreachFileOfUnit(StringRef unitName,
                         bool followDependencies,
                         function_ref<bool(CanonicalFilePathRef filePath)> receiver);

  bool foreachFilenameContainingPattern(StringRef Pattern,
                                        bool AnchorStart,
                                        bool AnchorEnd,
                                        bool Subsequence,
                                        bool IgnoreCase,
                               function_ref<bool(CanonicalFilePathRef FilePath)> Receiver);

  bool foreachFileIncludingFile(CanonicalFilePathRef targetPath,
                            function_ref<bool(CanonicalFilePathRef SourcePath, unsigned Line)> Receiver);

  bool foreachFileIncludedByFile(CanonicalFilePathRef sourcePath,
                            function_ref<bool(CanonicalFilePathRef TargetPath, unsigned Line)> Receiver);

  bool foreachIncludeOfUnit(StringRef unitName,
                            function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver);

private:
  void *Impl; // A FileIndexImpl.
};

typedef std::shared_ptr<FilePathIndex> FilePathIndexRef;

} // namespace index
} // namespace IndexStoreDB

#endif
