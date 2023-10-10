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

#ifndef _COPY_ON_WRITE_H
#define _COPY_ON_WRITE_H

#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/utility/utility.h"
#include "ref.h"

namespace refptr {

// Manages an instance of `T` on the heap. Copying `CopyOnWrite<T>` is as
// comparably cheap to copying a `shared_ptr`. The actual copying of `T` is
// deferred until a mutable reference is requested by `as_mutable`.
//
// Instances should be always passed by value, not by reference.
template <typename T>
class CopyOnWrite {
 public:
  using element_type = T;

  template <typename... Arg>
  explicit CopyOnWrite(absl::in_place_t, Arg&&... args)
      : ref_(New<T>(std::forward<Arg>(args)...).Share()) {}
  // TODO: Replace with a specialization of `absl::optional<CopyOnWrite>`.
  explicit CopyOnWrite(nullptr_t) : ref_(nullptr) {}

  CopyOnWrite(const CopyOnWrite&) = default;
  CopyOnWrite(CopyOnWrite&&) = default;
  CopyOnWrite& operator=(const CopyOnWrite&) = default;
  CopyOnWrite& operator=(CopyOnWrite&&) = default;

  // TODO: Replace with a specialization of `absl::optional<CopyOnWrite>`.
  bool operator==(nullptr_t) const { return ref_ == nullptr; }
  bool operator!=(nullptr_t) const { return ref_ != nullptr; }

  const T& operator*() const { return *ref_; }
  const T* operator->() const { return &this->operator*(); }

  // If this instance is the sole owner of `T`, returns it.
  // Otherwise makes a new copy on the heap, points this object to it, and
  // returns the copy.
  //
  // Important: The returned reference is valid only until this pointer is
  // copied or destroyed. Therefore callers should not store the returned
  // reference and rather call `as_mutable()` repeatedly as needed.
  // The `with` functions below provide a safer alternative.
  T& as_mutable() {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copy-constructible");
    auto as_owned = std::move(ref_).AttemptToClaim();
    Ref<T> owned = absl::holds_alternative<Ref<T>>(as_owned)
                       ? absl::get<Ref<T>>(std::move(as_owned))
                       : New<T>(*absl::get<Ref<const T>>(as_owned));
    T& value = *owned;
    ref_ = std::move(owned).Share();
    return value;
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
    std::forward<F>(mutator)(as_mutable());
    return std::move(*this);
  }

 private:
  Ref<const T> ref_;
};

}  // namespace refptr

#endif  // _COPY_ON_WRITE_H
