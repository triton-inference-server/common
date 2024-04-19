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

#include "triton/common/logging.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "triton/common/error.h"

static const uint8_t INTERNAL_ERROR =
    static_cast<uint8_t>(triton::common::Error::Code::INTERNAL);

static const uint8_t SUCCESS =
    static_cast<uint8_t>(triton::common::Error::Code::SUCCESS);

#define TRITONJSON_STATUSTYPE uint8_t
#define TRITONJSON_STATUSRETURN(M)              \
  do {                                          \
    LOG_ERROR << (M) << ": " << INTERNAL_ERROR; \
    return INTERNAL_ERROR;                      \
  } while (false)

#define TRITONJSON_STATUSSUCCESS SUCCESS
#include "triton/common/triton_json.h"

namespace triton { namespace common {

Logger gLogger_;

Logger::Logger()
    : enables_{true, true, true}, vlevel_(0), format_(Format::kDEFAULT)
{
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

const std::array<const char*, LogMessage::Level::kINFO + 1>
    LogMessage::LEVEL_NAMES_{"Error", "Warning", "Info"};
const std::vector<char> LogMessage::level_name_{'E', 'W', 'I'};

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
    case Logger::Format::JSONL: {
      stream << timestamp_.wYear << '-' << std::setfill('0') << std::setw(2)
             << timestamp_.wMonth << '-' << std::setw(2) << timestamp_.wDay
             << 'T' << std::setw(2) << timestamp_.wHour << ':' << std::setw(2)
             << timestamp_.wMinute << ':' << std::setw(2) << timestamp_.wSecond
             << '.' << std::setw(6) << timestamp_.wMilliseconds * 1000 << "Z";
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
    case Logger::Format::kJSONL: {
      stream << (tm_time.tm_year + 1900) << '-' << std::setfill('0')
             << std::setw(2) << (tm_time.tm_mon + 1) << '-' << std::setw(2)
             << tm_time.tm_mday << 'T' << std::setw(2) << tm_time.tm_hour << ':'
             << std::setw(2) << tm_time.tm_min << ':' << std::setw(2)
             << tm_time.tm_sec << '.' << std::setw(6) << timestamp_.tv_usec
             << "Z";
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
      stream << LEVEL_NAMES_[level_][0];
      LogTimestamp(stream);
      stream << ' ' << pid_ << ' ' << path_ << ':' << line_ << "] ";

      break;
    }
    case Logger::Format::kISO8601: {
      LogTimestamp(stream);
      stream << " " << LEVEL_NAMES_[level_][0] << ' ' << pid_ << ' ' << path_
             << ':' << line_ << "] ";
      break;
    }
    case Logger::Format::kJSONL: {
      break;
    }
  }
}


LogMessage::~LogMessage()
{
  gLogger_.SetLogFormat(Logger::Format::kJSONL);

  switch (gLogger_.LogFormat()) {
    case Logger::Format::kDEFAULT:
    case Logger::Format::kISO8601: {
      std::stringstream preamble;
      LogPreamble(preamble);
      preamble << message_.rdbuf();
      message_.str("");
      gLogger_.Log(preamble.str());
      break;
    }
    case Logger::Format::kJSONL: {
      TritonJson::Value logMessage(TritonJson::ValueType::OBJECT);
      TritonJson::WriteBuffer buffer;
      logMessage.AddString("file", path_);
      logMessage.AddInt("line", line_);
      logMessage.AddString("level", LEVEL_NAMES_[level_]);
      logMessage.AddInt("pid", pid_);
      logMessage.AddString("message", message_.str());
      logMessage.Write(&buffer);
      gLogger_.Log(buffer.Contents());
      break;
    }
  }
}
}}  // namespace triton::common
