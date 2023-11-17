// Copyright 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#ifdef TRITON_ENABLE_NVTX

#include <nvtx3/nvToolsExt.h>

namespace triton { namespace common {

// Updates a server stat with duration measured by a C++ scope.
class NvtxRange {
 public:
  explicit NvtxRange(const char* label, uint32_t rgb = kNvGreen);

  explicit NvtxRange(const std::string& label, uint32_t rgb = kNvGreen)
      : NvtxRange(label.c_str(), rgb)
  {
  }

  ~NvtxRange();

  static constexpr uint32_t kNvGreen = 0x76b900;
  static constexpr uint32_t kRed = 0xc1121f;
  static constexpr uint32_t kGreen = 0x588157;
  static constexpr uint32_t kBlue = 0x023047;
  static constexpr uint32_t kYellow = 0xffb703;
  static constexpr uint32_t kOrange = 0xfb8500;

 private:
  nvtxEventAttributes_t GetAttributes(const char* label, uint32_t rgb);
};

}}  // namespace triton::common

#endif  // TRITON_ENABLE_NVTX

//
// Macros to access NVTX functionality.
// For `NVTX_RANGE` macro please refer to the usage below.
//
#ifdef TRITON_ENABLE_NVTX
#define NVTX_INITIALIZE nvtxInitialize(nullptr)
#define NVTX_RANGE1(V, L) triton::common::NvtxRange V(L)
#define NVTX_RANGE2(V, L, RGB) triton::common::NvtxRange V(L, RGB)
#define NVTX_MARKER(L) nvtxMarkA(L)
#else
#define NVTX_INITIALIZE
#define NVTX_RANGE1(V, L)
#define NVTX_RANGE2(V, L, RGB)
#define NVTX_MARKER(L)
#endif  // TRITON_ENABLE_NVTX

// "Overload" for `NVTX_RANGE` macro.
// Usage:
// NVTX_RANGE(nvtx1, "My message")  -> Records NVTX marker with kNvGreen color.
// NVTX_RANGE(nvtx1, "My message", NvtxRange::kRed)  -> Records NVTX marker with
//                                                      kRed color.
#define GET_NVTX_MACRO(_1, _2, _3, NAME, ...) NAME
#define NVTX_RANGE(...) \
  GET_NVTX_MACRO(__VA_ARGS__, NVTX_RANGE2, NVTX_RANGE1)(__VA_ARGS__)
