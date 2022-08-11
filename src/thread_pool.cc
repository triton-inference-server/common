// Copyright 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "triton/common/thread_pool.h"
#include <stdexcept>

namespace triton { namespace common {

ThreadPool::ThreadPool(size_t thread_count)
{
  if (!thread_count) {
    throw std::invalid_argument("Thread count must be greater than zero.");
  }

  // Define infinite loop for each thread to wait for a task to complete
  const auto worker_loop = [this]() {
    while (true) {
      Task task;
      {
        std::unique_lock<std::mutex> lk(queue_mtx_);
        // Wake if there's a task to do, or the pool has been stopped.
        cv_.wait(lk, [&]() { return !task_queue_.empty() || stop_; });
        // Exit condition
        if (stop_ && task_queue_.empty()) {
          break;
        }
        task = std::move(task_queue_.front());
        task_queue_.pop();
      }

      // Execute task - ensure function has a valid target
      if (task) {
        task();
      }
    }
  };

  workers_.reserve(thread_count);
  for (size_t i = 0; i < thread_count; ++i) {
    workers_.emplace_back(worker_loop);
  }
}

ThreadPool::~ThreadPool()
{
  {
    std::lock_guard<std::mutex> lk(queue_mtx_);
    // Signal to each worker that it should exit loop when tasks are finished
    stop_ = true;
  }
  // Wake all threads to clean up
  cv_.notify_all();
  for (auto& t : workers_) {
    t.join();
  }
}

void
ThreadPool::Enqueue(Task&& task)
{
  {
    std::lock_guard<std::mutex> lk(queue_mtx_);
    // Don't accept more work if pool is shutting down
    if (stop_) {
      return;
    }
    task_queue_.push(std::move(task));
  }
  // Only wake one thread per task
  // Todo: DLIS-3859 if ThreadPool gets used more.
  cv_.notify_one();
}

}}  // namespace triton::common
