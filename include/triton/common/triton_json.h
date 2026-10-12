// Copyright 2020-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifdef _WIN32
// Remove GetObject definition from windows.h, which prevents calls to
// RapidJSON's GetObject.
// https://github.com/Tencent/rapidjson/issues/1448
#undef GetObject
#include <rapidjson/document.h>
#else
// Disable class-memaccess warning to facilitate compilation with gcc>7
// https://github.com/Tencent/rapidjson/issues/1700
#pragma GCC diagnostic push
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#include <rapidjson/document.h>
#pragma GCC diagnostic pop
#endif  // _WIN32

#include <rapidjson/allocators.h>  // CrtAllocator (default) for Writer instantiation
#include <rapidjson/encodings.h>  // UTF8 (default) for Writer instantiation
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

// This header can be used both within Triton server and externally
// (i.e. in source that interacts only via TRITONSERVER or
// TRITONBACKEND API). Status is handled differently in these cases so
// the following macros must be defined before including this
// header. As an example the defines are shown here as returned by the
// TRITONSERVER API.
//
//   #define TRITONJSON_STATUSTYPE TRITONSERVER_Error*
//   #define TRITONJSON_STATUSRETURN(M)
//        return TRITONSERVER_ErrorNew(TRITONSERVER_ERROR_INTERNAL, (M).c_str())
//   #define TRITONJSON_STATUSSUCCESS nullptr

namespace triton { namespace common {

template <typename StatusType>
class TritonJsonImpl;

//
// Memory budget for parsing JSON that comes from outside the process, such
// as HTTP request bodies. A parsed document can use many times more memory
// than its JSON text, so a limit on the size of the text does not limit the
// memory used to parse it. Documents parsed with
// TritonJson::Value::ParseWithBudget() count the memory they use against one
// budget shared by all threads, from the start of the parse until the
// document is destroyed. A parse that would take the total over the budget
// is stopped and returns an error instead. The memory of documents parsed or
// built in other ways is not counted. The budget belongs to the binary
// (executable or shared library) that includes this header, so a document
// must be destroyed in the binary that parsed it.
//
class TritonJsonParseBudget {
 public:
  // Why a parse was stopped by the budget.
  enum class Error {
    // The parse was not stopped by the budget.
    NONE,
    // Other documents were using part of the budget when the parse was
    // stopped. The same parse may succeed later, unless it needs more than
    // the whole budget.
    BUSY,
    // The document needs more memory than the whole budget.
    TOO_LARGE
  };

  // Set the budget in bytes. 0, the default, means no limit: the memory is
  // still counted but no parse is stopped. Every document uses at least one
  // 64 KiB memory pool chunk, so a smaller limit stops every parse.
  static void SetLimit(const size_t bytes)
  {
    LimitBytes().store(bytes, std::memory_order_relaxed);
  }

  // Return the budget in bytes, or 0 if there is no limit.
  static size_t Limit() { return LimitBytes().load(std::memory_order_relaxed); }

  // Return the number of bytes counted against the budget right now.
  static size_t InUse() { return InUseBytes().load(std::memory_order_relaxed); }

 private:
  template <typename StatusType>
  friend class TritonJsonImpl;

  // Thrown by Allocator to stop a parse that would go over the budget.
  class Exceeded : public std::bad_alloc {
   public:
    explicit Exceeded(const Error reason) : reason_(reason) {}
    const char* what() const noexcept override
    {
      return "JSON parse memory budget exceeded";
    }
    Error Reason() const { return reason_; }

   private:
    Error reason_;
  };

  // The ParseWithBudget() call in progress on a thread, if any.
  struct ParseState {
    bool counting{false};
    // Bytes counted for the call so far.
    size_t bytes{0};
  };

  // Count the memory allocated on this thread against the budget while it
  // exists. Used for the length of a ParseWithBudget() call.
  class CountingScope {
   public:
    CountingScope() : state_(ThreadState())
    {
      state_.counting = true;
      state_.bytes = 0;
    }
    ~CountingScope() { state_.counting = false; }

   private:
    ParseState& state_;
  };

  // RapidJSON base allocator of TritonJson documents. It provides the memory
  // pool chunks and the parse stacks. Blocks allocated on a thread while it
  // is in a ParseWithBudget() call are counted against the budget until they
  // are freed. Each block starts with a header that records its size and
  // whether it is counted, so it can be freed on any thread.
  class Allocator {
   public:
    static const bool kNeedFree = true;

    void* Malloc(size_t size)
    {
      if (size == 0) {
        return nullptr;
      }
      if (size > kMaxSize) {
        throw std::bad_alloc();
      }
      const bool counted = ThreadState().counting;
      if (counted) {
        Charge(size);
      }
      Header* header = static_cast<Header*>(std::malloc(sizeof(Header) + size));
      if (header == nullptr) {
        if (counted) {
          Uncharge(size);
        }
        throw std::bad_alloc();
      }
      header->size = size;
      header->counted = counted;
      return header + 1;
    }

    void* Realloc(
        void* original_ptr, size_t /* original_size */, size_t new_size)
    {
      if (original_ptr == nullptr) {
        return Malloc(new_size);
      }
      if (new_size == 0) {
        Free(original_ptr);
        return nullptr;
      }
      if (new_size > kMaxSize) {
        throw std::bad_alloc();
      }
      Header* header = static_cast<Header*>(original_ptr) - 1;
      const size_t old_size = header->size;
      const bool counted = header->counted;
      // Charge before reallocating, so that 'original_ptr' is still valid
      // if this throws.
      if (counted && (new_size > old_size)) {
        Charge(new_size - old_size);
      }
      Header* new_header =
          static_cast<Header*>(std::realloc(header, sizeof(Header) + new_size));
      if (new_header == nullptr) {
        if (counted && (new_size > old_size)) {
          Uncharge(new_size - old_size);
        }
        throw std::bad_alloc();
      }
      if (counted && (new_size < old_size)) {
        Uncharge(old_size - new_size);
      }
      new_header->size = new_size;
      return new_header + 1;
    }

    static void Free(void* ptr) noexcept
    {
      if (ptr == nullptr) {
        return;
      }
      Header* header = static_cast<Header*>(ptr) - 1;
      if (header->counted) {
        Uncharge(header->size);
      }
      std::free(header);
    }

   private:
    struct alignas(std::max_align_t) Header {
      size_t size;
      bool counted;
    };
    static constexpr size_t kMaxSize = SIZE_MAX - sizeof(Header);
  };

  // Count 'size' more bytes. If this thread is in a ParseWithBudget() call
  // and that would take the total over the budget, count nothing and throw
  // Exceeded instead.
  static void Charge(const size_t size)
  {
    ParseState& state = ThreadState();
    const size_t limit = Limit();
    std::atomic<size_t>& in_use = InUseBytes();
    if (!state.counting || (limit == 0)) {
      in_use.fetch_add(size, std::memory_order_relaxed);
    } else {
      size_t current = in_use.load(std::memory_order_relaxed);
      do {
        if ((size > limit) || (current > limit - size)) {
          throw Exceeded(
              ((size > limit) || (state.bytes > limit - size))
                  ? Error::TOO_LARGE
                  : Error::BUSY);
        }
      } while (!in_use.compare_exchange_weak(
          current, current + size, std::memory_order_relaxed));
    }
    if (state.counting) {
      state.bytes += size;
    }
  }

  static void Uncharge(const size_t size) noexcept
  {
    InUseBytes().fetch_sub(size, std::memory_order_relaxed);
    ParseState& state = ThreadState();
    if (state.counting) {
      state.bytes = (size < state.bytes) ? (state.bytes - size) : 0;
    }
  }

  static std::atomic<size_t>& LimitBytes()
  {
    static std::atomic<size_t> limit{0};
    return limit;
  }

  static std::atomic<size_t>& InUseBytes()
  {
    static std::atomic<size_t> in_use{0};
    return in_use;
  }

  static ParseState& ThreadState()
  {
    static thread_local ParseState state;
    return state;
  }
};

//
// A JSON parser/writer. Currently based on rapidjson but the intent
// is to provide an abstraction for JSON functions that make it easy
// to substitute a different JSON parser. Specifically for rapidjson
// the class is also designed to provide safe access and error
// reporting to avoid the cases where rapidjson would just abort the
// entire application (!).
//
template <typename StatusType>
class TritonJsonImpl {
 public:
  class Value;
  enum class ValueType {
    OBJECT = rapidjson::kObjectType,
    ARRAY = rapidjson::kArrayType,
  };

  //
  // Buffer used when writing JSON representation.
  //
  class WriteBuffer {
   public:
    // Get buffer base address.
    const char* Base() const { return buffer_.c_str(); }

    // Get a reference to the buffer itself. Useful to efficiently
    // move the contents out of the buffer.
    std::string& MutableContents() { return buffer_; }

    // Immutable contents.
    const std::string& Contents() const { return buffer_; }

    // Interface required by rapidjson::Writer
    typedef char Ch;
    void Put(char c) { buffer_.push_back(c); }
    void Clear() { buffer_.clear(); }
    void Flush() { return; }
    size_t Size() const { return buffer_.size(); }

   private:
    std::string buffer_;
  };

  //
  // Utility to serialize input string
  // as a JSON string value

  static std::string SerializeString(const std::string& input)
  {
    WriteBuffer writebuffer;
    const unsigned int writeFlags = rapidjson::kWriteNanAndInfFlag;
    // Provide default template arguments to pass writeFlags
    rapidjson::Writer<
        WriteBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>,
        rapidjson::CrtAllocator, writeFlags>
        writer(writebuffer);
    if (RAPIDJSON_UNLIKELY(!writer.String(input.c_str()))) {
      return "Error Serializing String";
    }
    return writebuffer.Contents();
  }

 private:
  // RapidJSON types of TritonJson documents. Their memory comes from
  // TritonJsonParseBudget's allocator, so that the memory of a document
  // parsed with Value::ParseWithBudget() can be counted against the budget.
  using RapidJsonAllocator =
      rapidjson::MemoryPoolAllocator<TritonJsonParseBudget::Allocator>;
  using RapidJsonDocument = rapidjson::GenericDocument<
      rapidjson::UTF8<>, RapidJsonAllocator, TritonJsonParseBudget::Allocator>;
  using RapidJsonValue = RapidJsonDocument::ValueType;

 public:
  //
  // Value representing the entire document or an element within a
  // document.
  //
  class Value {
   public:
    // Empty value. Will become a top-level Document value if
    // initialized by parsing or a non-top-level value if initialized
    // any other way.
    explicit Value() : value_(nullptr), allocator_(nullptr) {}

    // Construct a top-level JSON document.
    explicit Value(const ValueType type)
        : document_(static_cast<rapidjson::Type>(type)), value_(nullptr),
          allocator_(&document_.GetAllocator())
    {
    }

    // Construct a non-top-level JSON value in a 'document'.
    explicit Value(TritonJsonImpl::Value& document, const ValueType type)
    {
      allocator_ = &document.document_.GetAllocator();
      value_ = new (allocator_->Malloc(sizeof(RapidJsonValue)))
          RapidJsonValue(static_cast<rapidjson::Type>(type));
    }

    // Move constructor.
    explicit Value(Value&& other) { *this = std::move(other); }

    // Move assignment operator.
    Value& operator=(Value&& other)
    {
      document_ = std::move(other.document_);
      value_ = other.value_;
      allocator_ = other.allocator_;
      other.value_ = nullptr;
      other.allocator_ = nullptr;
      return *this;
    }

    // Parse JSON into document. Can only be called on top-level
    // document value, otherwise error is returned.
    StatusType Parse(const char* base, const size_t size)
    {
      return ParseImpl(base, size, false /* count */, nullptr);
    }

    // \see Parse(const char* base, const size_t size)
    StatusType Parse(const std::string& json)
    {
      return Parse(json.data(), json.size());
    }

    // Parse JSON into document like Parse(), and count the memory used by
    // the document against the TritonJsonParseBudget until the document is
    // destroyed. If the parse would go over the budget it is stopped, an
    // error is returned and 'budget_error', if not nullptr, is set to the
    // reason. The document is then left empty, so values taken from it
    // before must not be used. Memory the document already holds from
    // earlier content also stays counted until the document is destroyed,
    // so parse into a new Value.
    StatusType ParseWithBudget(
        const char* base, const size_t size,
        TritonJsonParseBudget::Error* budget_error = nullptr)
    {
      return ParseImpl(base, size, true /* count */, budget_error);
    }

    // Write JSON representation into a 'buffer' in a compact
    // format. Can only be called for a top-level document value,
    // otherwise error is returned.
    StatusType Write(WriteBuffer* buffer) const
    {
      if (value_ != nullptr) {
        TRITONJSON_STATUSRETURN(
            std::string("JSON writing only available for top-level document"));
      }
      const unsigned int writeFlags = rapidjson::kWriteNanAndInfFlag;
      // Provide default template arguments to pass writeFlags
      rapidjson::Writer<
          WriteBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>,
          rapidjson::CrtAllocator, writeFlags>
          writer(*buffer);
      if (!document_.Accept(writer)) {
        TRITONJSON_STATUSRETURN(
            std::string("Failed to accept document, invalid JSON."));
      }
      return TRITONJSON_STATUSSUCCESS;
    }

    // Write JSON representation into a 'buffer' in an easy-to-read
    // format. Can only be called for a top-level document value,
    // otherwise error is returned.
    StatusType PrettyWrite(WriteBuffer* buffer) const
    {
      if (value_ != nullptr) {
        TRITONJSON_STATUSRETURN(
            std::string("JSON writing only available for top-level document"));
      }

      // Can't pass writeFlags with latest release v1.1.0 of rapidjson-dev.
      // We would need to build rapidjson from source to capture latest fixes.
      // See this issue:
      // https://github.com/Tencent/rapidjson/issues/905#issuecomment-370981353
      // PrettyWrite is only used for displaying model configs currently, so
      // this should not be an issue.
      rapidjson::PrettyWriter<WriteBuffer> writer(*buffer);
      if (!document_.Accept(writer)) {
        TRITONJSON_STATUSRETURN(
            std::string("Failed to accept document, invalid JSON."));
      }
      return TRITONJSON_STATUSSUCCESS;
    }

    // Swap a value with another.
    StatusType Swap(TritonJsonImpl::Value& other)
    {
      RapidJsonValue& value = AsMutableValue();
      value.Swap(other.AsMutableValue());
      return TRITONJSON_STATUSSUCCESS;
    }

    // Set/overwrite a boolean in a value. This changes the
    // type of the value to boolean.
    StatusType SetBool(const bool value)
    {
      RapidJsonValue& v = AsMutableValue();
      v.SetBool(value);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Set/overwrite a signed integer in a value. This changes the
    // type of the value to signed int.
    StatusType SetInt(const int64_t value)
    {
      RapidJsonValue& v = AsMutableValue();
      v.SetInt64(value);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Set/overwrite an unsigned integer in a value. This changes the
    // type of the value to unsigned int.
    StatusType SetUInt(const uint64_t value)
    {
      RapidJsonValue& v = AsMutableValue();
      v.SetUint64(value);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Set/overwrite a double in a value. This changes the
    // type of the value to double.
    StatusType SetDouble(const double value)
    {
      RapidJsonValue& v = AsMutableValue();
      v.SetDouble(value);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Set/overwrite a string in a value. This changes the
    // type of the value to string
    StatusType SetString(const std::string& value)
    {
      RapidJsonValue& v = AsMutableValue();
      v.SetString(value.c_str(), value.length(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Set/overwrite a string member with provided name and value in this object
    StatusType SetStringObject(const char* name, const std::string& value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add/replace JSON member '") + name +
            "' to non-object");
      }
      auto itr = object.FindMember(name);
      if (itr == object.MemberEnd()) {
        AddString(name, value);
      } else {
        object.RemoveMember(itr);
        object.AddMember(
            RapidJsonValue(rapidjson::StringRef(name)).Move(),
            RapidJsonValue(value.c_str(), value.size(), *allocator_),
            *allocator_);
      }

      return TRITONJSON_STATUSSUCCESS;
    }

    // Add an array or object as a new member to this value. 'value'
    // is moved into this value and so on return 'value' should not be
    // used. It is assumed that 'name' can be used by reference, it is
    // the caller's responsibility to make sure the lifetime of 'name'
    // extends at least as long as the object.
    StatusType Add(const char* name, TritonJsonImpl::Value&& value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      if (value.value_ == nullptr) {
        RapidJsonValue v2;
        v2.CopyFrom(value.document_, *allocator_);
        object.AddMember(
            RapidJsonValue(rapidjson::StringRef(name)).Move(), v2.Move(),
            *allocator_);
      } else {
        object.AddMember(
            RapidJsonValue(rapidjson::StringRef(name)).Move(),
            value.value_->Move(), *allocator_);
      }
      value.Release();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a copy of a string as a new member to this value. It is
    // assumed that 'name' can be used by reference, it is the
    // caller's responsibility to make sure the lifetime of 'name'
    // extends at least as long as the object.
    StatusType AddString(const char* name, const std::string& value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          RapidJsonValue(value.c_str(), value.size(), *allocator_).Move(),
          *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a copy of a explicit-length string as a new member to this
    // value. It is assumed that 'name' can be used by reference, it
    // is the caller's responsibility to make sure the lifetime of
    // 'name' extends at least as long as the object.
    StatusType AddString(const char* name, const char* value, const size_t len)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          RapidJsonValue(value, len, *allocator_).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a reference to a string as a new member to this value. It
    // is assumed that 'name' and 'value' can be used by reference, it
    // is the caller's responsibility to make sure the lifetime of
    // 'name' and 'value' extend at least as long as the object.
    StatusType AddStringRef(const char* name, const char* value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          rapidjson::StringRef(value), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a reference to a explicit-length string as a new member to
    // this value. It is assumed that 'name' and 'value' can be used
    // by reference, it is the caller's responsibility to make sure
    // the lifetime of 'name' and 'value' extend at least as long as
    // the object.
    StatusType AddStringRef(
        const char* name, const char* value, const size_t len)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          rapidjson::StringRef(value, len), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a boolean new member to this value. It is assumed that
    // 'name' can be used by reference, it is the caller's
    // responsibility to make sure the lifetime of 'name' extends at
    // least as long as the object.
    StatusType AddBool(const char* name, const bool value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a signed integer as a new member to this value. It is
    // assumed that 'name' can be used by reference, it is the
    // caller's responsibility to make sure the lifetime of 'name'
    // extends at least as long as the object.
    StatusType AddInt(const char* name, const int64_t value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add an unsigned integer as a new member to this value. It is
    // assumed that 'name' can be used by reference, it is the
    // caller's responsibility to make sure the lifetime of 'name'
    // extends at least as long as the object.
    StatusType AddUInt(const char* name, const uint64_t value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Add a double as a new member to this value. It is assumed that
    // 'name' can be used by reference, it is the caller's
    // responsibility to make sure the lifetime of 'name' extends at
    // least as long as the object.
    StatusType AddDouble(const char* name, const double value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to add JSON member '") + name +
            "' to non-object");
      }
      object.AddMember(
          RapidJsonValue(rapidjson::StringRef(name)).Move(),
          RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append an array or object to this value, which must be an
    // array. 'value' is moved into this value and so on return
    // 'value' should not be used.
    StatusType Append(TritonJsonImpl::Value&& value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }
      if (value.value_ == nullptr) {
        RapidJsonValue v2;
        v2.CopyFrom(value.document_, *allocator_);
        array.PushBack(v2.Move(), *allocator_);
      } else {
        array.PushBack(value.value_->Move(), *allocator_);
      }

      value.Release();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a copy of a string to this value, which must be an
    // array.
    StatusType AppendString(const std::string& value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }
      array.PushBack(
          RapidJsonValue(value.c_str(), value.size(), *allocator_).Move(),
          *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a copy of an explicit-length string to this value, which
    // must be an array.
    StatusType AppendString(const char* value, const size_t len)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }
      array.PushBack(
          RapidJsonValue(value, len, *allocator_).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a reference to a string to this value, which must be an
    // array. It is assumed that 'value' can be used by reference, it
    // is the caller's responsibility to make sure the lifetime of
    // 'value' extends at least as long as the object.
    StatusType AppendStringRef(const char* value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }
      array.PushBack(rapidjson::StringRef(value), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a reference to a explicit-length string to this value,
    // which must be an array. It is assumed that 'value' can be used
    // by reference, it is the caller's responsibility to make sure
    // the lifetime of 'value' extends at least as long as the object.
    StatusType AppendStringRef(const char* value, const size_t len)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }
      array.PushBack(rapidjson::StringRef(value, len), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a boolean to this value, which must be an array.
    StatusType AppendBool(const bool value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }

      array.PushBack(RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a signed integer to this value, which must be an array.
    StatusType AppendInt(const int64_t value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }

      array.PushBack(RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append an unsigned integer to this value, which must be an
    // array.
    StatusType AppendUInt(const uint64_t value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }

      array.PushBack(RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Append a double to this value, which must be an array.
    StatusType AppendDouble(const double value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to append JSON member to non-array"));
      }

      array.PushBack(RapidJsonValue(value).Move(), *allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Remove member from this object
    StatusType Remove(const char* name)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to remove JSON member '") + name +
            "' to non-object");
      }
      auto itr = object.FindMember(name);
      if (itr != object.MemberEnd()) {
        object.RemoveMember(itr);
      }  // else report success

      return TRITONJSON_STATUSSUCCESS;
    }

    // Check if this value is of the specified type. Return appropriate
    // error if not.
    StatusType AssertType(TritonJsonImpl::ValueType type) const
    {
      if (static_cast<rapidjson::Type>(type) != AsValue().GetType()) {
        TRITONJSON_STATUSRETURN(std::string("unexpected type"));
      }
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get the size of an array. If called on non-array returns zero.
    size_t ArraySize() const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray()) {
        return 0;
      }
      return array.GetArray().Size();
    }

    // Return the specified index contained in this array.
    StatusType At(const size_t idx, TritonJsonImpl::Value* value = nullptr)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      *value = TritonJsonImpl::Value(array[idx], allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get the names of all members in an object.  Error if value is
    // not an object.
    StatusType Members(std::vector<std::string>* names) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to get members for non-object"));
      }
      for (const auto& m : object.GetObject()) {
        names->push_back(m.name.GetString());
      }
      return TRITONJSON_STATUSSUCCESS;
    }

    // Return true if this value is an object and the named member is
    // contained in this object.
    bool Find(const char* name) const
    {
      const RapidJsonValue& object = AsValue();
      return object.IsObject() && object.HasMember(name);
    }

    // Return true if this value is an object and the named member is
    // contained in this object. Return the member in 'value'.
    bool Find(const char* name, TritonJsonImpl::Value* value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (object.IsObject() && object.HasMember(name)) {
        if (value != nullptr) {
          *value = TritonJsonImpl::Value(object[name], allocator_);
        }
        return true;
      }

      return false;
    }

    // Whether the object is null value. Note that false will also be returned
    // if the object is not a JSON value.
    bool IsNull() const { return ((value_ != nullptr) && value_->IsNull()); }

    // Return true if the object is an object and it has no members;
    // false otherwise.
    bool IsEmpty() const
    {
      const RapidJsonValue& object = AsValue();
      if (object.IsObject() && object.MemberCount() == 0) {
        return true;
      }
      return false;
    }

    // Get value as a string. The string may contain null or other
    // special characters and so 'len' must be used to determine length.
    // Error if value is not a string.
    StatusType AsString(const char** value, size_t* len) const
    {
      if ((value_ == nullptr) || !value_->IsString()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-string as string"));
      }
      *value = value_->GetString();
      *len = value_->GetStringLength();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get value as a string. The string may contain null or other
    // special characters.  Error if value is not a string.
    StatusType AsString(std::string* str) const
    {
      if ((value_ == nullptr) || !value_->IsString()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-string as string"));
      }
      str->assign(value_->GetString(), value_->GetStringLength());
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get value as a boolean. Error if value is not a boolean.
    StatusType AsBool(bool* value) const
    {
      if ((value_ == nullptr) || !value_->IsBool()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-boolean as boolean"));
      }
      *value = value_->GetBool();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get value as a signed integer. Error if value is not a signed
    // integer.
    StatusType AsInt(int64_t* value) const
    {
      if ((value_ == nullptr) || !value_->IsInt64()) {
        TRITONJSON_STATUSRETURN(std::string(
            "attempt to access JSON non-signed-integer as signed-integer"));
      }
      *value = value_->GetInt64();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get value as an unsigned integer. Error if value is not an
    // unsigned integer.
    StatusType AsUInt(uint64_t* value) const
    {
      if ((value_ == nullptr) || !value_->IsUint64()) {
        TRITONJSON_STATUSRETURN(std::string(
            "attempt to access JSON non-unsigned-integer as unsigned-integer"));
      }
      *value = value_->GetUint64();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get value as a double. Error if value is not a double.
    StatusType AsDouble(double* value) const
    {
      if ((value_ == nullptr) || !value_->IsNumber()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-number as double"));
      }
      *value = value_->GetDouble();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get named array member contained in this object.
    StatusType MemberAsArray(const char* name, TritonJsonImpl::Value* value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      auto& v = object[name];
      if (!v.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-array as array"));
      }
      *value = TritonJsonImpl::Value(v, allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get named object member contained in this object.
    StatusType MemberAsObject(const char* name, TritonJsonImpl::Value* value)
    {
      RapidJsonValue& object = AsMutableValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      auto& v = object[name];
      if (!v.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-object as object"));
      }
      *value = TritonJsonImpl::Value(v, allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object member as a string. The string may contain null or other
    // special characters and so 'len' must be used to determine length.
    // Error if this is not an object or if the member is not a string.
    StatusType MemberAsString(
        const char* name, const char** value, size_t* len) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      const auto& v = object[name];
      if (!v.IsString()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-string as string"));
      }
      *value = v.GetString();
      *len = v.GetStringLength();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object member as a string. The string may contain null or
    // other special characters.  Error if this is not an object or if
    // the member is not a string.
    StatusType MemberAsString(const char* name, std::string* str) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      const auto& v = object[name];
      if (!v.IsString()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-string as string"));
      }
      str->assign(v.GetString(), v.GetStringLength());
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object member as a boolean.  Error if this is not an object
    // or if the member is not a boolean.
    StatusType MemberAsBool(const char* name, bool* value) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      const auto& v = object[name];
      if (!v.IsBool()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-boolean as boolean"));
      }
      *value = v.GetBool();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object member as a signed integer.  Error if this is not an object
    // or if the member is not a signed integer.
    StatusType MemberAsInt(const char* name, int64_t* value) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      const auto& v = object[name];
      if (!v.IsInt64()) {
        TRITONJSON_STATUSRETURN(std::string(
            "attempt to access JSON non-signed-integer as signed-integer"));
      }
      *value = v.GetInt64();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object member as an unsigned integer.  Error if this is not an object
    // or if the member is not an unsigned integer.
    StatusType MemberAsUInt(const char* name, uint64_t* value) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      const auto& v = object[name];
      if (!v.IsUint64()) {
        TRITONJSON_STATUSRETURN(std::string(
            "attempt to access JSON non-unsigned-integer as unsigned-integer"));
      }
      *value = v.GetUint64();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object member as a double.  Error if this is not an object
    // or if the member is not a double.
    StatusType MemberAsDouble(const char* name, double* value) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing object member '") +
            name + "'");
      }
      const auto& v = object[name];
      if (!v.IsNumber()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-number as double"));
      }
      *value = v.GetDouble();
      return TRITONJSON_STATUSSUCCESS;
    }

    bool MemberIsArray(const char* name) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        return false;
      }
      const auto& v = object[name];
      return v.IsArray();
    }

    bool MemberIsArray(const std::string& name) const
    {
      return MemberIsArray(name.c_str());
    }

    bool MemberIsNull(const char* name) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        return false;
      }
      const auto& v = object[name];
      return v.IsNull();
    }

    bool MemberIsNull(const std::string& name) const
    {
      return MemberIsNull(name.c_str());
    }

    bool MemberIsObject(const char* name) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name))
        return false;
      const auto& v = object[name];
      return v.IsObject();
    }

    bool MemberIsObject(const std::string& name) const
    {
      return MemberIsObject(name.c_str());
    }

    bool MemberIsString(const char* name) const
    {
      const RapidJsonValue& object = AsValue();
      if (!object.IsObject() || !object.HasMember(name)) {
        return false;
      }
      const auto& v = object[name];
      return v.IsString();
    }

    bool MemberIsString(const std::string& name) const
    {
      return MemberIsString(name.c_str());
    }

    bool IsString()
    {
      const RapidJsonValue& object = AsValue();
      return object.IsString();
    }

    bool IsArray()
    {
      const RapidJsonValue& object = AsValue();
      return object.IsArray();
    }

    bool IsObject()
    {
      const RapidJsonValue& object = AsValue();
      return object.IsObject();
    }

    bool IsInt()
    {
      const RapidJsonValue& object = AsValue();
      return object.IsInt64();
    }

    bool IsBool()
    {
      const RapidJsonValue& object = AsValue();
      return object.IsBool();
    }

    bool IsNumber()
    {
      const RapidJsonValue& object = AsValue();
      return object.IsNumber();
    }

    // Get array element at a given index within this array.
    StatusType IndexAsArray(const size_t idx, TritonJsonImpl::Value* value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      auto& v = array[idx];
      if (!v.IsArray()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-array as array"));
      }
      *value = TritonJsonImpl::Value(v, allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get object element at a given index within this array.
    StatusType IndexAsObject(const size_t idx, TritonJsonImpl::Value* value)
    {
      RapidJsonValue& array = AsMutableValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      auto& v = array[idx];
      if (!v.IsObject()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-object as object"));
      }
      *value = TritonJsonImpl::Value(v, allocator_);
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get array index as a string. The string may contain null or
    // other special characters and so 'len' must be used to determine
    // length.  Error if this is not an array or if the index element
    // is not a string.
    StatusType IndexAsString(
        const size_t idx, const char** value, size_t* len) const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      const auto& v = array[idx];
      if (!v.IsString()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-string as string"));
      }
      *value = v.GetString();
      *len = v.GetStringLength();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get array index as a string. The string may contain null or
    // other special characters.  Error if this is not an array or if
    // the index element is not a string.
    StatusType IndexAsString(const size_t idx, std::string* str) const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      const auto& v = array[idx];
      if (!v.IsString()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-string as string"));
      }
      str->assign(v.GetString(), v.GetStringLength());
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get array index as a boolean.  Error if this is not an array or
    // if the index element is not a boolean.
    StatusType IndexAsBool(const size_t idx, bool* value) const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      const auto& v = array[idx];
      if (!v.IsBool()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-boolean as boolean"));
      }
      *value = v.GetBool();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get array index as a signed integer.  Error if this is not an array or
    // if the index element is not a signed integer.
    StatusType IndexAsInt(const size_t idx, int64_t* value) const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      const auto& v = array[idx];
      if (!v.IsInt64()) {
        TRITONJSON_STATUSRETURN(std::string(
            "attempt to access JSON non-signed-integer as signed-integer"));
      }
      *value = v.GetInt64();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get array index as an unsigned integer.  Error if this is not an array or
    // if the index element is not an unsigned integer.
    StatusType IndexAsUInt(const size_t idx, uint64_t* value) const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      const auto& v = array[idx];
      if (!v.IsUint64()) {
        TRITONJSON_STATUSRETURN(std::string(
            "attempt to access JSON non-unsigned-integer as unsigned-integer"));
      }
      *value = v.GetUint64();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Get array index as a double.  Error if this is not an array or
    // if the index element is not a double.
    StatusType IndexAsDouble(const size_t idx, double* value) const
    {
      const RapidJsonValue& array = AsValue();
      if (!array.IsArray() || (idx >= array.GetArray().Size())) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access non-existing array index '") +
            std::to_string(idx) + "'");
      }
      const auto& v = array[idx];
      if (!v.IsNumber()) {
        TRITONJSON_STATUSRETURN(
            std::string("attempt to access JSON non-number as double"));
      }
      *value = v.GetDouble();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Release/clear a value.
    void Release()
    {
      if (value_ != nullptr) {
        allocator_->Free(value_);
      }
    }

   private:
    // Construct a non-top-level JSON value that references an
    // existing element in a document.
    explicit Value(RapidJsonValue& v, RapidJsonAllocator* allocator)
        : value_(&v), allocator_(allocator)
    {
    }

    // Parse JSON into document. If 'count' is true the memory used is
    // counted against the TritonJsonParseBudget.
    StatusType ParseImpl(
        const char* base, const size_t size, const bool count,
        TritonJsonParseBudget::Error* budget_error)
    {
      if (budget_error != nullptr) {
        *budget_error = TritonJsonParseBudget::Error::NONE;
      }
      if (value_ != nullptr) {
        TRITONJSON_STATUSRETURN(
            std::string("JSON parsing only available for top-level document"));
      }
      // Use iterative parsing so deeply nested JSON does not consume call
      // stack.
      const unsigned int parseFlags =
          rapidjson::kParseNanAndInfFlag | rapidjson::kParseIterativeFlag;
      std::string error;
      try {
        if (count) {
          TritonJsonParseBudget::CountingScope counting;
          document_.Parse<parseFlags>(base, size);
        } else {
          document_.Parse<parseFlags>(base, size);
        }
      }
      catch (const TritonJsonParseBudget::Exceeded& e) {
        if (budget_error != nullptr) {
          *budget_error = e.Reason();
        }
        const std::string limit =
            std::to_string(TritonJsonParseBudget::Limit());
        error = (e.Reason() == TritonJsonParseBudget::Error::BUSY)
                    ? "not enough of the parse memory budget of " + limit +
                          " bytes is free"
                    : "the JSON needs more memory than the parse memory "
                      "budget of " +
                          limit + " bytes";
      }
      catch (const std::bad_alloc&) {
        error = "out of memory";
      }
      if (!error.empty()) {
        // Free the partly built document, and with it the memory counted
        // against the budget. Without counting, keep it like a parse error
        // does, so values taken from the document stay valid. The counting
        // scope has ended, so creating the new document is not counted. If
        // even that runs out of memory, the old document stays until the
        // Value is destroyed.
        if (count) {
          try {
            document_ = RapidJsonDocument();
          }
          catch (const std::bad_alloc&) {
          }
          allocator_ = &document_.GetAllocator();
        }
        TRITONJSON_STATUSRETURN(
            std::string("failed to parse the request JSON buffer: ") + error);
      }
      if (document_.HasParseError()) {
        TRITONJSON_STATUSRETURN(std::string(
            "failed to parse the request JSON buffer: " +
            std::string(GetParseError_En(document_.GetParseError())) + " at " +
            std::to_string(document_.GetErrorOffset())));
      }
      allocator_ = &document_.GetAllocator();
      return TRITONJSON_STATUSSUCCESS;
    }

    // Return a value object that can be used for both a top-level
    // document as well as an element within a document.
    const RapidJsonValue& AsValue() const
    {
      if (value_ == nullptr) {
        return document_;
      }
      return *value_;
    }

    RapidJsonValue& AsMutableValue()
    {
      if (value_ == nullptr) {
        return document_;
      }
      return *value_;
    }

    // If this object a document or value. Based on this only one or
    // document_ or value_ is valid.
    RapidJsonDocument document_;
    RapidJsonValue* value_;
    RapidJsonAllocator* allocator_;
  };
};

using TritonJson = TritonJsonImpl<TRITONJSON_STATUSTYPE>;

}}  // namespace triton::common
