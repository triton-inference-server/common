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
#pragma once

#include <cerrno>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace triton { namespace common {

// A log message.
class LogMessage {
 public:
  // Log levels.
  enum Level { kERROR = 0, kWARNING = 1, kINFO = 2 };

  LogMessage(const char* file, int line, uint32_t level);
  ~LogMessage();

  std::stringstream& stream() { return stream_; }

 private:
  static const std::vector<char> level_name_;
  std::stringstream stream_;
};

// Global logger for messages. Controls how log messages are reported.
class Logger {
 public:
  enum class Format { kDEFAULT, kISO8601 };

  Logger();

  // Is a log level enabled.
  bool IsEnabled(LogMessage::Level level) const { return enables_[level]; }

  // Set enable for a log Level.
  void SetEnabled(LogMessage::Level level, bool enable)
  {
    enables_[level] = enable;
  }

  // Get the current verbose logging level.
  uint32_t VerboseLevel() const { return vlevel_; }

  // Set the current verbose logging level.
  void SetVerboseLevel(uint32_t vlevel) { vlevel_ = vlevel; }

  // Get the logging format.
  Format LogFormat() { return format_; }

  // Get the logging format as a string.
  std::string LogFormatString()
  {
    switch (format_) {
      case Format::kISO8601:
        return "ISO8601";
      case Format::kDEFAULT:
        return "default";
      default:
        return "Invalid format";
    }
  }

  // Set the logging format.
  void SetLogFormat(Format format) { format_ = format; }

  // Get the log output file name.
  const std::string& LogFile() { return filename_; }

  // Set the log output file. Returns an empty string upon
  // success, else returns an error string.
  const std::string SetLogFile(const std::string& filename)
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    file_stream_.close();
    std::string revert_name(filename_);
    filename_ = filename;
    if (!filename_.empty()) {
      file_stream_.open(filename_, std::ios::app);
      if (file_stream_.fail()) {
        std::stringstream error;
        error << __FILE__ << " " << __LINE__
              << ": Failed to open log file: " << std::strerror(errno)
              << std::endl;
        filename_ = revert_name;
        file_stream_.open(filename_, std::ios::app);
        return error.str();
      }
    }
    // will return an empty string
    return std::string();
  }

  // Log a message.
  void Log(const std::string& msg);

  // Flush the log.
  void Flush();

 private:
  std::vector<bool> enables_;
  uint32_t vlevel_;
  Format format_;
  std::mutex mutex_;
  std::string filename_;
  std::ofstream file_stream_;
};

extern Logger gLogger_;

#define LOG_ENABLE_INFO(E)             \
  triton::common::gLogger_.SetEnabled( \
      triton::common::LogMessage::Level::kINFO, (E))
#define LOG_ENABLE_WARNING(E)          \
  triton::common::gLogger_.SetEnabled( \
      triton::common::LogMessage::Level::kWARNING, (E))
#define LOG_ENABLE_ERROR(E)            \
  triton::common::gLogger_.SetEnabled( \
      triton::common::LogMessage::Level::kERROR, (E))
#define LOG_SET_VERBOSE(L)                  \
  triton::common::gLogger_.SetVerboseLevel( \
      static_cast<uint32_t>(std::max(0, (L))))
#define LOG_SET_OUT_FILE(FN) triton::common::gLogger_.SetLogFile((FN))
#define LOG_SET_FORMAT(F) triton::common::gLogger_.SetLogFormat((F))

#define LOG_VERBOSE_LEVEL triton::common::gLogger_.VerboseLevel()
#define LOG_FORMAT triton::common::gLogger_.LogFormat()
#define LOG_FORMAT_STRING triton::common::gLogger_.LogFormatString()
#define LOG_FILE triton::common::gLogger_.LogFile()

#ifdef TRITON_ENABLE_LOGGING

#define LOG_INFO_IS_ON \
  triton::common::gLogger_.IsEnabled(triton::common::LogMessage::Level::kINFO)
#define LOG_WARNING_IS_ON             \
  triton::common::gLogger_.IsEnabled( \
      triton::common::LogMessage::Level::kWARNING)
#define LOG_ERROR_IS_ON \
  triton::common::gLogger_.IsEnabled(triton::common::LogMessage::Level::kERROR)
#define LOG_VERBOSE_IS_ON(L) (triton::common::gLogger_.VerboseLevel() >= (L))

#else

// If logging is disabled, define macro to be false to avoid further evaluation
#define LOG_INFO_IS_ON false
#define LOG_WARNING_IS_ON false
#define LOG_ERROR_IS_ON false
#define LOG_VERBOSE_IS_ON(L) false

#endif  // TRITON_ENABLE_LOGGING

// Macros that use explicitly given filename and line number.
#define LOG_INFO_FL(FN, LN)                                      \
  if (LOG_INFO_IS_ON)                                            \
  triton::common::LogMessage(                                    \
      (char*)(FN), LN, triton::common::LogMessage::Level::kINFO) \
      .stream()
#define LOG_WARNING_FL(FN, LN)                                      \
  if (LOG_WARNING_IS_ON)                                            \
  triton::common::LogMessage(                                       \
      (char*)(FN), LN, triton::common::LogMessage::Level::kWARNING) \
      .stream()
#define LOG_ERROR_FL(FN, LN)                                      \
  if (LOG_ERROR_IS_ON)                                            \
  triton::common::LogMessage(                                     \
      (char*)(FN), LN, triton::common::LogMessage::Level::kERROR) \
      .stream()
#define LOG_VERBOSE_FL(L, FN, LN)                                \
  if (LOG_VERBOSE_IS_ON(L))                                      \
  triton::common::LogMessage(                                    \
      (char*)(FN), LN, triton::common::LogMessage::Level::kINFO) \
      .stream()

// Macros that use current filename and line number.
#define LOG_INFO LOG_INFO_FL(__FILE__, __LINE__)
#define LOG_WARNING LOG_WARNING_FL(__FILE__, __LINE__)
#define LOG_ERROR LOG_ERROR_FL(__FILE__, __LINE__)
#define LOG_VERBOSE(L) LOG_VERBOSE_FL(L, __FILE__, __LINE__)


#define LOG_STATUS_ERROR(X, MSG)                         \
  do {                                                   \
    const Status& status__ = (X);                        \
    if (!status__.IsOk()) {                              \
      LOG_ERROR << (MSG) << ": " << status__.AsString(); \
    }                                                    \
  } while (false)

#define LOG_TRITONSERVER_ERROR(X, MSG)                                  \
  do {                                                                  \
    TRITONSERVER_Error* err__ = (X);                                    \
    if (err__ != nullptr) {                                             \
      LOG_ERROR << (MSG) << ": " << TRITONSERVER_ErrorCodeString(err__) \
                << " - " << TRITONSERVER_ErrorMessage(err__);           \
      TRITONSERVER_ErrorDelete(err__);                                  \
    }                                                                   \
  } while (false)

#define LOG_FLUSH triton::common::gLogger_.Flush()

}}  // namespace triton::common
