// Copyright 2020-2023 Google LLC
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

#ifndef _COPY_ON_WRITE_H
#define _COPY_ON_WRITE_H

#include <cstddef>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/utility/utility.h"
#include "ref.h"

namespace refptr {

// Manages an instance of `T` on the heap. Copying `CopyOnWriteNoDef<T>` is
// similarly cheap as copying a `shared_ptr`. Actual copying of `T` is deferred
// until a mutable reference is requested by `AsMutable`.
//
// `T` must satisfy the following properties:
//
// 1. Must be copy-constructible.
// 2. Allow multi-threaded access to `const T&` (assuming no thread accesses it
//    mutably).
// 3. Copies of `T` must be semantically indistinguishable from each other.
//    In particular programs should not depend on pointer (in)equality of
//    instances.
//
// `CopyOnWriteNoDef<T>` then satisfies the same properties, allowing easy
// nesting of data structures.
//
// Instances should be always passed by value, not by reference.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI CopyOnWriteNoDef {
 public:
  using element_type = T;

  template <typename... Arg>
  explicit CopyOnWriteNoDef(absl::in_place_t, Arg&&... args)
      : ref_(New<T>(std::forward<Arg>(args)...).Share()) {}
  explicit CopyOnWriteNoDef(std::nullptr_t) : ref_(nullptr) {}

  CopyOnWriteNoDef(const CopyOnWriteNoDef&) = default;
  CopyOnWriteNoDef(CopyOnWriteNoDef&&) = default;
  CopyOnWriteNoDef& operator=(const CopyOnWriteNoDef&) = default;
  CopyOnWriteNoDef& operator=(CopyOnWriteNoDef&&) = default;

  bool operator==(std::nullptr_t) const { return ref_ == nullptr; }
  bool operator!=(std::nullptr_t) const { return ref_ != nullptr; }

  const T& operator*() const { return *ref_; }
  const T* operator->() const { return &this->operator*(); }

  // See `CopyOnWrite<T>::AsMutable` below.
  T& AsMutable() {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copy-constructible");
    return Adopt(
        absl::visit(ExtractOrCopy(), std::move(ref_).AttemptToClaim()));
  }

  // Modifies a copy of this instance with `mutator`, which receives `T&` as
  // its only argument. Returns the modified copy.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWriteNoDef With(F&& mutator) const& {
    return CopyOnWriteNoDef(*this).With(std::forward<F>(mutator));
  }
  // Consumes `*this` to modify it, making a copy of the pointed-to value if
  // necessary, and returns a pointer to the result.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWriteNoDef With(F&& mutator) && {
    std::forward<F>(mutator)(AsMutable());
    return std::move(*this);
  }

 protected:
  struct ExtractOrCopy {
    Ref<T> operator()(Ref<T> owned) { return owned; }
    Ref<T> operator()(const Ref<const T>& copy) { return New<T>(*copy); }
  };

  explicit CopyOnWriteNoDef(Ref<const T> ref) : ref_(std::move(ref)) {}

  T& Adopt(Ref<T> owned) {
    T& value = *owned;
    ref_ = std::move(owned).Share();
    return value;
  }

  Ref<const T> ref_;
};

// Manages an instance of `T` on the heap. Copying `CopyOnWrite<T>` is
// similarly cheap as copying a `shared_ptr`. Actual copying of `T` is deferred
// until a mutable reference is requested by `AsMutable`.
//
// Default creation of instances of `T` is deferred similarly:
// Default-constructed `CopyOnWrite<T>` object references a shared, global,
// read-only instance of `T` until it is modified for the first time (this
// state is tracked by the `LazyDefault()` method.).
// This allows easy, direct nesting of data structures as in:
//
//     struct Foo {
//       CopyOnWrite<Foo> nested;
//     };
//
// Unlike `CopyOnWriteNoDef`, `CopyOnWrite` can never be null, and therefore
// lacks the corresponding constructor and comparison operators.
//
// `T` must satisfy the following properties:
//
// 1. All requirements of `CopyOnWriteNoDef` above.
// 2. `T` must be default-constructible.
// 3. Two default-constructed instances of `T` must be semantically
//    indistinguishable from each other.
//
// `CopyOnWrite<T>` then satisfies the same properties, allowing easy nesting
// of data structures.
//
// Instances should be always passed by value, not by reference.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI CopyOnWrite : protected CopyOnWriteNoDef<T> {
 public:
  using element_type = T;

  // Construct a lazily initialized instance that returns a shared,
  // default-constructed `const T&` instance until modified.
  CopyOnWrite() : CopyOnWriteNoDef<T>(nullptr) {}
  template <typename... Arg>
  explicit CopyOnWrite(absl::in_place_t, Arg&&... args)
      : CopyOnWriteNoDef<T>(absl::in_place, std::forward<Arg>(args)...) {}

  CopyOnWrite(const CopyOnWrite&) = default;
  CopyOnWrite(CopyOnWrite&&) = default;
  CopyOnWrite& operator=(const CopyOnWrite&) = default;
  CopyOnWrite& operator=(CopyOnWrite&&) = default;

  const T& operator*() const {
    return LazyDefault() ? SharedDefault() : CopyOnWriteNoDef<T>::operator*();
  }
  const T* operator->() const { return &this->operator*(); }

  // Return `true` iff this instance was default-constructed and unmodified
  // yet, in which case `operator*` returns a shared instance of `const& T`.
  // Once `AsMutable()` is called, this always returns `false`.
  bool LazyDefault() const { return CopyOnWriteNoDef<T>::ref_ == nullptr; }

  // If this instance is the sole owner of `T`, returns it.
  // Otherwise makes a new copy on the heap, points this object to it, and
  // returns the copy.
  //
  // Important: The returned reference is valid only until this pointer is
  // copied or destroyed. Therefore callers should not store the returned
  // reference and rather call `AsMutable()` repeatedly as needed.
  // The `With` functions below provide a safer alternative.
  T& AsMutable() {
    if (LazyDefault()) {
      return CopyOnWriteNoDef<T>::Adopt(New<T>());
    }
    return CopyOnWriteNoDef<T>::AsMutable();
  }

  // Modifies a copy of this instance with `mutator`, which receives `T&` as
  // its only argument. Returns the modified copy, while `*this` remains
  // unchanged.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWrite With(F&& mutator) const& {
    return CopyOnWrite(*this).With(std::forward<F>(mutator));
  }
  // Consumes `*this` to modify it, making a copy of the pointed-to value if
  // necessary, and returns a pointer with the modified result.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWrite With(F&& mutator) && {
    std::forward<F>(mutator)(CopyOnWriteNoDef<T>::AsMutable());
    return std::move(*this);
  }

 protected:
  static const T& SharedDefault() {
    static const T instance;
    return instance;
  }
};

}  // namespace refptr

#endif  // _COPY_ON_WRITE_H
