#include <iostream>
#include <memory>

#include <jitmap/query/compiler.h>
#include <jitmap/query/parser.h>
#include <jitmap/query/query.h>

using namespace jitmap::query;

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  try {
    ExprBuilder builder;
    auto expr = Parse(argv[1], &builder);
    auto query = Query::Make("query", expr);
    auto compiler = QueryCompiler("jitmap-ir-module", {});
    compiler.Compile(*query);
    std::cout << compiler.DumpIR() << "\n";
  } catch (jitmap::query::ParserException& e) {
    std::cerr << "Problem parsing '" << argv[1] << "' :\n";
    std::cerr << "\t" << e.message() << "\n";
    return 1;
  }

  return 0;
}
