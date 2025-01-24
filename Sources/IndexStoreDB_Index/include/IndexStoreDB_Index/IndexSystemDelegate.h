//===--- IndexSystemDelegate.h ----------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_INDEX_INDEXSYSTEMDELEGATE_H
#define INDEXSTOREDB_INDEX_INDEXSYSTEMDELEGATE_H

#include <IndexStoreDB_Index/StoreUnitInfo.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Chrono.h>
#include <memory>
#include <string>

namespace IndexStoreDB {
namespace index {
struct StoreUnitInfo;
class OutOfDateFileTrigger;

typedef std::shared_ptr<OutOfDateFileTrigger> OutOfDateFileTriggerRef;

/// Records a known out-of-date file path for a unit, along with its
/// modification time. This is used to provide IndexDelegate with information
/// about the file that triggered the unit to become out-of-date.
class OutOfDateFileTrigger final {
  std::string FilePath;
  llvm::sys::TimePoint<> ModTime;

public:
  explicit OutOfDateFileTrigger(StringRef filePath,
                                llvm::sys::TimePoint<> modTime)
      : FilePath(filePath), ModTime(modTime) {}

  static OutOfDateFileTriggerRef create(StringRef filePath,
                                        llvm::sys::TimePoint<> modTime) {
    return std::make_shared<OutOfDateFileTrigger>(filePath, modTime);
  }

  llvm::sys::TimePoint<> getModTime() const { return ModTime; }

  /// Returns a reference to the stored file path. Note this has the same
  /// lifetime as the trigger.
  StringRef getPathRef() const { return FilePath; }

  std::string getPath() const { return FilePath; }
  std::string description() { return FilePath; }
};

class INDEXSTOREDB_EXPORT IndexSystemDelegate {
public:
  virtual ~IndexSystemDelegate() {}

  /// Called when the datastore gets initialized and receives the number of available units.
  virtual void initialPendingUnits(unsigned numUnits) {}

  virtual void processingAddedPending(unsigned NumActions) {}
  virtual void processingCompleted(unsigned NumActions) {}

  virtual void processedStoreUnit(StoreUnitInfo unitInfo) {}

  virtual void unitIsOutOfDate(StoreUnitInfo unitInfo,
                               OutOfDateFileTriggerRef trigger,
                               bool synchronous = false) {}

private:
  virtual void anchor();
};

} // namespace index
} // namespace IndexStoreDB

#endif
