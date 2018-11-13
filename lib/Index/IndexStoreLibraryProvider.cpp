//===--- IndexStoreLibraryProvider.cpp ------------------------------------===//
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

#include "IndexStoreDB/Index/IndexStoreLibraryProvider.h"
#include "indexstore/IndexStoreCXX.h"
#include "llvm/ADT/StringRef.h"
#include <dlfcn.h>

using namespace IndexStoreDB;
using namespace index;

// Forward-declare the indexstore symbols

void IndexStoreLibraryProvider::anchor() {}

static IndexStoreLibraryRef loadIndexStoreLibraryFromDLHandle(void *dlHandle, std::string &error);

IndexStoreLibraryRef GlobalIndexStoreLibraryProvider::getLibraryForStorePath(StringRef storePath) {

  // Note: we're using dlsym with RTLD_DEFAULT because we cannot #incldue indexstore.h and indexstore_functions.h
  std::string ignored;
  return loadIndexStoreLibraryFromDLHandle(RTLD_DEFAULT, ignored);
}

IndexStoreLibraryRef index::loadIndexStoreLibrary(std::string dylibPath,
                                                  std::string &error) {
  auto flags = RTLD_LAZY | RTLD_LOCAL;
#ifdef RTLD_FIRST
  flags |= RTLD_FIRST;
#endif

  void *dlHandle = dlopen(dylibPath.c_str(), flags);
  if (!dlHandle) {
    error = "failed to dlopen indexstore library: ";
    error += dlerror();
    return nullptr;
  }

  // Intentionally leak the dlhandle; we have no reason to dlclose it and it may be unsafe.
  (void)dlHandle;

  return loadIndexStoreLibraryFromDLHandle(dlHandle, error);
}

static IndexStoreLibraryRef loadIndexStoreLibraryFromDLHandle(void *dlHandle, std::string &error) {
  indexstore_functions_t api;

#define INDEXSTORE_FUNCTION(func, required) \
  api.func = (decltype(indexstore_functions_t::func))dlsym(dlHandle, "indexstore_" #func); \
  if (!api.func && required) { \
    error = "indexstore library missing required function indexstore_" #func; \
    return nullptr; \
  }

#include "indexstore_functions.def"

  return std::make_shared<IndexStoreLibrary>(api);
}
