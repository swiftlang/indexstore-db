//===-- llvm/ADT/APSInt.cpp - Arbitrary Precision Signed Int ---*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the APSInt class, which is a simple class that
// represents an arbitrary sized integer that knows its signedness.
//
//===----------------------------------------------------------------------===//

#include <IndexStoreDB_LLVMSupport/llvm_ADT_APSInt.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_FoldingSet.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>

using namespace llvm;

APSInt::APSInt(StringRef Str) {
  assert(!Str.empty() && "Invalid string length");

  // (Over-)estimate the required number of bits.
  unsigned NumBits = ((Str.size() * 64) / 19) + 2;
  APInt Tmp(NumBits, Str, /*Radix=*/10);
  if (Str[0] == '-') {
    unsigned MinBits = Tmp.getMinSignedBits();
    if (MinBits > 0 && MinBits < NumBits)
      Tmp = Tmp.trunc(MinBits);
    *this = APSInt(Tmp, /*IsUnsigned=*/false);
    return;
  }
  unsigned ActiveBits = Tmp.getActiveBits();
  if (ActiveBits > 0 && ActiveBits < NumBits)
    Tmp = Tmp.trunc(ActiveBits);
  *this = APSInt(Tmp, /*IsUnsigned=*/true);
}

void APSInt::Profile(FoldingSetNodeID& ID) const {
  ID.AddInteger((unsigned) (IsUnsigned ? 1 : 0));
  APInt::Profile(ID);
}
