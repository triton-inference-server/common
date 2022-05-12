# Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#
# Options
#
# Some components are expensive to build and have extensive
# dependencies, so those parts of the build must be enabled
# explicitly.
option(TRITON_COMMON_ENABLE_PROTOBUF "Build protobuf artifacts" OFF)
option(TRITON_COMMON_ENABLE_PROTOBUF_PYTHON "Build protobuf artifacts for python" ON)
option(TRITON_COMMON_ENABLE_GRPC "Build grpc artifacts" OFF)
option(TRITON_COMMON_ENABLE_JSON "Build json-related libs" ON)

if(TRITON_COMMON_ENABLE_JSON)
  find_package(RapidJSON CONFIG REQUIRED)
  message(STATUS "RapidJSON found. Headers: ${RAPIDJSON_INCLUDE_DIRS}")
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message("Using MSVC as compiler, default target on Windows 10. "
		  "If the target system is not Windows 10, please update _WIN32_WINNT "
		  "to corresponding value.")
endif()

add_library(common-compile-settings INTERFACE)

target_compile_features(common-compile-settings INTERFACE cxx_std_11)

target_compile_options(common-compile-settings INTERFACE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wno-unused-parameter -Wno-type-limits -Werror>
  $<$<CXX_COMPILER_ID:MSVC>:/W0 /D_WIN32_WINNT=0x0A00 /EHsc>
)

#
# Error
#
add_library(
  triton-common-error
  src/error.cc
  )

add_library(
  TritonCommon::triton-common-error ALIAS triton-common-error
)

target_include_directories(
  triton-common-error
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(triton-common-error PRIVATE common-compile-settings)

#
# Logging
#
add_library(
  triton-common-logging
  src/logging.cc
)

add_library(
  TritonCommon::triton-common-logging ALIAS triton-common-logging
)

target_include_directories(
  triton-common-logging
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(${TRITON_ENABLE_LOGGING})
  target_compile_definitions(
    triton-common-logging
    PRIVATE TRITON_ENABLE_LOGGING=1
  )
endif() # TRITON_ENABLE_LOGGING

target_link_libraries(triton-common-logging PRIVATE common-compile-settings)

#
# SyncQueue
#
add_library(
  triton-common-sync-queue INTERFACE
)

add_library(
  TritonCommon::triton-common-sync-queue ALIAS triton-common-sync-queue
)

target_include_directories(
  triton-common-sync-queue
  INTERFACE
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

#
# Async Work Queue
#
add_library(
  triton-common-async-work-queue
  src/async_work_queue.cc
  src/error.cc
)

add_library(
  TritonCommon::triton-common-async-work-queue ALIAS  triton-common-async-work-queue
)

target_include_directories(
  triton-common-async-work-queue
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(triton-common-async-work-queue
  PUBLIC
    Threads::Threads
  PRIVATE
    common-compile-settings
)

#
# JSON utilities
#
if(TRITON_COMMON_ENABLE_JSON)
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
endif()

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

target_link_libraries(triton-common-table-printer PRIVATE common-compile-settings)

set_target_properties(
  triton-common-async-work-queue
  triton-common-error
  triton-common-logging
  triton-common-table-printer
  PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    POSITION_INDEPENDENT_CODE ON
)

set_target_properties(
  triton-common-async-work-queue
  PROPERTIES
    OUTPUT_NAME tritonasyncworkqueue
)

set_target_properties(
  triton-common-error
  PROPERTIES
    OUTPUT_NAME tritoncommonerror
)

set_target_properties(
  triton-common-logging
  PROPERTIES
    OUTPUT_NAME tritoncommonlogging
)

set_target_properties(
  triton-common-table-printer
  PROPERTIES
    OUTPUT_NAME tritontableprinter
)

#
# Protobuf and GRPC artifacts
#
if(${TRITON_COMMON_ENABLE_PROTOBUF} OR ${TRITON_COMMON_ENABLE_GRPC})
  add_subdirectory(protobuf)

  set(protobuf_MODULE_COMPATIBLE TRUE CACHE BOOL "protobuf_MODULE_COMPATIBLE" FORCE)
  find_package(Protobuf CONFIG REQUIRED)
  message(STATUS "Using protobuf ${Protobuf_VERSION}")

  #
  # Model Config (depends on protobuf & generated .pb.h file)
  #
  add_library(
    triton-common-model-config
    src/model_config.cc
    )

  add_library(
    TritonCommon::triton-common-model-config ALIAS triton-common-model-config
  )

  target_include_directories(
    triton-common-model-config
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${Protobuf_INCLUDE_DIRS}
  )

  target_link_libraries(
    triton-common-model-config
    PRIVATE
      common-compile-settings
      protobuf::libprotobuf
      proto-library
  )

  set_target_properties(
    triton-common-model-config
    PROPERTIES
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
      POSITION_INDEPENDENT_CODE ON
      OUTPUT_NAME tritoncommonmodelconfig
  )

endif()

#
# Install
#
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/TritonCommon)

install(
  TARGETS
    triton-common-async-work-queue
    triton-common-error
    triton-common-logging
    triton-common-sync-queue
    triton-common-table-printer
    common-compile-settings
  EXPORT
    triton-common-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(TRITON_COMMON_ENABLE_JSON)
  install(
    TARGETS
      triton-common-json
    EXPORT
      triton-common-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()

if(${TRITON_COMMON_ENABLE_GRPC} OR ${TRITON_COMMON_ENABLE_PROTOBUF})
  install(
    TARGETS
      proto-library
      triton-common-model-config
#      proto-py-library
    EXPORT
      triton-common-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()

if(${TRITON_COMMON_ENABLE_GRPC})
  install(
    TARGETS
      grpc-service-library
#      grpc-service-py-library
    EXPORT
      triton-common-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()

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
