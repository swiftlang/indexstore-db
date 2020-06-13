/*===--- CIndexStoreDB.h ------------------------------------------*- C -*-===//
 *
 * This source file is part of the Swift.org open source project
 *
 * Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
 * Licensed under Apache License v2.0 with Runtime Library Exception
 *
 * See https://swift.org/LICENSE.txt for license information
 * See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
 *
 *===----------------------------------------------------------------------===*/

#ifndef INDEXSTOREDB_INTERNAL_H
#define INDEXSTOREDB_INTERNAL_H

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <utility>

namespace IndexStoreDB {
namespace internal {

class ObjectBase : public llvm::ThreadSafeRefCountedBase<ObjectBase> {
public:
    virtual ~ObjectBase();
};

template <typename T>
class Object: public ObjectBase {
public:
    T value;

    Object(T value) : value(std::move(value)) {}
};

template <typename T>
static inline Object<T> *make_object(const T &value) {
  auto obj = new Object<T>(value);
  obj->Retain();
  return obj;
}

} // namespace internal
} // namespace IndexStoreDB

#endif // INDEXSTOREDB_INTERNAL_H
