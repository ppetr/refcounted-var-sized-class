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
#include "absl/types/optional.h"
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

TEST(CopyOnWriteTest, ConstructsInPlace) {
  CopyOnWrite<std::string> cow(absl::in_place, kText);
  EXPECT_EQ(*cow, kText);
  EXPECT_FALSE(cow->empty());  // Test operator->.
  EXPECT_EQ(cow.as_mutable(), kText);
}

TEST(CopyOnWriteTest, Moves) {
  CopyOnWrite<std::string> original(absl::in_place, kText);
  CopyOnWrite<std::string> cow = std::move(original);
  EXPECT_EQ(*cow, kText);
  EXPECT_EQ(cow.as_mutable(), kText);
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
  Message() : Message(Empty()) {}
  Message(const Message&) = default;
  Message(Message&&) = default;
  Message& operator=(const Message&) = default;
  Message& operator=(Message&&) = default;

  absl::string_view value() const;
  std::string& mutable_value();

  const Message& nested() const;
  Message& mutable_nested();

 private:
  struct Data;

  Message(absl::in_place_t) : data_(absl::in_place) {}

  static const Message& Empty() {
    static const Message* const empty = new Message(absl::in_place);
    return *empty;
  }

  CopyOnWrite<Data> data_;
};

struct Message::Data {
  absl::optional<CopyOnWrite<std::string>> value;
  absl::optional<Message> nested;
};

absl::string_view Message::value() const {
  return data_->value ? static_cast<absl::string_view>(**data_->value) : "";
}
std::string& Message::mutable_value() {
  Data& data = data_.as_mutable();
  if (!data.value) {
    data.value.emplace(absl::in_place);
  }
  return data.value->as_mutable();
}

const Message& Message::nested() const {
  return data_->nested ? *data_->nested : Message::Empty();
}
Message& Message::mutable_nested() {
  Data& data = data_.as_mutable();
  if (!data.nested) {
    data.nested.emplace();
  }
  return *data.nested;
}

TEST(CopyOnWriteTest, MessagesExampleWorks) {
  Message message;
  message.mutable_value() = "foo";
  message.mutable_nested().mutable_value() = "bar";
  EXPECT_EQ(message.value(), "foo");
  EXPECT_EQ(message.nested().value(), "bar");
  EXPECT_EQ(message.nested().nested().value(), "");
}

}  // namespace
}  // namespace refptr
