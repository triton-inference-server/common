// Copyright 2023-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#define TRITONJSON_STATUSTYPE Error
#define TRITONJSON_STATUSRETURN(M) \
  return Error(Error::Code::INTERNAL, (M).c_str())
#define TRITONJSON_STATUSSUCCESS Error()

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "triton/common/triton_json.h"

namespace {

using triton::common::TritonJsonParseBudget;

TEST(JsonTypeCheck, TestIsBool)
{
  triton::common::TritonJson::Value value;
  value.Parse("{\"x\": true}");

  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsBool());

  value.Parse("{\"x\": false}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsBool());

  value.Parse("{\"x\": \"x\"}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsBool());
}

TEST(JsonTypeCheck, TestIsObject)
{
  triton::common::TritonJson::Value value;
  value.Parse("{\"x\": {}}");

  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsObject());

  value.Parse("{\"x\": 2}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsObject());
}

TEST(JsonTypeCheck, TestIsString)
{
  triton::common::TritonJson::Value value;
  value.Parse("{\"x\": \"123\"}");

  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsString());

  value.Parse("{\"x\": 2}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsString());
}

TEST(JsonTypeCheck, TestIsArray)
{
  triton::common::TritonJson::Value value;
  value.Parse("{\"x\": []}");

  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsArray());

  value.Parse("{\"x\": 2}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsArray());
}

TEST(JsonTypeCheck, TestIsNumber)
{
  triton::common::TritonJson::Value value;
  value.Parse("{\"x\": 2.0}");

  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsNumber());

  value.Parse("{\"x\": 2.001}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsNumber());

  value.Parse("{\"x\": 2}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsNumber());

  value.Parse("{\"x\": \"a\"}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsNumber());
}

TEST(JsonTypeCheck, TestIsInt)
{
  triton::common::TritonJson::Value value;
  value.Parse("{\"x\": 2.0}");

  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsInt());

  value.Parse("{\"x\": 2}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsInt());

  value.Parse("{\"x\": -2}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_TRUE(x.IsInt());

  value.Parse("{\"x\": \"a\"}");
  ASSERT_TRUE(value.Find("x", &x));
  ASSERT_FALSE(x.IsInt());
}

// {"data":[0,1,2,...]} with 'count' numbers.
std::string
NumberArray(const size_t count)
{
  std::string json = "{\"data\":[";
  for (size_t i = 0; i < count; ++i) {
    json += (i ? "," : "") + std::to_string(i % 1000);
  }
  return json + "]}";
}

// {"items":[{"name":"item0","v":[1.5,2,true,null]},...]} with 'count' items.
std::string
MixedDocument(const size_t count)
{
  std::string json = "{\"items\":[";
  for (size_t i = 0; i < count; ++i) {
    json += (i ? "," : "");
    json +=
        "{\"name\":\"item" + std::to_string(i) + "\",\"v\":[1.5,2,true,null]}";
  }
  return json + "]}";
}

class JsonParseBudgetTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    TritonJsonParseBudget::SetLimit(0);
    ASSERT_EQ(TritonJsonParseBudget::InUse(), 0u);
  }

  void TearDown() override { TritonJsonParseBudget::SetLimit(0); }

  // Parse 'json' with the budget and return why the budget stopped it.
  static TritonJsonParseBudget::Error ParseWithBudget(
      triton::common::TritonJson::Value* value, const std::string& json,
      bool* ok = nullptr)
  {
    TritonJsonParseBudget::Error budget_error;
    triton::common::Error err =
        value->ParseWithBudget(json.data(), json.size(), &budget_error);
    if (ok != nullptr) {
      *ok = err.IsOk();
    }
    return budget_error;
  }
};

TEST_F(JsonParseBudgetTest, CountedUntilDocumentIsDestroyed)
{
  const std::string json = NumberArray(10000);
  {
    triton::common::TritonJson::Value value;
    bool ok = false;
    ASSERT_EQ(
        ParseWithBudget(&value, json, &ok), TritonJsonParseBudget::Error::NONE);
    ASSERT_TRUE(ok);
    // At least one 16-byte value per number.
    EXPECT_GE(TritonJsonParseBudget::InUse(), 10000u * 16);

    triton::common::TritonJson::Value data;
    ASSERT_TRUE(value.Find("data", &data));
    EXPECT_EQ(data.ArraySize(), 10000u);
  }
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, ParseIsNotCounted)
{
  TritonJsonParseBudget::SetLimit(1);
  triton::common::TritonJson::Value value;
  ASSERT_TRUE(value.Parse(NumberArray(10000)).IsOk());
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, TooLargeForWholeBudget)
{
  // Enough for a small document (one 64 KB memory pool chunk), but not for
  // 100000 numbers.
  TritonJsonParseBudget::SetLimit(128 * 1024);
  triton::common::TritonJson::Value value;
  bool ok = true;
  EXPECT_EQ(
      ParseWithBudget(&value, NumberArray(100000), &ok),
      TritonJsonParseBudget::Error::TOO_LARGE);
  EXPECT_FALSE(ok);
  // The partly built document is freed right away.
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);

  // The same value can still be used.
  EXPECT_EQ(
      ParseWithBudget(&value, "{\"x\":[1,2,3]}", &ok),
      TritonJsonParseBudget::Error::NONE);
  EXPECT_TRUE(ok);
  triton::common::TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  EXPECT_EQ(x.ArraySize(), 3u);
}

TEST_F(JsonParseBudgetTest, BusyWhileOtherDocumentsUseBudget)
{
  auto held = std::make_unique<triton::common::TritonJson::Value>();
  ASSERT_EQ(
      ParseWithBudget(held.get(), MixedDocument(20000)),
      TritonJsonParseBudget::Error::NONE);
  const size_t held_bytes = TritonJsonParseBudget::InUse();

  // A small document fits in the budget on its own, but not next to the
  // held one.
  TritonJsonParseBudget::SetLimit(held_bytes + 1);
  const std::string small = "{\"x\":[1,2,3]}";
  triton::common::TritonJson::Value value;
  bool ok = true;
  EXPECT_EQ(
      ParseWithBudget(&value, small, &ok), TritonJsonParseBudget::Error::BUSY);
  EXPECT_FALSE(ok);
  EXPECT_EQ(TritonJsonParseBudget::InUse(), held_bytes);

  held.reset();
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
  EXPECT_EQ(
      ParseWithBudget(&value, small, &ok), TritonJsonParseBudget::Error::NONE);
  EXPECT_TRUE(ok);
}

TEST_F(JsonParseBudgetTest, ReasonIsOptional)
{
  const std::string json = NumberArray(100000);
  TritonJsonParseBudget::SetLimit(128 * 1024);
  {
    triton::common::TritonJson::Value value;
    EXPECT_FALSE(value.ParseWithBudget(json.data(), json.size()).IsOk());
  }
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);

  TritonJsonParseBudget::SetLimit(0);
  triton::common::TritonJson::Value value;
  EXPECT_TRUE(value.ParseWithBudget(json.data(), json.size()).IsOk());
  EXPECT_GT(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, MalformedJson)
{
  {
    triton::common::TritonJson::Value value;
    bool ok = true;
    EXPECT_EQ(
        ParseWithBudget(&value, "{\"a\":[1,2,", &ok),
        TritonJsonParseBudget::Error::NONE);
    EXPECT_FALSE(ok);
  }
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, ZeroLimitIsNoLimit)
{
  TritonJsonParseBudget::SetLimit(0);
  triton::common::TritonJson::Value value;
  EXPECT_EQ(
      ParseWithBudget(&value, NumberArray(100000)),
      TritonJsonParseBudget::Error::NONE);
  EXPECT_GT(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, StopAndSucceedRepeatedly)
{
  const std::string json = MixedDocument(2000);
  for (int i = 0; i < 200; ++i) {
    TritonJsonParseBudget::SetLimit((i % 2) ? 0 : 16 * 1024);
    triton::common::TritonJson::Value value;
    EXPECT_EQ(
        ParseWithBudget(&value, json),
        (i % 2) ? TritonJsonParseBudget::Error::NONE
                : TritonJsonParseBudget::Error::TOO_LARGE);
  }
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, DeepNestingAndLongStrings)
{
  const std::string nested =
      std::string(10000, '[') + "1" + std::string(10000, ']');
  const std::string long_string =
      "{\"s\":\"" + std::string(1024 * 1024, 'a') + "\"}";
  for (const std::string* json : {&nested, &long_string}) {
    TritonJsonParseBudget::SetLimit(16 * 1024);
    {
      triton::common::TritonJson::Value value;
      EXPECT_EQ(
          ParseWithBudget(&value, *json),
          TritonJsonParseBudget::Error::TOO_LARGE);
    }
    EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);

    TritonJsonParseBudget::SetLimit(0);
    {
      triton::common::TritonJson::Value value;
      bool ok = false;
      EXPECT_EQ(
          ParseWithBudget(&value, *json, &ok),
          TritonJsonParseBudget::Error::NONE);
      EXPECT_TRUE(ok);
    }
    EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
  }
}

TEST_F(JsonParseBudgetTest, FreedOnAnotherThread)
{
  auto value = std::make_unique<triton::common::TritonJson::Value>();
  ASSERT_EQ(
      ParseWithBudget(value.get(), MixedDocument(1000)),
      TritonJsonParseBudget::Error::NONE);
  EXPECT_GT(TritonJsonParseBudget::InUse(), 0u);
  std::thread([&value] { value.reset(); }).join();
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
}

TEST_F(JsonParseBudgetTest, ThreadsShareBudget)
{
  const std::string json = MixedDocument(5000);
  size_t one_document = 0;
  {
    triton::common::TritonJson::Value value;
    ASSERT_EQ(
        ParseWithBudget(&value, json), TritonJsonParseBudget::Error::NONE);
    one_document = TritonJsonParseBudget::InUse();
  }
  // Room for a few documents at a time, but not for one per thread.
  const size_t limit = 3 * one_document;
  TritonJsonParseBudget::SetLimit(limit);

  std::atomic<bool> done{false};
  std::atomic<size_t> max_in_use{0};
  std::thread monitor([&] {
    while (!done) {
      const size_t in_use = TritonJsonParseBudget::InUse();
      if (in_use > max_in_use) {
        max_in_use = in_use;
      }
    }
  });

  std::atomic<int> parsed{0}, busy{0}, other{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < 20; ++i) {
        triton::common::TritonJson::Value value;
        switch (ParseWithBudget(&value, json)) {
          case TritonJsonParseBudget::Error::NONE:
            ++parsed;
            break;
          case TritonJsonParseBudget::Error::BUSY:
            ++busy;
            break;
          default:
            ++other;
            break;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  done = true;
  monitor.join();

  EXPECT_GT(parsed, 0);
  EXPECT_EQ(other, 0);
  EXPECT_LE(max_in_use, limit);
  EXPECT_EQ(TritonJsonParseBudget::InUse(), 0u);
}

}  // namespace

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
