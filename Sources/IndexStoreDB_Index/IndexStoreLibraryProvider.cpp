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

#include <IndexStoreDB_Index/IndexStoreLibraryProvider.h>
#include <IndexStoreDB_Index/IndexStoreCXX.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_ConvertUTF.h>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

using namespace IndexStoreDB;
using namespace index;

// Forward-declare the indexstore symbols

void IndexStoreLibraryProvider::anchor() {}

static IndexStoreLibraryRef loadIndexStoreLibraryFromDLHandle(void *dlHandle, std::string &error);

IndexStoreLibraryRef GlobalIndexStoreLibraryProvider::getLibraryForStorePath(StringRef storePath) {

  // Note: we're using dlsym with RTLD_DEFAULT because we cannot #incldue indexstore.h and indexstore_functions.h
  std::string ignored;
#if defined(_WIN32)
  void* defaultHandle = GetModuleHandleW(NULL);
#else
  void* defaultHandle = RTLD_DEFAULT;
#endif
  return loadIndexStoreLibraryFromDLHandle(defaultHandle, ignored);
}

IndexStoreLibraryRef index::loadIndexStoreLibrary(std::string dylibPath,
                                                  std::string &error) {
#if defined(_WIN32)
  llvm::SmallVector<llvm::UTF16, 30> u16Path;
  if (!convertUTF8ToUTF16String(dylibPath, u16Path)) {
    error += "Failed to convert path: " + dylibPath + " to UTF-16";
    return nullptr;
  }
  HMODULE dlHandle = LoadLibraryW((LPCWSTR)u16Path.data());
  if (dlHandle == NULL) {
    error += "Failed to load " + dylibPath + ". Error: " + std::to_string(GetLastError());
    return nullptr;
  }
#else
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
#endif

  // Intentionally leak the dlhandle; we have no reason to dlclose it and it may be unsafe.
  (void)dlHandle;

  return loadIndexStoreLibraryFromDLHandle(dlHandle, error);
}

static IndexStoreLibraryRef loadIndexStoreLibraryFromDLHandle(void *dlHandle, std::string &error) {
  indexstore_functions_t api;

#if defined(_WIN32)
#define INDEXSTORE_FUNCTION(func, required) \
  api.func = (decltype(indexstore_functions_t::func))GetProcAddress((HMODULE)dlHandle, "indexstore_" #func); \
  if (!api.func && required) { \
    error = "indexstore library missing required function indexstore_" #func; \
    return nullptr; \
  }
#else
#define INDEXSTORE_FUNCTION(func, required) \
  api.func = (decltype(indexstore_functions_t::func))dlsym(dlHandle, "indexstore_" #func); \
  if (!api.func && required) { \
    error = "indexstore library missing required function indexstore_" #func; \
    return nullptr; \
  }
#endif

#include "indexstore_functions.def"

  return std::make_shared<IndexStoreLibrary>(api);
}
