// This is a stand-in instead of the real 'Demangle.h' to avoid needing to
// bring in the Demangle library.

#ifndef LLVM_DEMANGLE_DEMANGLE_H
#define LLVM_DEMANGLE_DEMANGLE_H

#include "llvm/Config/indexstoredb-prefix.h"

static inline char *itaniumDemangle(const char *mangled_name, char *buf,
                                    size_t *n, int *status) {
  return nullptr;
}

#endif
