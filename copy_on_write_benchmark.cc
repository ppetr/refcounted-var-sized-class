// Copyright 2023 Google LLC
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

// Benchmarks comparing var-sized, reference-counted data structures to
// unique/shared pointers.

#include <cassert>
#include <cstring>
#include <memory>

#include "absl/utility/utility.h"
#include "benchmark/benchmark.h"
#include "copy_on_write.h"

namespace refptr {
namespace {

static void BM_MutatingOwned(benchmark::State& state) {
  CopyOnWrite<benchmark::IterationCount> value(absl::in_place);
  for (auto _ : state) {
    auto& ref = value.as_mutable();
    ref = state.iterations();
    benchmark::DoNotOptimize(ref);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_MutatingOwned);

static void BM_MutatingCopy(benchmark::State& state) {
  CopyOnWrite<benchmark::IterationCount> value(absl::in_place);
  for (auto _ : state) {
    CopyOnWrite<benchmark::IterationCount> copy(value);
    auto& ref = copy.as_mutable();
    ref = state.iterations();
    benchmark::DoNotOptimize(ref);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_MutatingCopy);

}  // namespace
}  // namespace refptr
