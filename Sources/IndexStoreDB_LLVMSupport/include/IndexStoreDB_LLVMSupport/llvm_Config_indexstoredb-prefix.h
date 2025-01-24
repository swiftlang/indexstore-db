/*===------- llvm/Config/indexstoredb-prefix.h --------------------*- C -*-===*/
/*                                                                            */
/*                     The LLVM Compiler Infrastructure                       */
/*                                                                            */
/* This file is distributed under the University of Illinois Open Source      */
/* License. See LICENSE.TXT for details.                                      */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

#ifndef INDEXSTOREDB_PREFIX_H
#define INDEXSTOREDB_PREFIX_H

/* HACK: Rename all of the llvm symbols so that they will not collide if another
 * copy of llvm is linked into the same image. The use of llvm within IndexStore
 * is purely an implementation detail. Using a source-level rename is a
 * workaround for the lack of symbol visibility controls in swiftpm. Ideally we
 * could do this with a combination of `-fvisibility=hidden` and `ld -r`.
*/

#define llvm indexstoredb_llvm
#define LLVMEnablePrettyStackTrace indexstoredb_LLVMEnablePrettyStackTrace
#define LLVMParseCommandLineOptions indexstoredb_LLVMParseCommandLineOptions
#define LLVMResetFatalErrorHandler indexstoredb_LLVMResetFatalErrorHandler
#define LLVMInstallFatalErrorHandler indexstoredb_LLVMInstallFatalErrorHandler

#endif // INDEXSTOREDB_PREFIX_H
