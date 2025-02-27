//===- Errno.cpp - errno support --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the errno wrappers.
//
//===----------------------------------------------------------------------===//

#include <IndexStoreDB_LLVMSupport/llvm_Support_Errno.h>
#include <IndexStoreDB_LLVMSupport/llvm_Config_config.h> // Get autoconf configuration settings
#include <IndexStoreDB_LLVMSupport/llvm_Support_raw_ostream.h>
#include <string.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

namespace llvm {
namespace sys {

#if HAVE_ERRNO_H
std::string StrError() {
  return StrError(errno);
}
#endif  // HAVE_ERRNO_H

std::string StrError(int errnum) {
  std::string str;
  if (errnum == 0)
    return str;
#if defined(HAVE_STRERROR_R) || HAVE_DECL_STRERROR_S
  const int MaxErrStrLen = 2000;
  char buffer[MaxErrStrLen];
  buffer[0] = '\0';
#endif

#ifdef HAVE_STRERROR_R
  // strerror_r is thread-safe.
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
  // glibc defines its own incompatible version of strerror_r
  // which may not use the buffer supplied.
  str = strerror_r(errnum, buffer, MaxErrStrLen - 1);
#else
  strerror_r(errnum, buffer, MaxErrStrLen - 1);
  str = buffer;
#endif
#elif HAVE_DECL_STRERROR_S // "Windows Secure API"
  strerror_s(buffer, MaxErrStrLen - 1, errnum);
  str = buffer;
#elif defined(HAVE_STRERROR)
  // Copy the thread un-safe result of strerror into
  // the buffer as fast as possible to minimize impact
  // of collision of strerror in multiple threads.
  str = strerror(errnum);
#else
  // Strange that this system doesn't even have strerror
  // but, oh well, just use a generic message
  raw_string_ostream stream(str);
  stream << "Error #" << errnum;
  stream.flush();
#endif
  return str;
}

}  // namespace sys
}  // namespace llvm
