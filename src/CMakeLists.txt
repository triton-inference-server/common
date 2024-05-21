# Copyright 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#
# Error
#
add_library(
  triton-common-error
  error.cc
  )

add_library(
  TritonCommon::triton-common-error ALIAS triton-common-error
)

target_include_directories(
  triton-common-error
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(triton-common-error PRIVATE common-compile-settings)

#
# Logging
#
add_library(
  triton-common-logging
  logging.cc
)

add_library(
  TritonCommon::triton-common-logging ALIAS triton-common-logging
)

target_include_directories(
  triton-common-logging
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PRIVATE
    ${RAPIDJSON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(${TRITON_ENABLE_LOGGING})
  target_compile_definitions(
    triton-common-logging
    PRIVATE TRITON_ENABLE_LOGGING=1
  )
endif() # TRITON_ENABLE_LOGGING

target_link_libraries(triton-common-logging PRIVATE common-compile-settings)

#
# Async Work Queue
#
add_library(
  triton-common-async-work-queue
  async_work_queue.cc
  error.cc
  thread_pool.cc
)

add_library(
  TritonCommon::triton-common-async-work-queue ALIAS  triton-common-async-work-queue
)

target_include_directories(
  triton-common-async-work-queue
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(triton-common-async-work-queue
  PUBLIC
    Threads::Threads
  PRIVATE
    common-compile-settings
)

#
# Thread Pool
#
add_library(
  triton-common-thread-pool
  thread_pool.cc
)

add_library(
  TritonCommon::triton-common-thread-pool ALIAS  triton-common-thread-pool
)

target_include_directories(
  triton-common-thread-pool
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(triton-common-thread-pool
  PUBLIC
    Threads::Threads
  PRIVATE
    common-compile-settings
)

#
# Table Printer
#
add_library(
  triton-common-table-printer
  table_printer.cc
)

add_library(
  TritonBackend::triton-common-table-printer ALIAS triton-common-table-printer
)

target_include_directories(
  triton-common-table-printer
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(triton-common-table-printer PRIVATE common-compile-settings)

set_target_properties(
  triton-common-async-work-queue
  triton-common-error
  triton-common-logging
  triton-common-table-printer
  triton-common-thread-pool
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
  triton-common-thread-pool
  PROPERTIES
    OUTPUT_NAME tritonthreadpool
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

add_subdirectory(test)
