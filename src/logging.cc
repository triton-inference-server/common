// Copyright 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <algorithm>
#include <iomanip>
#include <iostream>

// Defined but not used
#define TRITONJSON_STATUSTYPE uint8_t
#define TRITONJSON_STATUSRETURN(M)
#define TRITONJSON_STATUSSUCCESS 0

#include "triton/common/logging.h"
#include "triton/common/triton_json.h"

namespace triton { namespace common {

Logger gLogger_;

Logger::Logger()
    : enables_{true, true, true}, vlevel_(0), format_(Format::kDEFAULT)
{
  const char* value = std::getenv(Logger::ESCAPE_ENVIRONMENT_VARIABLE);
  escape_log_messages_ = (value && std::strcmp(value, "0") == 0) ? false : true;
}

void
Logger::Log(const std::string& msg)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  if (file_stream_.is_open()) {
    file_stream_ << msg << std::endl;
  } else {
    std::cerr << msg << std::endl;
  }
}

void
Logger::Flush()
{
  std::cerr << std::flush;
}

#ifdef _WIN32

void
LogMessage::LogTimestamp(std::stringstream& stream)
{
  switch (gLogger_.LogFormat()) {
    case Logger::Format::kDEFAULT: {
      stream << std::setfill('0') << std::setw(2) << timestamp_.wMonth
             << std::setw(2) << timestamp_.wDay << ' ' << std::setw(2)
             << timestamp_.wHour << ':' << std::setw(2) << timestamp_.wMinute
             << ':' << std::setw(2) << timestamp_.wSecond << '.' << std::setw(6)
             << timestamp_.wMilliseconds * 1000;
      break;
    }
    case Logger::Format::kISO8601: {
      stream << timestamp_.wYear << '-' << std::setfill('0') << std::setw(2)
             << timestamp_.wMonth << '-' << std::setw(2) << timestamp_.wDay
             << 'T' << std::setw(2) << timestamp_.wHour << ':' << std::setw(2)
             << timestamp_.wMinute << ':' << std::setw(2) << timestamp_.wSecond
             << "Z";
      break;
    }
  }
}
#else
void
LogMessage::LogTimestamp(std::stringstream& stream)
{
  struct tm tm_time;
  gmtime_r(((time_t*)&(timestamp_.tv_sec)), &tm_time);

  switch (gLogger_.LogFormat()) {
    case Logger::Format::kDEFAULT: {
      stream << std::setfill('0') << std::setw(2) << (tm_time.tm_mon + 1)
             << std::setw(2) << tm_time.tm_mday << ' ' << std::setw(2)
             << tm_time.tm_hour << ':' << std::setw(2) << tm_time.tm_min << ':'
             << std::setw(2) << tm_time.tm_sec << '.' << std::setw(6)
             << timestamp_.tv_usec;
      break;
    }
    case Logger::Format::kISO8601: {
      stream << (tm_time.tm_year + 1900) << '-' << std::setfill('0')
             << std::setw(2) << (tm_time.tm_mon + 1) << '-' << std::setw(2)
             << tm_time.tm_mday << 'T' << std::setw(2) << tm_time.tm_hour << ':'
             << std::setw(2) << tm_time.tm_min << ':' << std::setw(2)
             << tm_time.tm_sec << "Z";
      break;
    }
  }
}

#endif

void
LogMessage::LogPreamble(std::stringstream& stream)
{
  switch (gLogger_.LogFormat()) {
    case Logger::Format::kDEFAULT: {
      stream << Logger::LEVEL_NAMES[static_cast<uint8_t>(level_)];
      LogTimestamp(stream);
      stream << ' ' << pid_ << ' ' << path_ << ':' << line_ << "] ";

      break;
    }
    case Logger::Format::kISO8601: {
      LogTimestamp(stream);
      stream << " " << Logger::LEVEL_NAMES[static_cast<uint8_t>(level_)] << ' '
             << pid_ << ' ' << path_ << ':' << line_ << "] ";
      break;
    }
  }
}


LogMessage::~LogMessage()
{
  std::stringstream log_record;
  LogPreamble(log_record);
  std::string escaped_message =
      escape_log_messages_ ? TritonJson::SerializeString(message_.str())
                           : message_.str();
  if (heading_ != nullptr) {
    std::string escaped_heading = gLogger_.EscapeLogMessages()
                                      ? TritonJson::SerializeString(heading_)
                                      : heading_;
    log_record << escaped_heading << '\n';
  }
  log_record << escaped_message;
  gLogger_.Log(log_record.str());
}

}}  // namespace triton::common
