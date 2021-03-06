project(TileSweep)
cmake_minimum_required(VERSION 3.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(ARTIFACT_DIR "${CMAKE_BINARY_DIR}/bundle")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARTIFACT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/modules")

option(WITH_MAPNIK "Use Mapnik" ON)

if (WITH_MAPNIK)
  find_package(Mapnik REQUIRED)
endif (WITH_MAPNIK)

SET(DISABLE_LIBUV ON)
SET(WITH_BUNDLED_SSL ON)
add_definitions(-DH2O_USE_LIBUV=0)
add_subdirectory("third_party/h2o")

include_directories(${H2O_SSL_INCLUDE_DIR})
add_subdirectory(src)
