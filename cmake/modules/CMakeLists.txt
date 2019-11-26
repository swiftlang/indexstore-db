
set(IndexStoreDB_EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/IndexStoreDBExports.cmake)
configure_file(IndexStoreDBConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/IndexStoreDBConfig.cmake)

get_property(IndexStoreDB_EXPORTS GLOBAL PROPERTY IndexStoreDB_EXPORTS)
export(TARGETS ${IndexStoreDB_EXPORTS} FILE ${IndexStoreDB_EXPORTS_FILE})
