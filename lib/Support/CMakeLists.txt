add_library(Support STATIC
  Concurrency-Mac.cpp
  FilePathWatcher.cpp
  Logging.cpp
  Logging-Mac.mm
  Logging-NonMac.cpp
  Path.cpp
  PatternMatching.cpp)
target_compile_options(Support PRIVATE
  -fblocks)
target_include_directories(Support PRIVATE
  include)
target_link_libraries(Support PRIVATE
  dispatch
  LLVMSupport)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Support)
endif()
