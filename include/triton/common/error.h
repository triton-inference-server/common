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
#pragma once

#include <string>

namespace triton { namespace common {

//
// Error
//
// Error returned by utilities from common repo.
//
class Error {
 public:
  enum class Code {
    SUCCESS,
    UNKNOWN,
    INTERNAL,
    NOT_FOUND,
    INVALID_ARG,
    UNAVAILABLE,
    UNSUPPORTED,
    ALREADY_EXISTS,
    CANCELLED
  };

  explicit Error(Code code = Code::SUCCESS) : code_(code) {}
  explicit Error(Code code, const std::string& msg) : code_(code), msg_(msg) {}

  // Convenience "success" value. Can be used as Error::Success to
  // indicate no error.
  static const Error Success;

  // Return the code for this status.
  Code ErrorCode() const { return code_; }

  // Return the message for this status.
  const std::string& Message() const { return msg_; }

  // Return true if this status indicates "ok"/"success", false if
  // status indicates some kind of failure.
  bool IsOk() const { return code_ == Code::SUCCESS; }

  // Return the status as a string.
  std::string AsString() const;

  // Return the constant string name for a code.
  static const char* CodeString(const Code code);

 protected:
  Code code_;
  std::string msg_;
};

}}  // namespace triton::common
