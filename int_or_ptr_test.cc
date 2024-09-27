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

#include "int_or_ptr.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace refptr {
namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::VariantWith;

struct Foo {
 public:
  Foo(int& counter, int value) : counter_(counter), value_(value) {
    counter_++;
  }
  // The constructor is intentionally virtual to make the class non-trivial.
  virtual ~Foo() { counter_--; }

  int& counter_;
  int value_;
};

TEST(IntOrRefTest, ConstructionByDefault) {
  IntOrRef<const Foo> value;
  ASSERT_TRUE(value.has_number());
  ASSERT_FALSE(value.has_ref());
  EXPECT_THAT(value.number(), Optional(0));
  EXPECT_THAT(value.ref(), Eq(nullptr));
}

TEST(IntOrRefTest, ConstructionOfNumber) {
  IntOrRef<const Foo> value(42);
  ASSERT_TRUE(value.has_number());
  ASSERT_FALSE(value.has_ref());
  EXPECT_THAT(value.number(), Optional(42));
  EXPECT_THAT(value.ref(), Eq(nullptr));
  EXPECT_THAT(value.Variant(), VariantWith<intptr_t>(42));
}

TEST(IntOrRefTest, MoveAndCopyConstructionOfNumber) {
  IntOrRef<const Foo> value(42);
  IntOrRef<const Foo> copied(value);
  EXPECT_THAT(copied.Variant(), VariantWith<intptr_t>(42));
  IntOrRef<const Foo> moved(std::move(value));
  EXPECT_THAT(moved.Variant(), VariantWith<intptr_t>(42));
}

TEST(IntOrRefTest, ConstructionOfObject) {
  int counter = 0;
  {
    IntOrRef<const Foo> value(absl::in_place, counter, 42);
    EXPECT_EQ(counter, 1);
    ASSERT_FALSE(value.has_number());
    ASSERT_TRUE(value.has_ref());
    EXPECT_THAT(value.number(), absl::nullopt);
    EXPECT_THAT(value.ref(), Pointee(Field(&Foo::value_, 42)));
    EXPECT_THAT(value.Variant(), VariantWith<std::reference_wrapper<const Foo>>(
                                     Field(&Foo::value_, 42)));
  }
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, CopyConstructionOfObject) {
  int counter = 0;
  {
    IntOrRef<const Foo> value(absl::in_place, counter, 42);
    EXPECT_EQ(counter, 1);
    IntOrRef<const Foo> copy(value);
    EXPECT_THAT(copy.Variant(), VariantWith<std::reference_wrapper<const Foo>>(
                                    Field(&Foo::value_, 42)));
    EXPECT_EQ(counter, 1);
  }
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, MoveConstructionOfObject) {
  int counter = 0;
  {
    IntOrRef<Foo> value(absl::in_place, counter, 42);
    EXPECT_EQ(counter, 1);
    IntOrRef<Foo> moved(std::move(value));
    EXPECT_THAT(moved.Variant(), VariantWith<std::reference_wrapper<Foo>>(
                                     Field(&Foo::value_, 42)));
    EXPECT_EQ(counter, 1);
  }
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, MoveConstructionOfConstFromNonConst) {
  EXPECT_THAT(IntOrRef<const Foo>(IntOrRef<Foo>(42)).number(), Optional(42));
  int counter = 0;
  {
    IntOrRef<const Foo> object(IntOrRef<Foo>(absl::in_place, counter, 73));
    EXPECT_THAT(object.ref(), Pointee(Field(&Foo::value_, 73)));
    EXPECT_EQ(counter, 1);
  }
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, MoveAssignment) {
  IntOrRef<Foo> value;
  int counter = 0;
  value = IntOrRef<Foo>(absl::in_place, counter, 42);
  EXPECT_THAT(value.Variant(), VariantWith<std::reference_wrapper<Foo>>(
                                   Field(&Foo::value_, 42)));
  EXPECT_EQ(counter, 1);
  value = IntOrRef<Foo>(73);
  EXPECT_THAT(value.Variant(), VariantWith<intptr_t>(73));
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, MoveAssignmentOfConst) {
  IntOrRef<const Foo> value;
  int counter = 0;
  value = IntOrRef<const Foo>(absl::in_place, counter, 42);
  EXPECT_THAT(value.Variant(), VariantWith<std::reference_wrapper<const Foo>>(
                                   Field(&Foo::value_, 42)));
  EXPECT_EQ(counter, 1);
  value = IntOrRef<const Foo>(73);
  EXPECT_THAT(value.Variant(), VariantWith<intptr_t>(73));
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, CopyAssignment) {
  IntOrRef<const Foo> value;
  int counter = 0;
  {
    IntOrRef<const Foo> copy(absl::in_place, counter, 42);
    value = copy;
    EXPECT_EQ(counter, 1);
  }
  EXPECT_EQ(counter, 1);
  EXPECT_THAT(value.Variant(), VariantWith<std::reference_wrapper<const Foo>>(
                                   Field(&Foo::value_, 42)));
  {
    IntOrRef<const Foo> copy(73);
    value = copy;
    EXPECT_EQ(counter, 0);
  }
  EXPECT_EQ(counter, 0);
}

TEST(IntOrRefTest, Equality) {
  EXPECT_EQ(IntOrRef<std::string>(42), IntOrRef<std::string>(42));
  EXPECT_NE(IntOrRef<std::string>(42), IntOrRef<std::string>(73));
  EXPECT_NE(IntOrRef<std::string>(42),
            IntOrRef<std::string>(absl::in_place, "Foo"));
  EXPECT_NE(IntOrRef<intptr_t>(42), IntOrRef<intptr_t>(absl::in_place, 42));
  EXPECT_EQ(IntOrRef<std::string>(absl::in_place, "Foo"),
            IntOrRef<std::string>(absl::in_place, "Foo"));
  EXPECT_NE(IntOrRef<std::string>(absl::in_place, "Foo"),
            IntOrRef<std::string>(absl::in_place, "Bar"));
}

// TODO: Add equality tests.

}  // namespace
}  // namespace refptr
