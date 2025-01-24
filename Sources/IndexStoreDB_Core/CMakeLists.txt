add_library(Core STATIC
  Symbol.cpp)
target_include_directories(Core PUBLIC
  include)
target_link_libraries(Core PUBLIC
  Support)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS Core)
endif()
