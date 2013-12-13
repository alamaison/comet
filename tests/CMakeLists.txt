#
# Copyright (C) 2012, 2013 Alexander Lamaison <alexander.lamaison@gmail.com>
#
# This material is provided "as is", with absolutely no warranty
# expressed or implied. Any use is at your own risk. Permission to
# use or copy this software for any purpose is hereby granted without
# fee, provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is
# granted, provided the above notices are retained, and a notice that
# the code was modified is included with the above copyright notice.
#
# This file is part of Comet version 2.
# https://github.com/alamaison/comet
#

cmake_minimum_required(VERSION 2.4)

set(Boost_USE_STATIC_LIBS TRUE)
set(Boost_DETAILED_FAILURE_MSG TRUE)
find_package(
  Boost 1.40 REQUIRED COMPONENTS  unit_test_framework)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

include_directories(../include)

include(max_warnings.cmake)


project(comet-unit-tests)

set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/module.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/bstr.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/currency.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/datetime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/enum.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ptr.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/registry.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/safearray.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/threading.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tlbinfo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/typelist.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/uuid.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/variant.cpp)

add_executable(unit-tests ${SOURCES})

add_custom_target(RUN_TESTS ALL
  $<TARGET_FILE:unit-tests>
  --catch_system_errors
  --detect_memory_leaks
  --result_code=no)

# Generate tests that library headers are self-containes
project(comet-include-tests)

foreach(header_file ${LIBRARY_HEADERS})

  # Extract the header file name and convert it to a build file name
  get_filename_component(filename ${header_file} NAME_WE)
  set(BUILD_FILE ${CMAKE_CURRENT_BINARY_DIR}/${filename}.cpp)

  add_custom_command(
    OUTPUT ${BUILD_FILE} VERBATIM
    COMMAND cmake -E echo \#include <${header_file}> > ${BUILD_FILE})

  list(APPEND INCLUDE_TEST_SOURCES ${BUILD_FILE})

endforeach()

add_executable(include-tests main.cpp ${INCLUDE_TEST_SOURCES})
