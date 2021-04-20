// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#include "triton/common/error.h"

namespace triton { namespace common {

const Error Error::Success(Error::Code::SUCCESS);

std::string
Error::AsString() const
{
  std::string str(CodeString(code_));
  str += ": " + msg_;
  return str;
}

const char*
Error::CodeString(const Code code)
{
  switch (code) {
    case Error::Code::SUCCESS:
      return "OK";
    case Error::Code::UNKNOWN:
      return "Unknown";
    case Error::Code::INTERNAL:
      return "Internal";
    case Error::Code::NOT_FOUND:
      return "Not found";
    case Error::Code::INVALID_ARG:
      return "Invalid argument";
    case Error::Code::UNAVAILABLE:
      return "Unavailable";
    case Error::Code::UNSUPPORTED:
      return "Unsupported";
    case Error::Code::ALREADY_EXISTS:
      return "Already exists";
    default:
      break;
  }

  return "<invalid code>";
}

}}  // namespace triton::common
