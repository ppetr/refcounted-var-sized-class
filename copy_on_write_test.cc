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

#include <memory>
#include <string>
#include <type_traits>

#include "absl/strings/string_view.h"
#include "absl/utility/utility.h"
#include "gtest/gtest.h"

namespace refptr {
namespace {

static_assert(
    std::is_same<
        std::string,
        std::pointer_traits<CopyOnWrite<std::string>>::element_type>::value,
    "The element_type of CopyOnWrite is incorrect");

constexpr absl::string_view kText = "Lorem ipsum dolor sit amet";

TEST(CopyOnWriteTest, DefaultConstructs) {
  CopyOnWrite<std::string> cow;
  EXPECT_TRUE(cow->empty());  // Test operator->.
  EXPECT_TRUE(cow.LazyDefault());
  EXPECT_EQ(*cow, "");
  cow.as_mutable() = std::string(kText);
  EXPECT_FALSE(cow->empty());  // Test operator->.
  EXPECT_FALSE(cow.LazyDefault());
  EXPECT_EQ(*cow, kText);
  cow = {};
  EXPECT_TRUE(cow.LazyDefault());
  EXPECT_EQ(*cow, "");
}

TEST(CopyOnWriteTest, ConstructsInPlace) {
  CopyOnWrite<std::string> cow(absl::in_place, kText);
  EXPECT_EQ(*cow, kText);
  EXPECT_FALSE(cow->empty());  // Test operator->.
  EXPECT_EQ(cow.as_mutable(), kText);
}

TEST(CopyOnWriteTest, Moves) {
  CopyOnWrite<std::string> original(absl::in_place, kText);
  CopyOnWrite<std::string>& ref_original = original;
  CopyOnWrite<std::string> cow = std::move(original);
  EXPECT_EQ(*cow, kText);
  EXPECT_EQ(cow.as_mutable(), kText);
  EXPECT_TRUE(ref_original.LazyDefault())
      << "A moved-out instance should be empty again";
}

TEST(CopyOnWriteTest, CopiesByWithMutation) {
  CopyOnWrite<std::string> original(absl::in_place, kText);
  CopyOnWrite<std::string> copy =
      original.with([](std::string& s) { s = "other"; });
  // Original.
  EXPECT_EQ(*original, kText);
  EXPECT_EQ(original.as_mutable(), kText);
  // Copy.
  EXPECT_EQ(*copy, "other");
  EXPECT_EQ(copy.as_mutable(), "other");
}

// An example of a data message object, similar to a proto-buf.
class Message {
 public:
  Message() = default;
  Message(const Message&) = default;
  Message(Message&&) = default;
  Message& operator=(const Message&) = default;
  Message& operator=(Message&&) = default;

  absl::string_view value() const { return *value_; }
  std::string& mutable_value() { return value_.as_mutable(); }
  bool has_value() const { return !value_.LazyDefault(); }
  void clear_value() { value_ = {}; }

  const Message& nested() const { return *nested_; }
  Message& mutable_nested() { return nested_.as_mutable(); }
  bool has_nested() const { return !nested_.LazyDefault(); }
  void clear_nested() { nested_ = {}; }

 private:
  CopyOnWrite<std::string> value_;
  CopyOnWrite<Message> nested_;
};

TEST(CopyOnWriteTest, MessagesExampleWorks) {
  Message message;
  EXPECT_FALSE(message.has_value());
  message.mutable_value() = "foo";
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(message.value(), "foo");

  EXPECT_FALSE(message.has_nested());
  message.mutable_nested().mutable_value() = "bar";
  EXPECT_TRUE(message.has_nested());
  EXPECT_EQ(message.nested().value(), "bar");

  EXPECT_FALSE(message.nested().has_nested());
  EXPECT_EQ(message.nested().nested().value(), "");
  EXPECT_FALSE(message.nested().nested().has_value());
}

}  // namespace
}  // namespace refptr
