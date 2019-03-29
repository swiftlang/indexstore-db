//===--- PatternMatching.h --------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SUPPORT_PATTERNMATCHING_H
#define INDEXSTOREDB_SUPPORT_PATTERNMATCHING_H

#include "IndexStoreDB/Support/LLVM.h"
#include "IndexStoreDB/Support/Visibility.h"

namespace IndexStoreDB {

INDEXSTOREDB_EXPORT
bool matchesPattern(StringRef Input,
                    StringRef Pattern,
                    bool AnchorStart,
                    bool AnchorEnd,
                    bool Subsequence,
                    bool IgnoreCase);

} // namespace IndexStoreDB

#endif
