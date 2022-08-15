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
namespace internal {

template <typename Ptr>
class IntOrValue {
 public:
  static_assert(__has_feature(cxx_unrestricted_unions),
                "IntOrValue relies on the unrestricted unions feature");
  static_assert(sizeof(intptr_t) == sizeof(Ptr),
                "Internal error: Ptr must have the same size as intptr_t");
  IntOrValue() : IntOrValue(0) {}
  explicit IntOrValue(intptr_t value) : number_((value << 1) | 1) {
    assert(ptr() == nullptr);
  }
  template <typename... Args>
  explicit IntOrValue(absl::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...) {
    assert(ptr() != nullptr);
  }

  IntOrValue(IntOrValue&& that) : IntOrValue() { *this = std::move(that); }
  IntOrValue(const IntOrValue& that) : IntOrValue() { *this = that; }
  IntOrValue& operator=(IntOrValue&& that) {
    std::swap(number_, that.number_);
    return *this;
  }
  IntOrValue& operator=(const IntOrValue& that) {
    Clear();
    if (that.has_ptr()) {
      new (&value_) Ptr(that.value_);
    } else {
      number_ = that.number_;
    }
    return *this;
  }

  ~IntOrValue() { Clear(); }

  bool has_number() const { return number_ & 1; }
  bool has_ptr() const { return !has_number(); }

  absl::optional<intptr_t> number() const {
    return has_number() ? absl::make_optional(number_ >> 1) : absl::nullopt;
  }

  const Ptr* ptr() const {
    // Here we rely on `cxx_unrestricted_unions`: We query `number_` in all
    // cases, even if the populated value is `value_`.
    return has_ptr() ? &value_ : nullptr;
  }
  Ptr* ptr() { return has_ptr() ? &value_ : nullptr; }

  // TODO: Add `operator==` and `!=`.

 private:
  void Clear() {
    if (ptr() != nullptr) {
      value_.~Ref();
      number_ = 0;
    }
  }

  union {
    intptr_t number_;
    Ptr value_;
  };
};

}  // namespace internal

// Provides a variant type between an `intptr_t` (without its most significant
// bit) and a reference-counted, const pointer. Relies on the fact that an
// allocated pointer is aligned and therefore always has zero least significant
// bit.
//
// If `T` is a `const` type then instances are cheaply copyable and movable.
// Otherwise `T` is (cheaply) movable-only.
//
// Therefore, instances should be always passed by value.
//
// The class has a very small memory footprint - the same as a plain pointer.
template <typename T>
class IntOrPtr {
 public:
  IntOrPtr() : IntOrPtr(0) {}
  explicit IntOrPtr(intptr_t value) : value_(value) { assert(has_number()); }
  explicit IntOrPtr(Ref<T> ptr) : value_(absl::in_place, std::move(ptr)) {
    assert(has_ptr());
  }
  template <typename... Args>
  explicit IntOrPtr(absl::in_place_t, Args&&... args)
      : IntOrPtr(New<absl::remove_const_t<T>, Args...>(
            std::forward<Args>(args)...)) {}
  template <typename U = std::remove_const<T>,
            typename std::enable_if<!std::is_same<U, T>::value, int>::type = 0>
  IntOrPtr(IntOrPtr<U>&& unique) {
    auto* value = unique.value_.ptr();
    if (value == nullptr) {
      value_ = internal::IntOrValue<Ref<T>>(*unique.number());
    } else {
      value_ = internal::IntOrValue<Ref<T>>(absl::in_place, std::move(*value));
    }
  }

  IntOrPtr(IntOrPtr&& that) = default;
  IntOrPtr(const IntOrPtr& that) = default;
  IntOrPtr& operator=(IntOrPtr&& that) = default;
  IntOrPtr& operator=(const IntOrPtr& that) = default;

  bool has_number() const { return value_.has_number(); }
  bool has_ptr() const { return value_.has_ptr(); }
  absl::optional<intptr_t> number() const { return value_.number(); }

  T* ptr() {
    Ref<T>* value = value_.ptr();
    return (value == nullptr) ? nullptr : &**value;
  }

  absl::variant<intptr_t, std::reference_wrapper<T>> Variant() {
    using result = absl::variant<intptr_t, std::reference_wrapper<T>>;
    T* value = ptr();
    if (value == nullptr) {
      return result(absl::in_place_index<0>, *number());
    } else {
      return result(absl::in_place_index<1>, *value);
    }
  }

  // TODO: Add `operator==` and `!=`.

  internal::IntOrValue<Ref<T>> value_;

  friend class IntOrPtr<typename std::remove_const<T>::type>;
};

// TODO: Add a similar class that wraps a type-safe enum `EnumOrPtr<E, T>`.

}  // namespace refptr

#endif  // _INT_OR_PTR_H
