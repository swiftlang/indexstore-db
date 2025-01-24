//===--- Logging-NonMac.cpp -----------------------------------------------===//
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

#if !defined(__APPLE__)

#include "Logging_impl.h"
#include <cstdio>

void IndexStoreDB::Log_impl(const char *loggerName, const char *message) {
  // FIXME: this can interleave output.
  fprintf(stderr, "%s: %s\n", loggerName, message);
}

#endif
