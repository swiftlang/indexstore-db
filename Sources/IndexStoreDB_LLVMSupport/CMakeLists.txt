add_library(LLVMSupport STATIC
  AArch64TargetParser.cpp
  Allocator.cpp
  APFloat.cpp
  APInt.cpp
  APSInt.cpp
  ARMTargetParser.cpp
  Atomic.cpp
  Chrono.cpp
  circular_raw_ostream.cpp
  CommandLine.cpp
  ConvertUTF.cpp
  ConvertUTFWrapper.cpp
  Debug.cpp
  DJB.cpp
  Errno.cpp
  Error.cpp
  ErrorHandling.cpp
  FileUtilities.cpp
  FoldingSet.cpp
  FormatVariadic.cpp
  Hashing.cpp
  Host.cpp
  JSON.cpp
  LineIterator.cpp
  Locale.cpp
  ManagedStatic.cpp
  MathExtras.cpp
  MD5.cpp
  MemoryBuffer.cpp
  Memory.cpp
  Mutex.cpp
  NativeFormatting.cpp
  Optional.cpp
  Options.cpp
  Path.cpp
  PrettyStackTrace.cpp
  Process.cpp
  Program.cpp
  raw_ostream.cpp
  Signals.cpp
  Signposts.cpp
  SmallPtrSet.cpp
  SmallVector.cpp
  SourceMgr.cpp
  Statistic.cpp
  StringExtras.cpp
  StringMap.cpp
  StringRef.cpp
  StringSaver.cpp
  TargetParser.cpp
  Threading.cpp
  Timer.cpp
  ToolOutputFile.cpp
  Triple.cpp
  Twine.cpp
  UnicodeCaseFold.cpp
  Unicode.cpp
  Valgrind.cpp
  VersionTuple.cpp
  Watchdog.cpp
  WithColor.cpp
  YAMLParser.cpp
  YAMLTraits.cpp)
target_compile_definitions(LLVMSupport PUBLIC
  LLVM_ENABLE_EXCEPTIONS=1)
if(CMAKE_CXX_SIMULATE_ID MATCHES MSVC)
  target_compile_options(LLVMSupport PUBLIC
    /EHsc
    /GR)
endif()
target_include_directories(LLVMSupport PUBLIC
 include)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS LLVMSupport)
endif()
