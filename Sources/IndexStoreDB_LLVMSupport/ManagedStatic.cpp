//===-- ManagedStatic.cpp - Static Global wrapper -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the ManagedStatic class and llvm_shutdown().
//
//===----------------------------------------------------------------------===//

#include <IndexStoreDB_LLVMSupport/llvm_Support_ManagedStatic.h>
#include <IndexStoreDB_LLVMSupport/llvm_Config_config.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Mutex.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_MutexGuard.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Threading.h>
#include <cassert>
using namespace llvm;

static const ManagedStaticBase *StaticList = nullptr;
static sys::Mutex *ManagedStaticMutex = nullptr;
static llvm::once_flag mutex_init_flag;

static void initializeMutex() {
  ManagedStaticMutex = new sys::Mutex();
}

static sys::Mutex* getManagedStaticMutex() {
  llvm::call_once(mutex_init_flag, initializeMutex);
  return ManagedStaticMutex;
}

void ManagedStaticBase::RegisterManagedStatic(void *(*Creator)(),
                                              void (*Deleter)(void*)) const {
  assert(Creator);
  if (llvm_is_multithreaded()) {
    MutexGuard Lock(*getManagedStaticMutex());

    if (!Ptr.load(std::memory_order_relaxed)) {
      void *Tmp = Creator();

      Ptr.store(Tmp, std::memory_order_release);
      DeleterFn = Deleter;

      // Add to list of managed statics.
      Next = StaticList;
      StaticList = this;
    }
  } else {
    assert(!Ptr && !DeleterFn && !Next &&
           "Partially initialized ManagedStatic!?");
    Ptr = Creator();
    DeleterFn = Deleter;

    // Add to list of managed statics.
    Next = StaticList;
    StaticList = this;
  }
}

void ManagedStaticBase::destroy() const {
  assert(DeleterFn && "ManagedStatic not initialized correctly!");
  assert(StaticList == this &&
         "Not destroyed in reverse order of construction?");
  // Unlink from list.
  StaticList = Next;
  Next = nullptr;

  // Destroy memory.
  DeleterFn(Ptr);

  // Cleanup.
  Ptr = nullptr;
  DeleterFn = nullptr;
}

/// llvm_shutdown - Deallocate and destroy all ManagedStatic variables.
void llvm::llvm_shutdown() {
  MutexGuard Lock(*getManagedStaticMutex());

  while (StaticList)
    StaticList->destroy();
}
