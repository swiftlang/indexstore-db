//===-- llvm/ADT/bit.h - C++20 <bit> ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the C++20 <bit> header.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_BIT_H
#define LLVM_ADT_BIT_H

#include <IndexStoreDB_LLVMSupport/llvm_Support_Compiler.h>
#include <cstring>
#include <type_traits>

namespace llvm {

// This implementation of bit_cast is different from the C++17 one in two ways:
//  - It isn't constexpr because that requires compiler support.
//  - It requires trivially-constructible To, to avoid UB in the implementation.
template <typename To, typename From
          , typename = typename std::enable_if<sizeof(To) == sizeof(From)>::type
#if (__has_feature(is_trivially_constructible) && defined(_LIBCPP_VERSION)) || \
    (defined(__GNUC__) && __GNUC__ >= 5)
          , typename = typename std::is_trivially_constructible<To>::type
#elif __has_feature(is_trivially_constructible)
          , typename = typename std::enable_if<__is_trivially_constructible(To)>::type
#else
  // See comment below.
#endif
#if (__has_feature(is_trivially_copyable) && defined(_LIBCPP_VERSION)) || \
    (defined(__GNUC__) && __GNUC__ >= 5)
          , typename = typename std::enable_if<std::is_trivially_copyable<To>::value>::type
          , typename = typename std::enable_if<std::is_trivially_copyable<From>::value>::type
#elif __has_feature(is_trivially_copyable)
          , typename = typename std::enable_if<__is_trivially_copyable(To)>::type
          , typename = typename std::enable_if<__is_trivially_copyable(From)>::type
#else
// This case is GCC 4.x. clang with libc++ or libstdc++ never get here. Unlike
// llvm/Support/type_traits.h's is_trivially_copyable we don't want to
// provide a good-enough answer here: developers in that configuration will hit
// compilation failures on the bots instead of locally. That's acceptable
// because it's very few developers, and only until we move past C++11.
#endif
>
inline To bit_cast(const From &from) noexcept {
  To to;
  std::memcpy(&to, &from, sizeof(To));
  return to;
}

} // namespace llvm

#endif
