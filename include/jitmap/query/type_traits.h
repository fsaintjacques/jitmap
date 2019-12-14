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
