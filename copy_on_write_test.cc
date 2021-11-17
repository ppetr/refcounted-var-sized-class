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

#include "copy_on_write.h"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>

#include "absl/utility/utility.h"

int main() {
  using refptr::CopyOnWrite;

  CopyOnWrite<std::string> cow(absl::in_place, "Lorem ipsum dolor sit amet");
  std::cout << *cow << std::endl;
  assert(cow);
  assert(*cow == "Lorem ipsum dolor sit amet");
  assert(!cow->empty());  // Test operator->.
  assert(cow.as_mutable() == "Lorem ipsum dolor sit amet");
  // Move and copy.
  CopyOnWrite<std::string> cow2 = std::move(cow);
  CopyOnWrite<std::string> cow3 = cow2;
  assert(cow2);
  assert(*cow2 == "Lorem ipsum dolor sit amet");
  assert(!cow2->empty());  // Test operator->.
  assert(cow2.as_mutable() == "Lorem ipsum dolor sit amet");
  // Copied.
  assert(cow3);
  assert(*cow3 == "Lorem ipsum dolor sit amet");
  assert(!cow3->empty());  // Test operator->.
  assert(cow3.as_mutable() == "Lorem ipsum dolor sit amet");
}
