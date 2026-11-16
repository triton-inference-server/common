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
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "triton/common/triton_json.h"

namespace {

using triton::common::TritonJson;

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

// Parse 'json' into 'value' with a memory limit of 'max_bytes'. Return
// whether the limit stopped the parse, and set 'ok' to whether the parse
// succeeded.
bool
ParseWithLimit(
    TritonJson::Value* value, const std::string& json, const size_t max_bytes,
    bool* ok = nullptr)
{
  bool limit_exceeded = true;
  triton::common::Error err = value->ParseWithMemoryLimit(
      json.data(), json.size(), max_bytes, &limit_exceeded);
  if (ok != nullptr) {
    *ok = err.IsOk();
  }
  return limit_exceeded;
}

// The smallest limit under which 'json' parses. A parse is stopped exactly
// when the bytes it allocates in total go over the limit, so this is found
// by bisection.
size_t
SmallestLimit(const std::string& json)
{
  size_t too_small = 0, enough = size_t(1) << 30;
  {
    TritonJson::Value value;
    bool ok = false;
    EXPECT_FALSE(ParseWithLimit(&value, json, enough, &ok));
    EXPECT_TRUE(ok);
  }
  {
    TritonJson::Value value;
    EXPECT_TRUE(ParseWithLimit(&value, json, too_small));
  }
  while (enough - too_small > 1) {
    const size_t mid = too_small + (enough - too_small) / 2;
    TritonJson::Value value;
    (ParseWithLimit(&value, json, mid) ? too_small : enough) = mid;
  }
  return enough;
}

std::string
Compact(TritonJson::Value& value)
{
  TritonJson::WriteBuffer buffer;
  EXPECT_TRUE(value.Write(&buffer).IsOk());
  return buffer.Contents();
}

TEST(JsonParseLimitTest, UnderLimit)
{
  TritonJson::Value value;
  bool ok = false;
  EXPECT_FALSE(ParseWithLimit(&value, NumberArray(10000), 16 << 20, &ok));
  ASSERT_TRUE(ok);
  TritonJson::Value data;
  ASSERT_TRUE(value.Find("data", &data));
  EXPECT_EQ(data.ArraySize(), 10000u);
}

TEST(JsonParseLimitTest, OverLimitIsStopped)
{
  TritonJson::Value value;
  bool ok = true;
  EXPECT_TRUE(ParseWithLimit(&value, NumberArray(100000), 128 << 10, &ok));
  EXPECT_FALSE(ok);
  // The partly parsed document is freed and the value can be used again.
  EXPECT_FALSE(value.IsObject());
  EXPECT_EQ(Compact(value), "null");
  EXPECT_FALSE(ParseWithLimit(&value, "{\"x\":[1,2,3]}", 1 << 20, &ok));
  EXPECT_TRUE(ok);
  TritonJson::Value x;
  ASSERT_TRUE(value.Find("x", &x));
  EXPECT_EQ(x.ArraySize(), 3u);
}

TEST(JsonParseLimitTest, StoppedParseEmptiesDocument)
{
  TritonJson::Value value;
  ASSERT_TRUE(value.Parse("{\"old\":[1,2,3]}").IsOk());
  EXPECT_TRUE(ParseWithLimit(&value, NumberArray(100000), 128 << 10));
  EXPECT_FALSE(value.Find("old"));
}

TEST(JsonParseLimitTest, OverLimitErrorMessage)
{
  const std::string json = NumberArray(100000);
  TritonJson::Value value;
  bool limit_exceeded = false;
  triton::common::Error err = value.ParseWithMemoryLimit(
      json.data(), json.size(), 128 << 10, &limit_exceeded);
  EXPECT_TRUE(limit_exceeded);
  EXPECT_FALSE(err.IsOk());
  EXPECT_EQ(
      err.Message(),
      "failed to parse the request JSON buffer: it needs more than 131072 "
      "bytes of memory");
}

// 'limit_exceeded' may be nullptr, whether the parse is stopped or not.
TEST(JsonParseLimitTest, LimitExceededCanBeNull)
{
  const std::string json = NumberArray(100000);
  TritonJson::Value value;
  triton::common::Error err =
      value.ParseWithMemoryLimit(json.data(), json.size(), 128 << 10, nullptr);
  EXPECT_FALSE(err.IsOk());
  EXPECT_EQ(
      err.Message(),
      "failed to parse the request JSON buffer: it needs more than 131072 "
      "bytes of memory");
  EXPECT_EQ(Compact(value), "null");

  err = value.ParseWithMemoryLimit(json.data(), json.size(), 64 << 20, nullptr);
  ASSERT_TRUE(err.IsOk());
  TritonJson::Value data;
  ASSERT_TRUE(value.Find("data", &data));
  EXPECT_EQ(data.ArraySize(), 100000u);

  // Malformed JSON.
  err = value.ParseWithMemoryLimit("{", 1, 64 << 20, nullptr);
  EXPECT_FALSE(err.IsOk());
}

TEST(JsonParseLimitTest, LimitExceededIsAlwaysSet)
{
  const std::string json = NumberArray(100000);
  TritonJson::Value value;
  bool limit_exceeded = false;
  triton::common::Error err = value.ParseWithMemoryLimit(
      json.data(), json.size(), 128 << 10, &limit_exceeded);
  EXPECT_FALSE(err.IsOk());
  EXPECT_TRUE(limit_exceeded);

  err = value.ParseWithMemoryLimit(
      json.data(), json.size(), 64 << 20, &limit_exceeded);
  EXPECT_TRUE(err.IsOk());
  EXPECT_FALSE(limit_exceeded);

  limit_exceeded = true;
  err = value.ParseWithMemoryLimit("{", 1, 64 << 20, &limit_exceeded);
  EXPECT_FALSE(err.IsOk());
  EXPECT_FALSE(limit_exceeded);
}

// Malformed JSON gives the error that Parse() without a limit gives.
TEST(JsonParseLimitTest, MalformedJsonIsNotOverLimit)
{
  for (const std::string json :
       {"{\"a\":[1,2,", "{\"a\" 1}", "[1,]", "nul", "\"abc", "1 2"}) {
    TritonJson::Value plain;
    const triton::common::Error expected = plain.Parse(json);
    ASSERT_FALSE(expected.IsOk()) << json;

    TritonJson::Value value;
    bool limit_exceeded = true;
    const triton::common::Error err = value.ParseWithMemoryLimit(
        json.data(), json.size(), 1 << 20, &limit_exceeded);
    EXPECT_FALSE(limit_exceeded) << json;
    EXPECT_FALSE(err.IsOk()) << json;
    EXPECT_EQ(err.Message(), expected.Message()) << json;
  }
}

// Empty input needs no memory, so it gives the error that Parse() without a
// limit gives, even when the limit is 0.
TEST(JsonParseLimitTest, EmptyInputIsNotOverLimit)
{
  for (const std::string json : {"", "  "}) {
    TritonJson::Value plain;
    const triton::common::Error expected = plain.Parse(json);
    ASSERT_FALSE(expected.IsOk());

    TritonJson::Value value;
    bool limit_exceeded = true;
    const triton::common::Error err = value.ParseWithMemoryLimit(
        json.data(), json.size(), 0, &limit_exceeded);
    EXPECT_FALSE(limit_exceeded);
    EXPECT_FALSE(err.IsOk());
    EXPECT_EQ(err.Message(), expected.Message());
  }

  TritonJson::Value value;
  bool limit_exceeded = true;
  const triton::common::Error err =
      value.ParseWithMemoryLimit("", 0, 0, &limit_exceeded);
  EXPECT_FALSE(limit_exceeded);
  EXPECT_EQ(
      err.Message(),
      "failed to parse the request JSON buffer: The document is empty. at 0");
}

TEST(JsonParseLimitTest, ParseOfNonDocumentIsNotOverLimit)
{
  TritonJson::Value doc(TritonJson::ValueType::OBJECT);
  TritonJson::Value child(doc, TritonJson::ValueType::OBJECT);
  const triton::common::Error expected = child.Parse("{}", 2);
  ASSERT_FALSE(expected.IsOk());

  bool limit_exceeded = true;
  const triton::common::Error err =
      child.ParseWithMemoryLimit("{}", 2, 0, &limit_exceeded);
  EXPECT_FALSE(limit_exceeded);
  EXPECT_FALSE(err.IsOk());
  EXPECT_EQ(err.Message(), expected.Message());
  child.Release();
}

// The limit is on the most memory the parse uses: the elements of an array
// sit on a parse stack until the array ends and are then copied into the
// document, so the parse needs at least twice the memory that the numbers
// take in the document. A number takes 16 to 24 bytes, depending on the
// platform.
TEST(JsonParseLimitTest, CountsParseStacks)
{
  const size_t count = 100000;
  const size_t smallest = SmallestLimit(NumberArray(count));
  EXPECT_GE(smallest, 2 * 16 * count);
  EXPECT_LE(smallest, 3 * 24 * count + (256 << 10));
}

TEST(JsonParseLimitTest, SmallestDocuments)
{
  // Parse stacks only.
  EXPECT_LT(SmallestLimit("1"), 4096u);
  EXPECT_LT(SmallestLimit("{}"), 4096u);
  // Plus one 64 KiB memory pool chunk.
  const size_t small_request = SmallestLimit(
      "{\"inputs\":[{\"name\":\"INPUT0\",\"datatype\":\"FP32\",\"shape\":[1,1],"
      "\"data\":[1.0]}],\"outputs\":[{\"name\":\"OUTPUT0\"}]}");
  EXPECT_GT(small_request, 64u << 10);
  EXPECT_LT(small_request, 68u << 10);
}

// A limit of 0 allows no memory, it does not mean no limit.
TEST(JsonParseLimitTest, ZeroLimit)
{
  for (const std::string json : {"1", "{}", "[]", "\"a\"", "{\"x\":[1]}"}) {
    TritonJson::Value value;
    bool ok = true;
    EXPECT_TRUE(ParseWithLimit(&value, json, 0, &ok)) << json;
    EXPECT_FALSE(ok);
  }
}

// A limit below one memory pool chunk only fits documents that keep nothing
// in the pool.
TEST(JsonParseLimitTest, LimitBelowOnePoolChunk)
{
  const size_t limit = 60u << 10;
  for (const std::string json : {"1", "{}", "[]", "\"a\"", "null"}) {
    TritonJson::Value value;
    bool ok = false;
    EXPECT_FALSE(ParseWithLimit(&value, json, limit, &ok)) << json;
    EXPECT_TRUE(ok);
  }
  for (const std::string json :
       {"{\"x\":1}", "[1]", "\"a string that is kept in the memory pool\""}) {
    TritonJson::Value value;
    bool ok = true;
    EXPECT_TRUE(ParseWithLimit(&value, json, limit, &ok)) << json;
    EXPECT_FALSE(ok);
    EXPECT_FALSE(ParseWithLimit(&value, json, 128u << 10, &ok)) << json;
    EXPECT_TRUE(ok);
  }
}

TEST(JsonParseLimitTest, LargestLimit)
{
  TritonJson::Value value;
  bool ok = false;
  EXPECT_FALSE(ParseWithLimit(
      &value, NumberArray(100000), std::numeric_limits<size_t>::max(), &ok));
  EXPECT_TRUE(ok);
}

TEST(JsonParseLimitTest, PlainParseIsNotLimited)
{
  TritonJson::Value stopped;
  EXPECT_TRUE(ParseWithLimit(&stopped, NumberArray(100000), 16 << 10));
  TritonJson::Value value;
  EXPECT_TRUE(value.Parse(NumberArray(100000)).IsOk());
  EXPECT_TRUE(stopped.Parse(NumberArray(100000)).IsOk());
}

TEST(JsonParseLimitTest, DeepNestingAndLongStrings)
{
  const std::string nested =
      std::string(10000, '[') + "1" + std::string(10000, ']');
  const std::string long_string =
      "{\"s\":\"" + std::string(1024 * 1024, 'a') + "\"}";
  for (const std::string* json : {&nested, &long_string}) {
    TritonJson::Value stopped;
    EXPECT_TRUE(ParseWithLimit(&stopped, *json, 16 << 10));
    TritonJson::Value value;
    bool ok = false;
    EXPECT_FALSE(ParseWithLimit(&value, *json, 64 << 20, &ok));
    EXPECT_TRUE(ok);
  }
}

TEST(JsonParseLimitTest, StopAndSucceedRepeatedly)
{
  const std::string json = MixedDocument(2000);
  for (int i = 0; i < 200; ++i) {
    TritonJson::Value value;
    const bool stop = (i % 2) == 0;
    bool ok = stop;
    EXPECT_EQ(
        ParseWithLimit(&value, json, stop ? (16 << 10) : (64 << 20), &ok),
        stop);
    EXPECT_EQ(ok, !stop);
  }
}

// Stop the parse at many of the points where it allocates. It must fail
// cleanly each time and then succeed with enough memory.
TEST(JsonParseLimitTest, StopsCleanlyAnywhere)
{
  const std::vector<std::string> jsons = {
      MixedDocument(300), NumberArray(20000),
      std::string(2000, '[') + "1" + std::string(2000, ']'),
      "{\"s\":\"" + std::string(100000, 'a') + "\",\"t\":[\"" +
          std::string(300, 'b') + "\"]}",
      "{\"a\":{\"b\":{\"c\":[1,2,{\"d\":\"" + std::string(40, 'x') +
          "\"}]}},\"e\":[[],[[]],{}]}"};
  for (const std::string& json : jsons) {
    TritonJson::Value expected;
    ASSERT_TRUE(expected.Parse(json).IsOk());
    const size_t smallest = SmallestLimit(json);
    const size_t step = std::max<size_t>(1, smallest / 400);
    for (size_t limit = 0; limit < smallest; limit += step) {
      TritonJson::Value value;
      bool ok = true;
      EXPECT_TRUE(ParseWithLimit(&value, json, limit, &ok)) << limit;
      EXPECT_FALSE(ok);
    }
    TritonJson::Value value;
    EXPECT_TRUE(ParseWithLimit(&value, json, smallest - 1));
    bool ok = false;
    EXPECT_FALSE(ParseWithLimit(&value, json, smallest, &ok));
    ASSERT_TRUE(ok);
    EXPECT_EQ(Compact(value), Compact(expected));
  }
}

// The limit only applies during the parse: changing, moving or copying the
// document afterwards, on this or another thread, is not limited.
TEST(JsonParseLimitTest, ChangesAfterParseAreNotLimited)
{
  const std::string json = MixedDocument(100);
  const size_t smallest = SmallestLimit(json);
  // Member names are referenced, not copied, so they must outlive the
  // documents.
  std::vector<std::string> names;
  for (int i = 0; i < 1000; ++i) {
    names.push_back("k" + std::to_string(i));
  }
  const std::string text(1000, 'z');

  // The parse leaves nothing of its limit unused.
  TritonJson::Value value;
  bool ok = false;
  EXPECT_FALSE(ParseWithLimit(&value, json, smallest, &ok));
  ASSERT_TRUE(ok);
  for (int i = 0; i < 1000; ++i) {
    ASSERT_TRUE(value.AddString(names[i].c_str(), text).IsOk());
  }

  TritonJson::Value moved(std::move(value));
  TritonJson::Value items;
  ASSERT_TRUE(moved.Find("items", &items));
  for (int i = 0; i < 1000; ++i) {
    ASSERT_TRUE(items.AppendString(text).IsOk());
  }

  std::thread([&moved] {
    ASSERT_TRUE(moved.AddString("thread", std::string(100000, 'w')).IsOk());
  }).join();
  const size_t moved_size = Compact(moved).size();
  EXPECT_GT(moved_size, 2000000u);

  // Add() copies a document into another one.
  TritonJson::Value copy(TritonJson::ValueType::OBJECT);
  ASSERT_TRUE(copy.Add("copy", std::move(moved)).IsOk());
  EXPECT_GT(Compact(copy).size(), moved_size);
}

// Each parse has its own count, so documents that are alive on a thread do
// not use up the limit of a later parse on it.
TEST(JsonParseLimitTest, ParsesOnOneThreadHaveSeparateLimits)
{
  const std::string json = MixedDocument(1000);
  const size_t smallest = SmallestLimit(json);
  TritonJson::Value values[10];
  for (TritonJson::Value& value : values) {
    bool ok = false;
    EXPECT_FALSE(ParseWithLimit(&value, json, smallest, &ok));
    EXPECT_TRUE(ok);
    TritonJson::Value stopped;
    EXPECT_TRUE(ParseWithLimit(&stopped, json, smallest - 1));
    TritonJson::Value plain;
    EXPECT_TRUE(plain.Parse(json).IsOk());
    ASSERT_TRUE(values[0].AddString("text", std::string(100000, 'z')).IsOk());
  }
}

TEST(JsonParseLimitTest, ThreadsHaveSeparateLimits)
{
  const std::string json = MixedDocument(5000);
  const size_t smallest = SmallestLimit(json);
  std::atomic<bool> start{false};
  std::atomic<int> ok_count{0}, stopped_count{0}, plain_count{0}, wrong{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 9; ++t) {
    threads.emplace_back([&, t] {
      while (!start) {
        std::this_thread::yield();
      }
      for (int i = 0; i < 20; ++i) {
        TritonJson::Value value;
        if ((t % 3) == 2) {
          // Parses without a limit are not affected by the other threads.
          wrong += value.Parse(json).IsOk() ? 0 : 1;
          ++plain_count;
          continue;
        }
        const bool enough = (t % 3) == 0;
        bool ok = false;
        const bool stopped =
            ParseWithLimit(&value, json, enough ? smallest : smallest - 1, &ok);
        wrong += ((stopped == enough) || (ok != enough)) ? 1 : 0;
        ++(enough ? ok_count : stopped_count);
      }
    });
  }
  start = true;
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(wrong, 0);
  EXPECT_EQ(ok_count, 60);
  EXPECT_EQ(stopped_count, 60);
  EXPECT_EQ(plain_count, 60);
}

TEST(JsonParseLimitTest, ReparseIntoSameValue)
{
  const std::string json = MixedDocument(1000);
  const size_t smallest = SmallestLimit(json);
  TritonJson::Value value;
  bool ok = false;
  EXPECT_FALSE(ParseWithLimit(&value, json, smallest, &ok));
  EXPECT_TRUE(ok);
  // Memory the document already holds is not counted again.
  EXPECT_FALSE(ParseWithLimit(&value, json, smallest, &ok));
  EXPECT_TRUE(ok);
}

}  // namespace

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
