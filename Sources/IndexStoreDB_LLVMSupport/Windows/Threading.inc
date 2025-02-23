//===- Windows/Threading.inc - Win32 Threading Implementation - -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides the Win32 specific implementation of Threading functions.
//
//===----------------------------------------------------------------------===//

#include <IndexStoreDB_LLVMSupport/llvm_ADT_SmallString.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_Twine.h>

#include "WindowsSupport.h"
#include <process.h>

// Windows will at times define MemoryFence.
#ifdef MemoryFence
#undef MemoryFence
#endif

namespace {
  struct ThreadInfo {
    void(*func)(void*);
    void *param;
  };
}

static unsigned __stdcall ThreadCallback(void *param) {
  struct ThreadInfo *info = reinterpret_cast<struct ThreadInfo *>(param);
  info->func(info->param);

  return 0;
}

void llvm::llvm_execute_on_thread(void(*Fn)(void*), void *UserData,
  unsigned RequestedStackSize) {
  struct ThreadInfo param = { Fn, UserData };

  HANDLE hThread = (HANDLE)::_beginthreadex(NULL,
    RequestedStackSize, ThreadCallback,
    &param, 0, NULL);

  if (hThread) {
    // We actually don't care whether the wait succeeds or fails, in
    // the same way we don't care whether the pthread_join call succeeds
    // or fails.  There's not much we could do if this were to fail. But
    // on success, this call will wait until the thread finishes executing
    // before returning.
    (void)::WaitForSingleObject(hThread, INFINITE);
    ::CloseHandle(hThread);
  }
}

uint64_t llvm::get_threadid() {
  return uint64_t(::GetCurrentThreadId());
}

uint32_t llvm::get_max_thread_name_length() { return 0; }

#if defined(_MSC_VER)
static void SetThreadName(DWORD Id, LPCSTR Name) {
  constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
  struct THREADNAME_INFO {
    DWORD dwType;     // Must be 0x1000.
    LPCSTR szName;    // Pointer to thread name
    DWORD dwThreadId; // Thread ID (-1 == current thread)
    DWORD dwFlags;    // Reserved.  Do not use.
  };
#pragma pack(pop)

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = Name;
  info.dwThreadId = Id;
  info.dwFlags = 0;

  __try {
    ::RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
      (ULONG_PTR *)&info);
  }
  __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}
#endif

void llvm::set_thread_name(const Twine &Name) {
#if defined(_MSC_VER)
  // Make sure the input is null terminated.
  SmallString<64> Storage;
  StringRef NameStr = Name.toNullTerminatedStringRef(Storage);
  SetThreadName(::GetCurrentThreadId(), NameStr.data());
#endif
}

void llvm::get_thread_name(SmallVectorImpl<char> &Name) {
  // "Name" is not an inherent property of a thread on Windows.  In fact, when
  // you "set" the name, you are only firing a one-time message to a debugger
  // which it interprets as a program setting its threads' name.  We may be
  // able to get fancy by creating a TLS entry when someone calls
  // set_thread_name so that subsequent calls to get_thread_name return this
  // value.
  Name.clear();
}

SetThreadPriorityResult llvm::set_thread_priority(ThreadPriority Priority) {
  // https://docs.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
  // Begin background processing mode. The system lowers the resource scheduling
  // priorities of the thread so that it can perform background work without
  // significantly affecting activity in the foreground.
  // End background processing mode. The system restores the resource scheduling
  // priorities of the thread as they were before the thread entered background
  // processing mode.
  return SetThreadPriority(GetCurrentThread(),
                           Priority == ThreadPriority::Background
                               ? THREAD_MODE_BACKGROUND_BEGIN
                               : THREAD_MODE_BACKGROUND_END)
             ? SetThreadPriorityResult::SUCCESS
             : SetThreadPriorityResult::FAILURE;
}
