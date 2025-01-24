add_library(CIndexStoreDB STATIC
  CIndexStoreDB.cpp)
target_compile_options(CIndexStoreDB PRIVATE
  -fblocks)
target_include_directories(CIndexStoreDB PUBLIC
 include)
target_link_libraries(CIndexStoreDB PRIVATE
  Index)
if(NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  target_link_libraries(CIndexStoreDB PRIVATE
    dispatch)
endif()

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS CIndexStoreDB)
endif()
