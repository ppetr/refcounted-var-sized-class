# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.11)

enable_testing()

set(CMAKE_CXX_STANDARD 11)
# Set variables for subdirectories.
# See https://stackoverflow.com/a/3769269/1333025.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL
    "Disable Benchmark library tests")
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "")

project(refcounted-var-sized CXX)

include(FetchContent)
function(FetchLibrary)
    set(options)
    set(oneValueArgs GIT_REPOSITORY GIT_TAG)
    set(multiValueArgs FIND_PACKAGE_ARGS)
    cmake_parse_arguments(
        FETCH_LIBRARY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
    set(package ${FETCH_LIBRARY_UNPARSED_ARGUMENTS})
    if (package STREQUAL "")
        message(FATAL_ERROR "No package (non-option) argument given" ${package})
    endif()
    set(find_package_args ${FETCH_LIBRARY_FIND_PACKAGE_ARGS})
    list(REMOVE_ITEM find_package_args "REQUIRED")
    find_package(${package} ${find_package_args} QUIET)
    if(NOT ${package}_FOUND)
        FetchContent_Declare(
            ${package}
            GIT_REPOSITORY ${FETCH_LIBRARY_GIT_REPOSITORY}
            GIT_TAG        ${FETCH_LIBRARY_GIT_TAG}
            )
        FetchContent_MakeAvailable(${package})
    endif()
endfunction()

# sudo apt install libbenchmark-dev libabsl-dev
FetchLibrary(
    absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG        8c0b94e793a66495e0b1f34a5eb26bd7dc672db0  # lts_2022_06_23
    FIND_PACKAGE_ARGS REQUIRED NAMES absl
    )

FetchLibrary(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        e2239ee6043f73722e7aa812a459f54a28552929  # release-1.11.0
    FIND_PACKAGE_ARGS REQUIRED NAMES googletest GTest
    )

# For the benchmarks below be sure to enable optimizations, for example:
#   cmake -DBENCHMARK_ENABLE_LTO=true -DCMAKE_BUILD_TYPE=Release
# See https://github.com/google/benchmark#debug-vs-release.
FetchLibrary(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        db55c89f31385f8105f56ac8245a51777e94a628
    FIND_PACKAGE_ARGS REQUIRED NAMES benchmark
    )

# Var-sized and ref-counted structures.

add_library(ref INTERFACE)
target_include_directories(ref INTERFACE .)
target_link_libraries(ref INTERFACE absl::base absl::utility absl::variant)

add_executable(ref_test ref_test.cc)
target_link_libraries(ref_test absl::utility absl::variant GTest::gtest_main)
add_test(NAME ref_test COMMAND ref_test)

add_library(var_sized INTERFACE)
target_include_directories(var_sized INTERFACE .)
target_link_libraries(var_sized INTERFACE ref)

add_executable(var_sized_test var_sized_test.cc)
target_link_libraries(var_sized_test var_sized absl::strings GTest::gtest_main)
add_test(NAME var_sized_test COMMAND var_sized_test)

add_executable(var_sized_benchmark var_sized_benchmark.cc)
target_link_libraries(var_sized_benchmark var_sized absl::memory benchmark::benchmark_main)
add_test(NAME var_sized_benchmark COMMAND var_sized_benchmark)

# IntOrPtr

add_library(int_or_ptr INTERFACE)
target_include_directories(int_or_ptr INTERFACE .)
target_link_libraries(int_or_ptr INTERFACE ref absl::optional absl::utility absl::variant)

add_executable(int_or_ptr_test int_or_ptr_test.cc)
target_link_libraries(int_or_ptr_test int_or_ptr absl::strings gmock GTest::gtest_main)
add_test(NAME int_or_ptr_test COMMAND int_or_ptr_test)

# Copy on Write.

add_library(copy_on_write INTERFACE)
target_include_directories(copy_on_write INTERFACE .)
target_link_libraries(copy_on_write INTERFACE ref absl::utility)

add_executable(copy_on_write_test copy_on_write_test.cc)
target_link_libraries(copy_on_write_test copy_on_write absl::strings absl::utility GTest::gtest_main)
add_test(NAME copy_on_write_test COMMAND copy_on_write_test)
