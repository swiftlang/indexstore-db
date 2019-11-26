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
target_include_directories(Index PRIVATE
  include)
target_link_libraries(Index PRIVATE
  dispatch
  Database
  LLVMSupport)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Index)
endif()
