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

cmake_minimum_required(VERSION 3.14)

project(tritoncommon LANGUAGES C CXX)

#
# Triton Server API
#
add_library(
  triton-common-serverapi INTERFACE
)

add_library(
  TritonCommon::triton-common-serverapi ALIAS triton-common-serverapi
)

target_include_directories(
  triton-common-serverapi
  INTERFACE
    $<INSTALL_INTERFACE:include/triton/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

#
# Triton Backend API
#
add_library(
  triton-common-backendapi INTERFACE
)

add_library(
  TritonCommon::triton-common-backendapi ALIAS triton-common-backendapi
)

target_include_directories(
  triton-common-backendapi
  INTERFACE
    $<INSTALL_INTERFACE:include/triton/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

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
    $<INSTALL_INTERFACE:include/triton/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

#
# Install
#
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/TritonCommon)

install(
  TARGETS
    triton-common-json
    triton-common-backendapi
    triton-common-serverapi
  EXPORT
    triton-common-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/triton/common
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
