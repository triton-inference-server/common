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

#include "triton/common/error.h"

#define TRITONJSON_STATUSTYPE Error
#define TRITONJSON_STATUSRETURN(M) \
  return Error(Error::Code::INTERNAL, (M).c_str())
#define TRITONJSON_STATUSSUCCESS Error()

#include "gtest/gtest.h"
#include "triton/common/triton_json.h"

namespace {

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

}  // namespace

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
