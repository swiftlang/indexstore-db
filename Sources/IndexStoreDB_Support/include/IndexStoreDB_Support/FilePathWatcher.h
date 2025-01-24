//===--- FilePathWatcher.h --------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SUPPORT_FILEPATHWATCHER_H
#define INDEXSTOREDB_SUPPORT_FILEPATHWATCHER_H

#include <IndexStoreDB_Support/LLVM.h>
#include <IndexStoreDB_Support/Visibility.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_ArrayRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <functional>

namespace IndexStoreDB {

class INDEXSTOREDB_EXPORT FilePathWatcher {
  struct Implementation;

public:
  typedef std::function<void(std::vector<std::string>)> FileEventsReceiverTy;
  explicit FilePathWatcher(FileEventsReceiverTy pathsReceiver);
  ~FilePathWatcher();

private:
  Implementation &Impl;
};

} // namespace IndexStoreDB

#endif
