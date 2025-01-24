//===--- IndexStoreLibraryProvider.h ----------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_INDEX_SYMBOLDATAPROVIDER_H
#define INDEXSTOREDB_INDEX_SYMBOLDATAPROVIDER_H

#include <IndexStoreDB_Support/LLVM.h>
#include <IndexStoreDB_Support/Visibility.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <memory>

namespace indexstore {
  class IndexStoreLibrary;
  typedef std::shared_ptr<IndexStoreLibrary> IndexStoreLibraryRef;
}

namespace IndexStoreDB {

namespace index {

using IndexStoreLibrary = ::indexstore::IndexStoreLibrary;
using IndexStoreLibraryRef = ::indexstore::IndexStoreLibraryRef;

class INDEXSTOREDB_EXPORT IndexStoreLibraryProvider {
public:
  virtual ~IndexStoreLibraryProvider() {}

  /// Returns an indexstore compatible with the data format in given store path.
  virtual IndexStoreLibraryRef getLibraryForStorePath(StringRef storePath) = 0;

private:
  virtual void anchor();
};

/// A simple library provider that can be used if libIndexStore is linked to your binary.
class INDEXSTOREDB_EXPORT GlobalIndexStoreLibraryProvider: public IndexStoreLibraryProvider {
public:
  IndexStoreLibraryRef getLibraryForStorePath(StringRef storePath) override;
};

INDEXSTOREDB_EXPORT IndexStoreLibraryRef loadIndexStoreLibrary(std::string dylibPath,
                                                       std::string &error);

} // namespace index
} // namespace IndexStoreDB

#endif
