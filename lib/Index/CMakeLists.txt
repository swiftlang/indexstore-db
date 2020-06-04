add_library(Index STATIC
  FilePathIndex.cpp
  FileVisibilityChecker.cpp
  IndexDatastore.cpp
  indexstore_functions.def
  IndexStoreLibraryProvider.cpp
  IndexSystem.cpp
  StoreSymbolRecord.cpp
  SymbolIndex.cpp)
target_compile_options(Index PRIVATE
  -fblocks)
target_include_directories(Index PUBLIC
  ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Index PRIVATE
  Database
  LLVMSupport)
if(NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  target_link_libraries(Index PRIVATE
    dispatch)
endif()

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Index)
endif()
