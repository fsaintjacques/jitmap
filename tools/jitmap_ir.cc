#include <iostream>
#include <memory>
#include <string>

#include <llvm/IR/Module.h>

#include <jitmap/query/compiler.h>
#include <jitmap/query/parser.h>
#include <jitmap/query/query.h>

using namespace jitmap::query;

std::string DumpIR(llvm::Module& module) {
  std::string buffer;
  llvm::raw_string_ostream ss{buffer};
  module.print(ss, nullptr);
  return ss.str();
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  try {
    ExprBuilder builder;
    auto expr = Parse(argv[1], &builder);
    auto query = Query::Make("query", expr);
    auto compiler = QueryIRCodeGen("jitmap-ir-module", {});
    compiler.Compile(*query);
    auto [module, ctx] = std::move(compiler).Finish();
    std::cout << DumpIR(*module) << "\n";
    module.reset();
  } catch (jitmap::query::ParserException& e) {
    std::cerr << "Problem parsing '" << argv[1] << "' :\n";
    std::cerr << "\t" << e.message() << "\n";
    return 1;
  }

  return 0;
}
