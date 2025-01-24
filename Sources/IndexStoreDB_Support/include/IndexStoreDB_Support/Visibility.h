//===--- Visibility.h ------------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_INDEXSTOREDB_SUPPORT_VISIBILITY_H
#define LLVM_INDEXSTOREDB_SUPPORT_VISIBILITY_H

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(visibility)
#define INDEXSTOREDB_EXPORT __attribute__ ((visibility("default")))
#else
#define INDEXSTOREDB_EXPORT
#endif

#endif
