#include <memory>
#include <iostream>

#include <jitmap/query/compiler.h>
#include <jitmap/query/parser.h>
#include <jitmap/query/query.h>

using namespace jitmap::query;

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  ExprBuilder builder;
  auto expr = Parse(argv[1], &builder);
  auto query = Query::Make("query", expr);
  auto compiler = QueryCompiler("jitmap-ir-module", {});
  compiler.Compile(*query);
  std::cout << compiler.DumpIR() << "\n";

  return 0;
}
