// Copyright 2020 RStudio, Inc. All rights reserved.
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

#pragma once

#include <memory>

namespace jitmap {
namespace util {

template <typename ImplType>
class Pimpl {
 public:
  explicit Pimpl(std::unique_ptr<ImplType> impl) : impl_(std::move(impl)) {}
  ~Pimpl() {}

 protected:
  ImplType& impl() { return *impl_; }
  const ImplType& impl() const { return *impl_; }

  Pimpl(Pimpl&&) = default;
  Pimpl& operator=(Pimpl&&) = default;

  // Disable copy & assign
  Pimpl(const Pimpl&) = delete;
  Pimpl operator=(const Pimpl&) = delete;

 private:
  std::unique_ptr<ImplType> impl_;
};

}  // namespace util
}  // namespace jitmap
