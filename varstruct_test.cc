// Copyright 2017 Google Inc.
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

#include "varstruct.h"

#include <cstdint>
#include <string>
#include <type_traits>

#include "gtest/gtest.h"

namespace {

using std::string;

DEFINE_VARSTRUCT(EmptyStruct){};

TEST(VarstructTest, EmptyVarstruct) {
  auto empty = EmptyStruct::Create({});
  EXPECT_EQ(empty.num_members(), 0);
  EXPECT_EQ(empty.size_bytes(), 0);
}

TEST(Varstruct, TooManyArraySizes) {
  // There are no VARSTRUCT_ARRAY() declarations, so we shouldn't be able to
  // pass in array sizes.
  EXPECT_DEATH_IF_SUPPORTED(EmptyStruct::Create({1}),
                            "array_sizes\\.empty\\(\\)");
}

DEFINE_VARSTRUCT(SimpleStruct) {
  VARSTRUCT_SCALAR(int32_t, foo);
  VARSTRUCT_ARRAY(char, bar);
  VARSTRUCT_ARRAY(char, baz);
};

TEST(VarstructTest, SizeOfScalarsIsStatic) {
  EXPECT_EQ(SimpleStruct::foo_size(), 4);
}

TEST(VarstructTest, ComputesOffsets) {
  auto simple_struct = SimpleStruct::Create({5, 8});
  EXPECT_EQ(simple_struct.foo_offset(), 0);
  EXPECT_EQ(simple_struct.bar_offset(), 4);
  EXPECT_EQ(simple_struct.baz_offset(), 9);
  EXPECT_EQ(simple_struct.num_members(), 3);
  EXPECT_EQ(simple_struct.size_bytes(), 4 + 5 + 8);

  // Attempts to perform pointer operations should result in compile errors.
}

TEST(VarstructTest, NotEnoughArraySizes) {
  EXPECT_DEATH_IF_SUPPORTED(SimpleStruct::Create({}),
                            "!array_sizes\\.empty\\(\\)");
}

TEST(VarstructTest, AccessMembers) {
  // Declare a real struct with compile-time fixed array sizes to allocate and
  // use as input to SimpleStruct::Create(). Because it's always the case that
  // alignof(char) == 1, this struct doesn't have any padding that needs to be
  // forcibly removed with a nonstanard notion like gcc's "packed" attribute.
  struct SimpleStructSource {
    int32_t foo = 3;
    char bar[4] = "abc";
    char baz[5] = "wxyz";
  } simple_struct_source;

  auto simple_struct = SimpleStruct::Create(
      &simple_struct_source,
      {sizeof(SimpleStructSource::bar), sizeof(SimpleStructSource::baz)});

  EXPECT_EQ(simple_struct.foo(), simple_struct_source.foo);
  EXPECT_EQ(simple_struct.baz(2), simple_struct_source.baz[2]);
  simple_struct.set_baz(3, 'a');
  EXPECT_EQ(string(simple_struct_source.baz), "wxya");
}

TEST(VarstructTest, ConstPointer) {
  constexpr char kConstData[] = "This is const data";
  auto simple_struct = SimpleStruct::Create(
      &kConstData, {3, sizeof(kConstData) - sizeof(int) - 3});
  constexpr int kThisStr = 1936287828;  // Bytes "This" interpreted as an int.
  EXPECT_EQ(simple_struct.foo(), kThisStr);
  EXPECT_EQ(simple_struct.bar(1), 'i');

  // Modification is not allowed -- it is a compile error.
}

DEFINE_VARSTRUCT(NonstandardAlignment) {
  VARSTRUCT_SCALAR(char, first);
  VARSTRUCT_SCALAR(uint32_t, second);
};

TEST(VarstructTest, NonstandardAlignment) {
  char buf[] = {'z', 'a', 'b', 'c', 'd'};
  auto nonstandard_alignment = NonstandardAlignment::Create(&buf, {});

  constexpr int kAbcdStr = 1684234849;  // Bytes "abcd" interpreted as an int.
  constexpr int kAbddStr = 1684300385;  // Bytes "abdd" interpreted as an int.
  EXPECT_EQ(nonstandard_alignment.second(), kAbcdStr);
  nonstandard_alignment.set_second(kAbddStr);
  EXPECT_EQ(buf[3], 'd');
  EXPECT_EQ(nonstandard_alignment.second(), kAbddStr);
}


struct InternalStruct {
  int a;
  char b;

  // The compiler may implicitly insert padding even if not specified to
  // ensure alignment. We assume (and verify) that sizeof(InternalStruct) == 8.
  // This test may need to be adjusted for compilers where that is not the case.
  char padding[3];
};

DEFINE_VARSTRUCT(UsesInternalStruct) {
  VARSTRUCT_SCALAR(InternalStruct, first);
  VARSTRUCT_ARRAY(InternalStruct, second);
};

TEST(VarstructTest, InternalStructsWork) {
  // We assume the compiler won't add any extra padding beyond what is specified
  // in the struct definition. If that is not the case for a given complier,
  // this test case may need to be adjusted.
  EXPECT_EQ(sizeof(InternalStruct), 8);

  char buf[] = {'1', '2', '3', '4', 'a', '0', '0', '0', '1', '2', '3', '4',
                'a', '0', '0', '0', '1', '2', '3', '4', 'a', '0', '0', '0'};
  auto uses_internal_struct = UsesInternalStruct::Create(&buf, {2});
  constexpr auto kExpected = InternalStruct{875770417, 'a', "00"};
  EXPECT_EQ(uses_internal_struct.first().a, kExpected.a);
  EXPECT_EQ(uses_internal_struct.first().b, kExpected.b);
  EXPECT_EQ(uses_internal_struct.second(1).a, kExpected.a);
  EXPECT_EQ(uses_internal_struct.second(1).b, kExpected.b);
}

DEFINE_VARSTRUCT(OobChecks) { VARSTRUCT_ARRAY(char, the_array); };

TEST(VarstructTest, OutOfBoundsChecks) {
  char the_source_array[] = "A large buffer with plenty of space";

  // Intentionally tell the varstruct that the size of the array is smaller than
  // it actually is. We do this so that when we test turning off bounds checks
  // we don't actually overrun a real buffer.
  constexpr std::size_t kTheArraySize = 5;
  auto oob_checks = OobChecks::Create(&the_source_array, {kTheArraySize});

  EXPECT_DEATH_IF_SUPPORTED(oob_checks.the_array(kTheArraySize),
                            "array_index >= 0 && array_index < array_elems");
  EXPECT_DEATH_IF_SUPPORTED(oob_checks.the_array(-1),
                            "array_index >= 0 && array_index < array_elems");
  EXPECT_DEATH_IF_SUPPORTED(oob_checks.set_the_array(kTheArraySize, 'a'),
                            "array_index >= 0 && array_index < array_elems");

  // Now, try disabling the checks. These shouldn't crash -- they're not
  // actually outside the bounds of the_source_array.
  EXPECT_EQ(oob_checks.the_array</*bounds_check=*/false>(kTheArraySize), 'g');
  oob_checks.set_the_array</*bounds_check=*/false>(kTheArraySize, 'a');
  EXPECT_EQ(the_source_array[kTheArraySize], 'a');
}

namespace TestNamespace {
DEFINE_VARSTRUCT(InNamespace) { VARSTRUCT_SCALAR(int, the_scalar); };
}  // namespace TestNamespace

TEST(VarstructTest, InsideNamespace) {
  int in_namespace_source = 5;
  auto in_namespace =
      TestNamespace::InNamespace::Create(&in_namespace_source, {});

  EXPECT_EQ(in_namespace.the_scalar(), 5);
  in_namespace.set_the_scalar(7);
  EXPECT_EQ(in_namespace_source, 7);
}

}  // namespace
