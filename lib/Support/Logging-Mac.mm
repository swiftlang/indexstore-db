//===--- Logging-Mac.mm ---------------------------------------------------===//
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

#if defined(__APPLE__)

#include "Logging_impl.h"
#import <Foundation/Foundation.h>

void IndexStoreDB::Log_impl(const char *loggerName, const char *message) {
  // Using NSLog instead of stderr, to avoid interleaving with other log output in the process.
  // NSLog also logs to asl.
  NSLog(@"%s: %s", loggerName, message);
}

#endif
