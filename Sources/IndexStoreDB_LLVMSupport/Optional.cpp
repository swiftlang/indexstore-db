//===- Optional.cpp - Optional values ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <IndexStoreDB_LLVMSupport/llvm_ADT_Optional.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_raw_ostream.h>

llvm::raw_ostream &llvm::operator<<(raw_ostream &OS, NoneType) {
  return OS << "None";
}
