//===--- FilePathIndex.cpp ------------------------------------------------===//
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

#include "IndexStoreDB/Index/FilePathIndex.h"
#include "IndexStoreDB/Index/IndexSystem.h"
#include "IndexStoreDB/Index/StoreUnitInfo.h"
#include "IndexStoreDB/Database/Database.h"
#include "IndexStoreDB/Database/ImportTransaction.h"
#include "IndexStoreDB/Database/ReadTransaction.h"
#include "IndexStoreDB/Support/Logging.h"
#include "IndexStoreDB/Support/Path.h"
#include "FileVisibilityChecker.h"

#include "indexstore/IndexStoreCXX.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>
#include <set>

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;
using namespace IndexStoreDB::index;
using namespace indexstore;
using namespace llvm;

namespace {

class FileIndexImpl {
  DatabaseRef DBase;
  IndexStoreRef IdxStore;
  std::shared_ptr<FileVisibilityChecker> VisibilityChecker;
  std::shared_ptr<CanonicalPathCache> CanonPathCache;

public:
  FileIndexImpl(DatabaseRef dbase, IndexStoreRef idxStore,
                std::shared_ptr<FileVisibilityChecker> visibilityChecker,
                std::shared_ptr<CanonicalPathCache> canonPathCache)
    : DBase(std::move(dbase)), IdxStore(std::move(idxStore)),
      VisibilityChecker(visibilityChecker),
      CanonPathCache(std::move(canonPathCache)) {}

  CanonicalFilePath getCanonicalPath(StringRef Path, StringRef WorkingDir);

  bool isKnownFile(CanonicalFilePathRef filePath);

  bool foreachMainUnitContainingFile(CanonicalFilePathRef filePath,
                                 function_ref<bool(const StoreUnitInfo &unitInfo)> receiver);

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
                                function_ref<bool(CanonicalFilePathRef sourcePath, unsigned line)> Receiver);

  bool foreachFileIncludedByFile(CanonicalFilePathRef sourcePath,
                                 function_ref<bool(CanonicalFilePathRef targetPath, unsigned line)> Receiver);

  bool foreachIncludeOfUnit(StringRef unitName,
                            function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver);

  bool foreachIncludeOfStoreUnitContainingFile(CanonicalFilePathRef filePath,
               function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver);
};

} // anonymous namespace

//===----------------------------------------------------------------------===//
// FileIndexImpl
//===----------------------------------------------------------------------===//

static void collectFileDependencies(DatabaseRef dbase,
                                    IDCode unitCode,
                                    bool followDependencies,
                                    std::set<std::string> &pathsSet,
                                    std::unordered_set<IDCode> &visitedUnits) {
  if (visitedUnits.count(unitCode))
    return;
  visitedUnits.insert(unitCode);

  std::vector<IDCode> UnitDepends;
  {
    ReadTransaction reader(dbase);
    auto dbUnit = reader.getUnitInfo(unitCode);
    if (dbUnit.isInvalid())
      return; // Does not exist.

    auto addPath = [&](IDCode pathCode) {
      std::string pathStr;
      raw_string_ostream OS(pathStr);
      if (reader.getFullFilePathFromCode(pathCode, OS)) {
        pathsSet.insert(std::move(OS.str()));
      }
    };

    for (auto pathCode : dbUnit.FileDepends) {
      addPath(pathCode);
    }
    for (auto &prov : dbUnit.ProviderDepends) {
      addPath(prov.FileCode);
    }

    if (followDependencies)
      UnitDepends.insert(UnitDepends.end(), dbUnit.UnitDepends.begin(), dbUnit.UnitDepends.end());
  }

  for (auto unitDepCode : UnitDepends) {
    collectFileDependencies(dbase, unitDepCode, followDependencies, pathsSet, visitedUnits);
  }
}

CanonicalFilePath FileIndexImpl::getCanonicalPath(StringRef Path, StringRef WorkingDir) {
  return CanonPathCache->getCanonicalPath(Path, WorkingDir);
}

bool FileIndexImpl::isKnownFile(CanonicalFilePathRef filePath) {
  bool foundUnit = false;
  {
    ReadTransaction reader(DBase);
    IDCode pathCode = reader.getFilePathCode(filePath);
    reader.foreachUnitContainingFile(pathCode, [&](ArrayRef<IDCode> unitCodes) -> bool {
      for (IDCode unitCode: unitCodes) {
        auto unitInfo = reader.getUnitInfo(unitCode);
        if (unitInfo.isValid() && VisibilityChecker->isUnitVisible(unitInfo, reader)) {
          foundUnit = true;
          return false;
        }
      }
      return true;
    });
  }

  return foundUnit;
}

bool FileIndexImpl::foreachMainUnitContainingFile(CanonicalFilePathRef filePath,
                                              function_ref<bool(const StoreUnitInfo &unitInfo)> receiver) {
  std::vector<StoreUnitInfo> unitInfos;
  {
    ReadTransaction reader(DBase);
    IDCode pathCode = reader.getFilePathCode(filePath);
    reader.foreachRootUnitOfFile(pathCode, [&](const UnitInfo &unitInfo) -> bool {
      unitInfos.resize(unitInfos.size()+1);
      StoreUnitInfo &currUnit = unitInfos.back();
      currUnit.UnitName = unitInfo.UnitName;
      currUnit.ModTime = unitInfo.ModTime;
      currUnit.MainFilePath = reader.getFullFilePathFromCode(unitInfo.MainFileCode);
      currUnit.OutFileIdentifier = reader.getUnitFileIdentifierFromCode(unitInfo.OutFileCode);
      currUnit.SymProviderKind = unitInfo.SymProviderKind;
      return true;
    });
  }

  for (auto &unit : unitInfos) {
    if (!receiver(unit))
      return false;
  }
  return true;
}

bool FileIndexImpl::foreachFileOfUnit(StringRef unitName,
                                      bool followDependencies,
                                      function_ref<bool(CanonicalFilePathRef filePath)> receiver) {
  if (unitName.empty())
    return true;

  std::set<std::string> pathsSet;
  std::unordered_set<IDCode> visitedUnits;
  collectFileDependencies(DBase, makeIDCodeFromString(unitName), followDependencies, pathsSet, visitedUnits);

  for (auto &path : pathsSet) {
    if (!receiver(CanonicalFilePathRef::getAsCanonicalPath(path)))
      return false;
  }
  return true;
}

bool FileIndexImpl::foreachFilenameContainingPattern(StringRef Pattern,
                                                     bool AnchorStart,
                                                     bool AnchorEnd,
                                                     bool Subsequence,
                                                     bool IgnoreCase,
                              function_ref<bool(CanonicalFilePathRef FilePath)> Receiver) {
  ReadTransaction reader(DBase);
  return reader.findFilenamesContaining(Pattern, AnchorStart, AnchorEnd, Subsequence, IgnoreCase, [&](CanonicalFilePathRef filePath) -> bool {
    return Receiver(filePath);
  });
}

bool FileIndexImpl::foreachFileIncludingFile(CanonicalFilePathRef inputTargetPath,
                              function_ref<bool(CanonicalFilePathRef SourcePath, unsigned Line)> Receiver) {
  StringSet<> pathsSeen;
  return foreachIncludeOfStoreUnitContainingFile(inputTargetPath, [&](CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line) -> bool {
    if (targetPath != inputTargetPath || !pathsSeen.insert(sourcePath.getPath()).second)
      return true;
    return Receiver(sourcePath, line);
  });
};

bool FileIndexImpl::foreachFileIncludedByFile(CanonicalFilePathRef inputSourcePath,
                               function_ref<bool(CanonicalFilePathRef TargetPath, unsigned Line)> Receiver) {
  StringSet<> pathsSeen;
  return foreachIncludeOfStoreUnitContainingFile(inputSourcePath, [&](CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line) -> bool {
    if (sourcePath != inputSourcePath || !pathsSeen.insert(targetPath.getPath()).second)
      return true;
    return Receiver(targetPath, line);
  });
}

bool FileIndexImpl::foreachIncludeOfUnit(StringRef unitName,
        function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver) {
  std::string error;
  IndexUnitReader storeUnit(*IdxStore, unitName, error);
  if (!storeUnit.isValid())
    return true;
  StringRef workDir = storeUnit.getWorkingDirectory();
  return storeUnit.foreachInclude([&](IndexUnitInclude inc) -> bool {
    StringRef sourcePath = inc.getSourcePath();
    StringRef targetPath = inc.getTargetPath();
    unsigned line = inc.getSourceLine();
    CanonicalFilePath fullSourcePath = getCanonicalPath(sourcePath, workDir);
    CanonicalFilePath fullTargetPath = getCanonicalPath(targetPath, workDir);
    return receiver(fullSourcePath, fullTargetPath, line);
  });
}

bool FileIndexImpl::foreachIncludeOfStoreUnitContainingFile(CanonicalFilePathRef filePath,
                   function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver) {
  SmallVector<std::string, 32> allUnitNames;
  {
    ReadTransaction Reader(DBase);
    IDCode pathCode = Reader.getFilePathCode(filePath);

    Reader.foreachUnitContainingFile(pathCode, [&](ArrayRef<IDCode> UnitCodes) -> bool {
      for (IDCode unitCode: UnitCodes) {
        auto unitInfo = Reader.getUnitInfo(unitCode);
        if (unitInfo.isValid() && VisibilityChecker->isUnitVisible(unitInfo, Reader))
          allUnitNames.push_back(unitInfo.UnitName);
      }
      return true;
    });
  }

  for (auto &unitName: allUnitNames) {
    bool cont = foreachIncludeOfUnit(unitName, receiver);
    if (!cont)
      return false;
  }

  return true;
}

//===----------------------------------------------------------------------===//
// FilePathIndex
//===----------------------------------------------------------------------===//

FilePathIndex::FilePathIndex(db::DatabaseRef dbase, IndexStoreRef idxStore,
                             std::shared_ptr<FileVisibilityChecker> visibilityChecker,
                             std::shared_ptr<CanonicalPathCache> canonPathCache) {
  Impl = new FileIndexImpl(std::move(dbase), std::move(idxStore),
                           std::move(visibilityChecker), std::move(canonPathCache));
}

#define IMPL static_cast<FileIndexImpl*>(Impl)

FilePathIndex::~FilePathIndex() {
  delete IMPL;
}

CanonicalFilePath FilePathIndex::getCanonicalPath(StringRef Path, StringRef WorkingDir) {
  return IMPL->getCanonicalPath(Path, WorkingDir);
}

bool FilePathIndex::isKnownFile(CanonicalFilePathRef filePath) {
  return IMPL->isKnownFile(filePath);
}

bool FilePathIndex::foreachMainUnitContainingFile(CanonicalFilePathRef filePath,
                                              function_ref<bool(const StoreUnitInfo &unitInfo)> receiver) {
  return IMPL->foreachMainUnitContainingFile(filePath, std::move(receiver));
}

bool FilePathIndex::foreachFileOfUnit(StringRef unitName,
                                      bool followDependencies,
                                      function_ref<bool(CanonicalFilePathRef filePath)> receiver) {
  return IMPL->foreachFileOfUnit(unitName, followDependencies, std::move(receiver));
}

bool FilePathIndex::foreachFilenameContainingPattern(StringRef Pattern,
                                                bool AnchorStart,
                                                bool AnchorEnd,
                                                bool Subsequence,
                                                bool IgnoreCase,
                              function_ref<bool(CanonicalFilePathRef FilePath)> Receiver) {
  return IMPL->foreachFilenameContainingPattern(Pattern, AnchorStart, AnchorEnd,
                                  Subsequence, IgnoreCase, std::move(Receiver));
}

bool FilePathIndex::foreachFileIncludingFile(CanonicalFilePathRef TargetPath, function_ref<bool (CanonicalFilePathRef, unsigned int)> Receiver) {
  return IMPL->foreachFileIncludingFile(TargetPath, Receiver);
}

bool FilePathIndex::foreachFileIncludedByFile(CanonicalFilePathRef SourcePath, function_ref<bool (CanonicalFilePathRef, unsigned int)> Receiver) {
  return IMPL->foreachFileIncludedByFile(SourcePath, Receiver);
}

bool FilePathIndex::foreachIncludeOfUnit(StringRef unitName, function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver) {
  return IMPL->foreachIncludeOfUnit(unitName, receiver);
}
