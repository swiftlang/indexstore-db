#include <IndexStoreDB_LLVMSupport/llvm_Support_Locale.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Unicode.h>

namespace llvm {
namespace sys {
namespace locale {

int columnWidth(StringRef Text) {
  return llvm::sys::unicode::columnWidthUTF8(Text);
}

bool isPrint(int UCS) {
  return llvm::sys::unicode::isPrintable(UCS);
}

} // namespace locale
} // namespace sys
} // namespace llvm
