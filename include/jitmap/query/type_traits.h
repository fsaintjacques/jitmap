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

#include <type_traits>

#include <jitmap/query/type_fwd.h>

namespace jitmap {
namespace query {

template <typename E>
using is_literal = std::is_base_of<LiteralExpr, E>;

template <typename E, typename R = void>
using enable_if_literal = std::enable_if_t<is_literal<E>::value, R>;

template <typename E>
using is_variable = std::is_base_of<VariableExpr, E>;

template <typename E, typename R = void>
using enable_if_variable = std::enable_if_t<is_variable<E>::value, R>;

template <typename E>
using is_op = std::is_base_of<OpExpr, E>;

template <typename E, typename R = void>
using enable_if_op = std::enable_if_t<is_op<E>::value, R>;

template <typename E>
using is_unary_op = std::is_base_of<UnaryOpExpr, E>;

template <typename E, typename R = void>
using enable_if_unary_op = std::enable_if_t<is_unary_op<E>::value, R>;

template <typename E>
using is_not_op = std::is_same<NotOpExpr, E>;

template <typename E, typename R = void>
using enable_if_not_op = std::enable_if_t<is_not_op<E>::value, R>;

template <typename E>
using is_binary_op = std::is_base_of<BinaryOpExpr, E>;

template <typename E, typename R = void>
using enable_if_binary_op = std::enable_if_t<is_binary_op<E>::value, R>;

template <typename E>
using is_and_op = std::is_same<AndOpExpr, E>;

template <typename E, typename R = void>
using enable_if_and_op = std::enable_if_t<is_and_op<E>::value, R>;

template <typename E>
using is_or_op = std::is_same<OrOpExpr, E>;

template <typename E, typename R = void>
using enable_if_or_op = std::enable_if_t<is_or_op<E>::value, R>;

template <typename E>
using is_xor_op = std::is_same<XorOpExpr, E>;

template <typename E, typename R = void>
using enable_if_xor_op = std::enable_if_t<is_xor_op<E>::value, R>;

}  // namespace query
}  // namespace jitmap
