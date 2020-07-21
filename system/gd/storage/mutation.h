/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <queue>

#include "os/log.h"
#include "storage/config_cache.h"
#include "storage/mutation_entry.h"

namespace bluetooth {
namespace storage {

class Mutation {
 public:
  explicit Mutation(ConfigCache* config_cache) : config_cache_(config_cache) {
    ASSERT(config_cache_ != nullptr);
  }

  void Add(MutationEntry entry) {
    entries_.emplace(std::move(entry));
  }

  void Commit() {
    config_cache_->Commit(*this);
  }

  friend ConfigCache;

 private:
  ConfigCache* config_cache_;
  std::queue<MutationEntry> entries_;
};

}  // namespace storage
}  // namespace bluetooth