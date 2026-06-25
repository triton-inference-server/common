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
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace tc = triton::common;

namespace {

// A single log record as delivered to the callback.
struct CapturedRecord {
  tc::Logger::Level level;
  bool is_verbose;
  std::string file;
  int line;
  uint64_t timestamp_us;
  std::string message;
};

// Returns a callback that appends each record it receives to `records`.
tc::Logger::LogCallbackFn
MakeCapturingCallback(std::vector<CapturedRecord>& records)
{
  return [&records](
             tc::Logger::Level level, bool is_verbose, const char* file,
             int line, uint64_t ts, const char* msg) {
    records.push_back(
        {level, is_verbose, file ? file : "", line, ts, msg ? msg : ""});
  };
}

// Runs `fn` with std::cout redirected to `buf`, restores cout even if `fn`
// throws.
template <typename Fn>
void
WithCapturedStdout(std::ostringstream& buf, Fn&& fn)
{
  std::streambuf* prev = std::cout.rdbuf(buf.rdbuf());
  try {
    fn();
  }
  catch (...) {
    std::cout.rdbuf(prev);
    throw;
  }
  std::cout.rdbuf(prev);
}

// The logger is a process-global singleton, clear the callback after each test.
class LogCallbackTest : public ::testing::Test {
 protected:
  void TearDown() override
  {
    tc::gLogger_.SetLogCallback(tc::Logger::LogCallbackFn());
  }
};


TEST_F(LogCallbackTest, ReceivesCorrectRecordFields)
{
  std::vector<CapturedRecord> records;
  tc::gLogger_.SetLogCallback(MakeCapturingCallback(records));
  EXPECT_TRUE(tc::gLogger_.HasLogCallback());

  // ERROR, non-verbose: every structured field is forwarded correctly.
  const int emit_line = __LINE__ + 1;
  tc::LogMessage(__FILE__, emit_line, tc::Logger::Level::kERROR).stream()
      << "structured-callback-test";

  ASSERT_EQ(records.size(), 1u);
  {
    const auto& r = records[0];
    EXPECT_EQ(r.level, tc::Logger::Level::kERROR);
    EXPECT_FALSE(r.is_verbose);
    EXPECT_EQ(r.line, emit_line);
    // LogMessage trims the path to its basename.
    EXPECT_NE(r.file.find("logging_test"), std::string::npos);
    // The message reaches the callback raw (unescaped).
    EXPECT_EQ(r.message, "structured-callback-test");
  }

  // INFO with the verbose flag set is reported as verbose.
  tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO)
          .SetVerbose()
          .stream()
      << "verbose-callback-test";

  ASSERT_EQ(records.size(), 2u);
  {
    const auto& r = records[1];
    EXPECT_EQ(r.level, tc::Logger::Level::kINFO);
    EXPECT_TRUE(r.is_verbose);
  }
}

TEST_F(LogCallbackTest, CallbackAndDefaultSinkAreMutuallyExclusive)
{
  // Registered: the record goes only to the callback, never to std::cout.
  int count = 0;
  tc::gLogger_.SetLogCallback([&count](
                                  tc::Logger::Level, bool, const char*, int,
                                  uint64_t, const char*) { ++count; });

  std::ostringstream captured;
  WithCapturedStdout(captured, [&] {
    tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO).stream()
        << "callback-only";
  });

  EXPECT_EQ(count, 1);
  EXPECT_TRUE(captured.str().empty());  // default sink skipped

  // Cleared: records return to the default sink and the callback stops firing.
  tc::gLogger_.SetLogCallback(tc::Logger::LogCallbackFn());
  EXPECT_FALSE(tc::gLogger_.HasLogCallback());

  captured.str("");
  WithCapturedStdout(captured, [&] {
    tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO).stream()
        << "after-clear";
  });

  EXPECT_EQ(count, 1);  // callback not invoked after clear
  EXPECT_NE(captured.str().find("after-clear"), std::string::npos);
}

TEST_F(LogCallbackTest, ThrowingCallbackDoesNotEscapeDestructor)
{
  // A callback that throws must never propagate out of LogMessage's destructor.
  tc::gLogger_.SetLogCallback(
      [](tc::Logger::Level, bool, const char*, int, uint64_t, const char*) {
        throw std::runtime_error("callback failure");
      });

  EXPECT_NO_THROW({
    tc::LogMessage(__FILE__, __LINE__, tc::Logger::Level::kINFO).stream()
        << "boom";
  });
}

}  // namespace
