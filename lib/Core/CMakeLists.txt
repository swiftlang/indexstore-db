add_library(Core STATIC
  Symbol.cpp)
target_include_directories(Core PRIVATE
  include)
target_link_libraries(Core PRIVATE
  Support
  LLVMSupport)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Core)
endif()
