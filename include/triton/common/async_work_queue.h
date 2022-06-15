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
#pragma once

#include "error.h"
#include "thread_pool.h"

namespace triton { namespace common {
// Manager for asynchronous worker threads. Use to accelerate copies and
// other such operations by running them in parallel.
// Call Initialize to start the worker threads (once) and AddTask to tasks to
// the queue.

class AsyncWorkQueue {
 public:
  // Start 'worker_count' number of worker threads.
  static Error Initialize(size_t worker_count);

  // Get the number of worker threads.
  static size_t WorkerCount();

  // Add a 'task' to the queue. The function will take ownership of 'task'.
  // Therefore std::move should be used when calling AddTask.
  static Error AddTask(std::function<void(void)>&& task);

 protected:
  static void Reset();

 private:
  AsyncWorkQueue() = default;
  ~AsyncWorkQueue();
  static AsyncWorkQueue* GetSingleton();
  std::unique_ptr<ThreadPool> thread_pool_;
};

}}  // namespace triton::common
