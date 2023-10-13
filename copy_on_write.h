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
// until a mutable reference is requested by `as_mutable`.
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
// Instances should be always passed by value, not by reference.
template <typename T>
class CopyOnWriteNoDef {
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

  // See `CopyOnWrite<T>::as_mutable` below.
  T& as_mutable() {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copy-constructible");
    auto as_owned = std::move(ref_).AttemptToClaim();
    return assign(absl::holds_alternative<Ref<T>>(as_owned)
                      ? absl::get<Ref<T>>(std::move(as_owned))
                      : New<T>(*absl::get<Ref<const T>>(as_owned)));
  }

  // Modifies a copy of this instance with `mutator`, which receives `T&` as
  // its only argument. Returns the modified copy.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWriteNoDef with(F&& mutator) const& {
    return CopyOnWriteNoDef(*this).with(std::forward<F>(mutator));
  }
  // Consumes `*`this`` to modify it, making a copy of the pointed-to value if
  // necessary, and returns a pointer to the result.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWriteNoDef with(F&& mutator) && {
    std::forward<F>(mutator)(as_mutable());
    return std::move(*this);
  }

 protected:
  explicit CopyOnWriteNoDef(Ref<const T> ref) : ref_(std::move(ref)) {}

  T& assign(Ref<T> owned) {
    T& value = *owned;
    ref_ = std::move(owned).Share();
    return value;
  }

  Ref<const T> ref_;
};

// Manages an instance of `T` on the heap. Copying `CopyOnWriteNoDef<T>` is
// similarly cheap as copying a `shared_ptr`. Actual copying of `T` is deferred
// until a mutable reference is requested by `as_mutable`. Default creation of
// instances of `T` is deferred similarly.
//
// `T` must satisfy the following properties:
//
// 1. All requirements of `CopyOnWriteNoDef` above.
// 2. `T` must be default-constructible.
// 3. Two default-constructed instances of `T` must be semantically
//    indistinguishable from each other.
//
// Instances should be always passed by value, not by reference.
template <typename T>
class CopyOnWrite : protected CopyOnWriteNoDef<T> {
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
  // Once `as_mutable()` is called, this always returns `false`.
  bool LazyDefault() const { return CopyOnWriteNoDef<T>::ref_ == nullptr; }

  // If this instance is the sole owner of `T`, returns it.
  // Otherwise makes a new copy on the heap, points this object to it, and
  // returns the copy.
  //
  // Important: The returned reference is valid only until this pointer is
  // copied or destroyed. Therefore callers should not store the returned
  // reference and rather call `as_mutable()` repeatedly as needed.
  // The `with` functions below provide a safer alternative.
  T& as_mutable() {
    if (LazyDefault()) {
      return CopyOnWriteNoDef<T>::assign(New<T>());
    }
    return CopyOnWriteNoDef<T>::as_mutable();
  }

  // Modifies a copy of this instance with `mutator`, which receives `T&` as
  // its only argument. Returns the modified copy.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWrite with(F&& mutator) const& {
    return CopyOnWrite(*this).with(std::forward<F>(mutator));
  }
  // Consumes `*`this`` to modify it, making a copy of the pointed-to value if
  // necessary, and returns a pointer to the result.
  template <typename F>
  ABSL_MUST_USE_RESULT CopyOnWrite with(F&& mutator) && {
    std::forward<F>(mutator)(CopyOnWriteNoDef<T>::as_mutable());
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
