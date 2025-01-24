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
target_include_directories(Support PUBLIC
  include)
target_link_libraries(Support PUBLIC
  LLVMSupport)
if(NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  target_link_libraries(Support PRIVATE
    dispatch)
endif()

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Support)
endif()
