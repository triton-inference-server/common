// Copyright 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "triton/common/nvtx.h"

#ifdef TRITON_ENABLE_NVTX

namespace triton::common {

class NvtxTritonDomain {
 public:
  static nvtxDomainHandle_t& GetDomain()
  {
    static NvtxTritonDomain inst;
    return inst.triton_nvtx_domain_;
  }

 private:
  NvtxTritonDomain() { triton_nvtx_domain_ = nvtxDomainCreateA("Triton"); }

  ~NvtxTritonDomain() { nvtxDomainDestroy(triton_nvtx_domain_); }

  nvtxDomainHandle_t triton_nvtx_domain_;
};


NvtxRange::NvtxRange(const char* label, uint32_t rgb)
{
  auto attr = GetAttributes(label, rgb);
  nvtxDomainRangePushEx(NvtxTritonDomain::GetDomain(), &attr);
}


NvtxRange::~NvtxRange()
{
  nvtxDomainRangePop(NvtxTritonDomain::GetDomain());
}


nvtxEventAttributes_t
NvtxRange::GetAttributes(const char* label, uint32_t rgb)
{
  nvtxEventAttributes_t attr;
  attr.version = NVTX_VERSION;
  attr.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
  attr.colorType = NVTX_COLOR_ARGB;
  attr.color = rgb | 0xff000000;
  attr.messageType = NVTX_MESSAGE_TYPE_ASCII;
  attr.message.ascii = label
}

}  // namespace triton::common

#endif  // TRITON_ENABLE_NVTX
