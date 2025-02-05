#ifndef LLVM_SUPPORT_LOCALE_H
#define LLVM_SUPPORT_LOCALE_H

#include <IndexStoreDB_LLVMSupport/llvm_Config_indexstoredb-prefix.h>

namespace llvm {
class StringRef;

namespace sys {
namespace locale {

int columnWidth(StringRef s);
bool isPrint(int c);

}
}
}

#endif // LLVM_SUPPORT_LOCALE_H
