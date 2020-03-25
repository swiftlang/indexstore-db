add_library(LLVMSupport STATIC
  Support/AArch64TargetParser.cpp
  Support/Allocator.cpp
  Support/APFloat.cpp
  Support/APInt.cpp
  Support/APSInt.cpp
  Support/ARMTargetParser.cpp
  Support/Atomic.cpp
  Support/Chrono.cpp
  Support/circular_raw_ostream.cpp
  Support/CommandLine.cpp
  Support/ConvertUTF.cpp
  Support/ConvertUTFWrapper.cpp
  Support/Debug.cpp
  Support/DJB.cpp
  Support/Errno.cpp
  Support/Error.cpp
  Support/ErrorHandling.cpp
  Support/FileUtilities.cpp
  Support/FoldingSet.cpp
  Support/FormatVariadic.cpp
  Support/Hashing.cpp
  Support/Host.cpp
  Support/JSON.cpp
  Support/LineIterator.cpp
  Support/Locale.cpp
  Support/ManagedStatic.cpp
  Support/MathExtras.cpp
  Support/MD5.cpp
  Support/MemoryBuffer.cpp
  Support/Memory.cpp
  Support/Mutex.cpp
  Support/NativeFormatting.cpp
  Support/Optional.cpp
  Support/Options.cpp
  Support/Path.cpp
  Support/PrettyStackTrace.cpp
  Support/Process.cpp
  Support/Program.cpp
  Support/raw_ostream.cpp
  Support/Signals.cpp
  Support/Signposts.cpp
  Support/SmallPtrSet.cpp
  Support/SmallVector.cpp
  Support/SourceMgr.cpp
  Support/Statistic.cpp
  Support/StringExtras.cpp
  Support/StringMap.cpp
  Support/StringRef.cpp
  Support/StringSaver.cpp
  Support/TargetParser.cpp
  Support/Threading.cpp
  Support/Timer.cpp
  Support/ToolOutputFile.cpp
  Support/Triple.cpp
  Support/Twine.cpp
  Support/UnicodeCaseFold.cpp
  Support/Unicode.cpp
  Support/Valgrind.cpp
  Support/VersionTuple.cpp
  Support/Watchdog.cpp
  Support/WithColor.cpp
  Support/YAMLParser.cpp
  Support/YAMLTraits.cpp)
target_compile_definitions(LLVMSupport PUBLIC
  LLVM_ENABLE_EXCEPTIONS=1)
if(CMAKE_CXX_SIMULATE_ID MATCHES MSVC)
  target_compile_options(LLVMSupport PUBLIC
    /EHsc
    /GR)
endif()
target_include_directories(LLVMSupport PUBLIC
  ${PROJECT_SOURCE_DIR}/include)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS LLVMSupport)
endif()
