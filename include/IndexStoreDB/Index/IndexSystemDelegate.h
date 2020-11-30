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

#include "IndexStoreDB/Index/StoreUnitInfo.h"
#include "llvm/Support/Chrono.h"
#include <memory>
#include <string>

namespace IndexStoreDB {
namespace index {
  struct StoreUnitInfo;

/// Used to document why a unit was considered out-of-date.
/// Primarily used for logging/debugging purposes.
class OutOfDateTriggerHint {
public:
  virtual ~OutOfDateTriggerHint() {}
  virtual std::string originalFileTrigger() = 0;
  virtual std::string description() = 0;

private:
  virtual void _anchor();
};
typedef std::shared_ptr<OutOfDateTriggerHint> OutOfDateTriggerHintRef;

class DependentFileOutOfDateTriggerHint : public OutOfDateTriggerHint {
  std::string FilePath;

public:
  explicit DependentFileOutOfDateTriggerHint(StringRef filePath) : FilePath(filePath) {}

  static OutOfDateTriggerHintRef create(StringRef filePath) {
    return std::make_shared<DependentFileOutOfDateTriggerHint>(filePath);
  }

  virtual std::string originalFileTrigger() override;
  virtual std::string description() override;
};

class DependentUnitOutOfDateTriggerHint : public OutOfDateTriggerHint {
  std::string UnitName;
  OutOfDateTriggerHintRef DepHint;

public:
  DependentUnitOutOfDateTriggerHint(StringRef unitName, OutOfDateTriggerHintRef depHint)
  : UnitName(unitName), DepHint(std::move(depHint)) {}

  static OutOfDateTriggerHintRef create(StringRef unitName, OutOfDateTriggerHintRef depHint) {
    return std::make_shared<DependentUnitOutOfDateTriggerHint>(unitName, std::move(depHint));
  }

  virtual std::string originalFileTrigger() override;
  virtual std::string description() override;
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
                               llvm::sys::TimePoint<> outOfDateModTime,
                               OutOfDateTriggerHintRef hint,
                               bool synchronous = false) {}

private:
  virtual void anchor();
};

} // namespace index
} // namespace IndexStoreDB

#endif
