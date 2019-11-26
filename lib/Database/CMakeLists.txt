add_library(Database STATIC
  Database.cpp
  DatabaseError.cpp
  ImportTransaction.cpp
  ReadTransaction.cpp
  lmdb/mdb.c
  lmdb/midl.c)
target_compile_options(Database PRIVATE
  -fblocks)
target_include_directories(Database PRIVATE
  include)
target_link_libraries(Database PRIVATE
  dispatch
  Core
  LLVMSupport)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Database)
endif()
