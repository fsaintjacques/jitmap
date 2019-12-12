#pragma once

namespace jitmap {
namespace query {

class Expr;

class LiteralExpr;

class ConstantExpr;
class EmptyBitmapExpr;
class FullBitmapExpr;

class ReferenceExpr;
class NamedRefExpr;
class IndexRefExpr;

class OpExpr;

class UnaryOpExpr;
class NotOpExpr;

class BinaryOpExpr;
class AndOpExpr;
class OrOpExpr;
class XorOpExpr;

}  // namespace query
}  // namespace jitmap
