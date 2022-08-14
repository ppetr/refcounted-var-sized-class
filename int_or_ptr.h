// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _INT_OR_PTR_H
#define _INT_OR_PTR_H

#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "ref.h"

namespace refptr {

// Provides a variant type between an `intptr_t` (without its most significant
// bit) and a reference-counted, const pointer. Relies on the fact that an
// allocated pointer is aligned and therefore always has zero least significant
// bit.
//
// TODO: Extend this so that `const T` is copyable and plain `T` is move-only.
template <typename T>
class IntOrPtr {
 public:
  static_assert(__has_feature(cxx_unrestricted_unions),
                "IntOrPtr relies on the unrestricted unions feature");
  static_assert(
      sizeof(intptr_t) == sizeof(Ref<const T>),
      "Internal error: Ref<const T> must have the same size as intptr_t");
  IntOrPtr() : IntOrPtr(0) {}
  explicit IntOrPtr(intptr_t value) : number_((value << 1) | 1) {
    assert(has_number());
  }
  explicit IntOrPtr(Ref<const T> ref) : ref_(Ref<const T>(std::move(ref))) {
    assert(has_ref());
  }
  template <typename... Args>
  explicit IntOrPtr(absl::in_place_t, Args&&... args)
      : IntOrPtr(New<T, Args...>(std::forward<Args>(args)...).Share()) {}

  IntOrPtr(IntOrPtr&& that) : IntOrPtr() { *this = std::move(that); }
  IntOrPtr(const IntOrPtr& that) : IntOrPtr() { *this = that; }
  IntOrPtr& operator=(IntOrPtr&& that) {
    std::swap(number_, that.number_);
    return *this;
  }
  IntOrPtr& operator=(const IntOrPtr& that) {
    Clear();
    if (that.has_ref()) {
      new (&ref_) Ref<const T>(that.ref_);
    } else {
      number_ = that.number_;
    }
    return *this;
  }

  ~IntOrPtr() { Clear(); }

  bool has_number() const { return ref() == nullptr; }
  bool has_ref() const { return ref() != nullptr; }

  absl::optional<intptr_t> number() const {
    return has_number() ? absl::make_optional(number_ >> 1) : absl::nullopt;
  }
  const T* ref() const {
    // Here we rely on `cxx_unrestricted_unions`: We query `number_` in all
    // cases, even if the populated value is `ref_`.
    return (number_ & 1) ? nullptr : &*ref_;
  }

  absl::variant<intptr_t, std::reference_wrapper<const T>> Variant() const {
    using result = absl::variant<intptr_t, std::reference_wrapper<const T>>;
    const T* ptr = ref();
    if (ptr == nullptr) {
      return result(absl::in_place_index<0>, number_ >> 1);
    } else {
      return result(absl::in_place_index<1>, *ptr);
    }
  }

  // TODO: Add `operator==` and `!=`.

 private:
  void Clear() {
    if (has_ref()) {
      ref_.~Ref();
      number_ = 0;
    }
  }

  union {
    intptr_t number_;
    Ref<const T> ref_;
  };
};

}  // namespace refptr

#endif  // _INT_OR_PTR_H
