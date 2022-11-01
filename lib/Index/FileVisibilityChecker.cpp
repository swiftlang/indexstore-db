//===--- FileVisibilityChecker.cpp ----------------------------------------===//
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

#include "FileVisibilityChecker.h"
#include "IndexStoreDB/Database/ReadTransaction.h"
#include "IndexStoreDB/Support/Path.h"

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;
using namespace IndexStoreDB::index;
using namespace llvm;

FileVisibilityChecker::FileVisibilityChecker(DatabaseRef dbase,
                                             std::shared_ptr<CanonicalPathCache> canonPathCache,
                                             bool useExplicitOutputUnits)
    : DBase(std::move(dbase)), CanonPathCache(std::move(canonPathCache)), UseExplicitOutputUnits(useExplicitOutputUnits) {}

void FileVisibilityChecker::registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName) {
  sys::ScopedLock L(VisibleCacheMtx);

  ReadTransaction reader(DBase);
  for (StringRef filePath : filePaths) {
    CanonicalFilePath canonPath = CanonPathCache->getCanonicalPath(filePath);
    if (canonPath.empty())
      continue;
    IDCode pathCode = reader.getFilePathCode(canonPath);
    ++MainFilesRefCount[pathCode];
    VisibleMainFiles.insert(pathCode);
  }
  UnitVisibilityCache.clear();
}

void FileVisibilityChecker::unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName) {
  sys::ScopedLock L(VisibleCacheMtx);

  ReadTransaction reader(DBase);
  for (StringRef filePath : filePaths) {
    CanonicalFilePath canonPath = CanonPathCache->getCanonicalPath(filePath);
    if (canonPath.empty())
      continue;
    IDCode pathCode = reader.getFilePathCode(canonPath);
    auto It = MainFilesRefCount.find(pathCode);
    if (It == MainFilesRefCount.end())
      continue;
    if (It->second <= 1) {
      MainFilesRefCount.erase(It);
      VisibleMainFiles.erase(pathCode);
    } else {
      --It->second;
    }
  }
  UnitVisibilityCache.clear();
}

void FileVisibilityChecker::addUnitOutFilePaths(ArrayRef<StringRef> filePaths) {
  sys::ScopedLock L(VisibleCacheMtx);

  ReadTransaction reader(DBase);
  for (StringRef filePath : filePaths) {
    IDCode pathCode = reader.getUnitFileIdentifierCode(filePath);
    OutUnitFiles.insert(pathCode);
  }
  UnitVisibilityCache.clear();
}

void FileVisibilityChecker::removeUnitOutFilePaths(ArrayRef<StringRef> filePaths) {
  sys::ScopedLock L(VisibleCacheMtx);

  ReadTransaction reader(DBase);
  for (StringRef filePath : filePaths) {
    IDCode pathCode = reader.getUnitFileIdentifierCode(filePath);
    OutUnitFiles.erase(pathCode);
  }
  UnitVisibilityCache.clear();
}

bool FileVisibilityChecker::isUnitVisible(const db::UnitInfo &unitInfo, db::ReadTransaction &reader) {
  if (unitInfo.isInvalid())
    return false;

  sys::ScopedLock L(VisibleCacheMtx);

  auto visibleCheck = [&](const db::UnitInfo &unitInfo) -> bool {
    if (UseExplicitOutputUnits) {
      return OutUnitFiles.count(unitInfo.OutFileCode);
    } else {
      return VisibleMainFiles.count(unitInfo.MainFileCode);
    }
  };

  if (!UseExplicitOutputUnits && VisibleMainFiles.empty())
    return true; // If not using main file 'visibility' feature, then assume all files visible.

  if (unitInfo.HasMainFile) {
    return visibleCheck(unitInfo);
  }

  auto pair = UnitVisibilityCache.insert(std::make_pair(unitInfo.UnitCode, false));
  bool &isVisible = pair.first->second;
  bool isNew = pair.second;
  if (!isNew) {
    return isVisible;
  }

  reader.foreachRootUnitOfUnit(unitInfo.UnitCode, [&](const UnitInfo &unitInfo) -> bool {
    if (visibleCheck(unitInfo)) {
      isVisible = true;
      return false;
    }
    return true;
  });
  return isVisible;
}
