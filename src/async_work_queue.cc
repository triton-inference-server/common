// Copyright 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "triton/common/async_work_queue.h"

#include <iostream>

namespace triton { namespace common {

AsyncWorkQueue::~AsyncWorkQueue()
{
  GetSingleton()->thread_pool_.reset();
}

AsyncWorkQueue*
AsyncWorkQueue::GetSingleton()
{
  static AsyncWorkQueue singleton;
  // std::cout << "[DEBUG][common::AsyncWorkQueue] singleton address: "
  //           << &singleton << std::endl;
  return &singleton;
}

Error
AsyncWorkQueue::Initialize(size_t worker_count)
{
  if (worker_count < 1) {
    return Error(
        Error::Code::INVALID_ARG,
        "Async work queue must be initialized with positive 'worker_count'");
  }

  static std::mutex init_mtx;
  std::lock_guard<std::mutex> lk(init_mtx);

  if (GetSingleton()->thread_pool_) {
    return Error(
        Error::Code::ALREADY_EXISTS,
        "Async work queue has been initialized with " +
            std::to_string(GetSingleton()->thread_pool_->Size()) +
            " 'worker_count'");
  }

  GetSingleton()->thread_pool_.reset(new ThreadPool(worker_count));
  std::cout << "[DEBUG][common::AsyncWorkQueue] Successfully initialized "
               "AsyncWorkQueue thread pool with worker_count="
            << worker_count << std::endl;

  std::cout << "[DEBUG][common::AsyncWorkQueue] singleton address "
               "called from core: "
            << GetSingleton() << std::endl;

  std::cout << "[DEBUG][common::AsyncWorkQueue] singleton thread_pool address "
               "called from core: "
            << GetSingleton()->thread_pool_.get() << std::endl;
  return Error::Success;
}

size_t
AsyncWorkQueue::WorkerCount()
{
  std::cout << "[DEBUG][common::AsyncWorkQueue] singleton address "
               "called from WorkerCount(): "
            << GetSingleton() << std::endl;

  std::cout << "[DEBUG][common::AsyncWorkQueue] singleton thread_pool address "
               "called from backend: "
            << GetSingleton()->thread_pool_.get() << std::endl;

  if (!GetSingleton()->thread_pool_) {
    std::cout << "[DEBUG][common::AsyncWorkQueue] singleton thread_pool is "
                 "null, so return WorkerCount() = 0 (zero)"
              << std::endl;
    return 0;
  }

  std::cout << "[DEBUG][common::AsyncWorkQueue] singleton thread_pool is: "
            << GetSingleton()->thread_pool_->Size() << std::endl;
  return GetSingleton()->thread_pool_->Size();
}

Error
AsyncWorkQueue::AddTask(std::function<void(void)>&& task)
{
  if (!GetSingleton()->thread_pool_) {
    return Error(
        Error::Code::UNAVAILABLE,
        "Async work queue must be initialized before adding task");
  }
  GetSingleton()->thread_pool_->Enqueue(std::move(task));

  return Error::Success;
}

void
AsyncWorkQueue::Reset()
{
  // Reconstruct the singleton to reset it
  GetSingleton()->~AsyncWorkQueue();
  new (GetSingleton()) AsyncWorkQueue();
}

}}  // namespace triton::common
