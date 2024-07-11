# Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
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

cmake_minimum_required (VERSION 3.18)

set(protobuf_MODULE_COMPATIBLE TRUE CACHE BOOL "protobuf_MODULE_COMPATIBLE" FORCE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
include_directories(${Protobuf_INCLUDE_DIRS})

if(${TRITON_COMMON_ENABLE_GRPC})
  find_package(gRPC CONFIG REQUIRED)
  message(STATUS "Using gRPC ${gRPC_VERSION}")
  include_directories($<TARGET_PROPERTY:gRPC::grpc,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

#
# Protobuf
#
if(${TRITON_COMMON_ENABLE_PROTOBUF})
  file(GLOB proto-srcs *.proto)
  protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${proto-srcs})
  if(TRITON_COMMON_ENABLE_PROTOBUF_PYTHON)
    protobuf_generate_python(PROTO_PY ${proto-srcs})
  endif()

  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
  if(${TRITON_COMMON_ENABLE_GRPC})
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
  endif()

  add_library(
    proto-library EXCLUDE_FROM_ALL OBJECT
    ${PROTO_SRCS} ${PROTO_HDRS}
  )

  target_include_directories(
    proto-library
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  )

  target_link_libraries(
    proto-library
    PRIVATE
      common-compile-settings
  )

  set_target_properties(
    proto-library
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
  )

  if(TRITON_COMMON_ENABLE_PROTOBUF_PYTHON)
    add_custom_target(proto-py-library DEPENDS ${PROTO_PY})
  endif()

  install(
    FILES
      ${PROTO_HDRS}
    DESTINATION include
  )
endif()

#
# GRPC
#
if(${TRITON_COMMON_ENABLE_GRPC})
  get_filename_component(grpc_service_proto_abspath "grpc_service.proto" ABSOLUTE)
  get_filename_component(grpc_service_proto_dir "${grpc_service_proto_abspath}" PATH)
  set(GRPC_SRCS "grpc_service.grpc.pb.cc")
  set(GRPC_HDRS "grpc_service.grpc.pb.h")
  if(TRITON_COMMON_ENABLE_PROTOBUF_PYTHON)
    set(GRPC_PY "grpc_service.grpc.py")
  endif()

  add_custom_command(
    OUTPUT "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS
      --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
      --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
      -I "${grpc_service_proto_dir}"
      --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
      "grpc_service.proto"
    DEPENDS "grpc_service.proto" proto-library
  )

  if(TRITON_COMMON_ENABLE_PROTOBUF_PYTHON)
    find_package(Python REQUIRED COMPONENTS Interpreter)
    add_custom_command(
      OUTPUT "${GRPC_PY}"
      COMMAND ${Python_EXECUTABLE}
      ARGS
        -m grpc_tools.protoc
        -I "${grpc_service_proto_dir}"
        --grpc_python_out "${CMAKE_CURRENT_BINARY_DIR}"
        "grpc_service.proto"
      DEPENDS "grpc_service.proto" proto-library
    )
  endif()

  add_library(
    grpc-service-library EXCLUDE_FROM_ALL OBJECT
    ${GRPC_SRCS} ${GRPC_HDRS}
  )

  target_include_directories(
    grpc-service-library
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  )

  target_link_libraries(
    grpc-service-library
    PRIVATE
      common-compile-settings
  )

  set_target_properties(
    grpc-service-library
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
  )

  if(TRITON_COMMON_ENABLE_PROTOBUF_PYTHON)
    add_custom_target(grpc-service-py-library DEPENDS ${GRPC_PY})
  endif()

  install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/grpc_service.grpc.pb.h
    DESTINATION include
    OPTIONAL
  )
endif()

#
# GRPC Health Service
#
if(${TRITON_COMMON_ENABLE_GRPC})
  get_filename_component(health_proto_abspath "health.proto" ABSOLUTE)
  get_filename_component(health_proto_dir "${health_proto_abspath}" PATH)
  set(HEALTH_SRCS "health.grpc.pb.cc")
  set(HEALTH_HDRS "health.grpc.pb.h")

  add_custom_command(
    OUTPUT "${HEALTH_SRCS}" "${HEALTH_HDRS}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS
      --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
      --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
      -I "${health_proto_dir}"
      --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
      "health.proto"
    DEPENDS "health.proto" proto-library
  )

  add_library(
    grpc-health-library EXCLUDE_FROM_ALL OBJECT
    ${HEALTH_SRCS} ${HEALTH_HDRS}
  )

  target_include_directories(
    grpc-health-library
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  )

  target_link_libraries(
    grpc-health-library
    PRIVATE
      common-compile-settings
  )

  set_target_properties(
    grpc-health-library
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
  )

  install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/health.grpc.pb.h
    DESTINATION include
    OPTIONAL
  )
endif()
