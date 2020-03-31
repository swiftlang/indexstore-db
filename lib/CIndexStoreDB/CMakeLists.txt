add_library(CIndexStoreDB STATIC
  CIndexStoreDB.cpp)
target_compile_options(CIndexStoreDB PRIVATE
  -fblocks)
target_include_directories(CIndexStoreDB PUBLIC
  ${PROJECT_SOURCE_DIR}/include/CIndexStoreDB)
target_link_libraries(CIndexStoreDB PRIVATE
  dispatch
  LLVMSupport)
target_link_libraries(CIndexStoreDB PUBLIC
  Index)

if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY IndexStoreDB_EXPORTS CIndexStoreDB)
endif()
