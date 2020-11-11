# Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.17)

project(tritoncommon LANGUAGES C CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message("Using MSVC as compiler, default target on Windows 10. "
		  "If the target system is not Windows 10, please update _WIN32_WINNT "
		  "to corresponding value.")
  set(CMAKE_CXX_FLAGS "/W4 /D_WIN32_WINNT=0x0A00 /EHsc")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
else()
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -Werror -Wno-deprecated-declarations")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(RapidJSON CONFIG REQUIRED)
message(STATUS "RapidJSON found. Headers: ${RAPIDJSON_INCLUDE_DIRS}")

#
# JSON utilities
#
add_library(
  triton-common-json INTERFACE
)

add_library(
  TritonCommon::triton-common-json ALIAS triton-common-json
)

target_include_directories(
  triton-common-json
  INTERFACE
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:${RAPIDJSON_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${RAPIDJSON_INCLUDE_DIRS}>
)

#
# Table Printer
#
add_library(
  triton-common-table-printer
  src/table_printer.cc
)

add_library(
  TritonBackend::triton-common-table-printer ALIAS triton-common-table-printer
)

target_include_directories(
  triton-common-table-printer
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(
  triton-common-table-printer PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME tritontableprinter
)

#
# Install
#
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/TritonCommon)

install(
  TARGETS
    triton-common-json
    triton-common-table-printer
  EXPORT
    triton-common-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT
    triton-common-targets
  FILE
    TritonCommonTargets.cmake
  NAMESPACE
    TritonCommon::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/TritonCommonConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TritonCommonConfig.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TritonCommonConfig.cmake
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

#
# Export from build tree
#
export(
  EXPORT
    triton-common-targets
  FILE
    ${CMAKE_CURRENT_BINARY_DIR}/TritonCommonTargets.cmake
  NAMESPACE
    TritonCommon::
)

export(PACKAGE TritonCommon)
