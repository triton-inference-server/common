// Copyright 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace tc = triton::common;

namespace {

struct CapturedRecord {
  tc::Logger::Level level;
  bool is_verbose;
  std::string file;
  int line;
  uint64_t timestamp_us;
  std::string message;
};

// gLogger_ is a process-global singleton; clear the callback after each test
// so registrations cannot leak between tests.
class LogCallbackTest : public ::testing::Test {
 protected:
  void TearDown() override
  {
    tc::gLogger_.SetLogCallback(tc::Logger::LogCallbackFn());
  }
};

TEST_F(LogCallbackTest, ReceivesStructuredRecord)
{
  std::vector<CapturedRecord> records;
  tc::gLogger_.SetLogCallback([&records](
                                  tc::Logger::Level level, bool is_verbose,
                                  const char* file, int line, uint64_t ts,
                                  const char* msg) {
    records.push_back(
        {level, is_verbose, file ? file : "", line, ts, msg ? msg : ""});
  });
  EXPECT_TRUE(tc::gLogger_.HasLogCallback());

  const int emit_line = __LINE__ + 1;
  tc::LogMessage(__FILE__, emit_line, tc::Logger::Level::kERROR).stream()
      << "structured-callback-test";

  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].level, tc::Logger::Level::kERROR);
  EXPECT_FALSE(records[0].is_verbose);  // not a verbose record
  EXPECT_EQ(records[0].line, emit_line);
  // LogMessage reduces the path to its basename.
  EXPECT_NE(records[0].file.find("logging_test"), std::string::npos);
  // The callback receives the RAW (unescaped) message.
  EXPECT_EQ(records[0].message, "structured-callback-test");
#ifndef _WIN32
  EXPECT_GT(records[0].timestamp_us, 0u);
#endif
}

TEST_F(LogCallbackTest, ReportsVerboseFlag)
{
  // LOG_VERBOSE records are emitted at the INFO 'level' but marked verbose via
  // SetVerbose (exactly what the LOG_VERBOSE macros do). The callback must
  // observe the flag so the host can report VERBOSE distinctly from INFO.
  std::vector<CapturedRecord> records;
  tc::gLogger_.SetLogCallback([&records](
                                  tc::Logger::Level level, bool is_verbose,
                                  const char* file, int line, uint64_t ts,
                                  const char* msg) {
    records.push_back(
        {level, is_verbose, file ? file : "", line, ts, msg ? msg : ""});
  });

  tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO)
          .SetVerbose()
          .stream()
      << "verbose-callback-test";

  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].level, tc::Logger::Level::kINFO);
  EXPECT_TRUE(records[0].is_verbose);
}

TEST_F(LogCallbackTest, RegisteredCallbackBypassesDefaultSink)
{
  // INFO records go to std::cout when no log file is configured; capture it.
  // While a callback is registered, the record must NOT also hit the default
  // sink (that is the whole point — one stream, not two).
  std::ostringstream captured;
  std::streambuf* prev = std::cout.rdbuf(captured.rdbuf());

  int count = 0;
  tc::gLogger_.SetLogCallback([&count](
                                  tc::Logger::Level, bool, const char*, int,
                                  uint64_t, const char*) { ++count; });
  tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO).stream()
      << "callback-only";

  std::cout.rdbuf(prev);
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(captured.str().empty());  // default sink skipped
}

TEST_F(LogCallbackTest, ClearRestoresDefaultSink)
{
  int count = 0;
  tc::gLogger_.SetLogCallback([&count](
                                  tc::Logger::Level, bool, const char*, int,
                                  uint64_t, const char*) { ++count; });
  tc::gLogger_.SetLogCallback(tc::Logger::LogCallbackFn());  // clear
  EXPECT_FALSE(tc::gLogger_.HasLogCallback());

  // After clearing, the record goes back to the default sink (std::cout for
  // INFO) and the (now-unset) callback is not invoked.
  std::ostringstream captured;
  std::streambuf* prev = std::cout.rdbuf(captured.rdbuf());
  tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO).stream()
      << "after-clear";
  std::cout.rdbuf(prev);

  EXPECT_EQ(count, 0);
  EXPECT_NE(captured.str().find("after-clear"), std::string::npos);
}

}  // namespace
