cmake_minimum_required(VERSION 3.15.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

project(IndexStoreDB
  LANGUAGES C CXX Swift)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_POSITION_INDEPENDENT_CODE YES)

find_package(dispatch CONFIG)
find_package(Foundation CONFIG)
find_package(LMDB CONFIG)

include(SwiftSupport)

add_subdirectory(Sources)
add_subdirectory(cmake/modules)
