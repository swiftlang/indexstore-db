//===--- Logging_impl.h -----------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_SUPPORT_LOGGING_IMPL_H
#define INDEXSTOREDB_SUPPORT_LOGGING_IMPL_H

namespace IndexStoreDB {
void Log_impl(const char *loggerName, const char *message);
}

#endif
