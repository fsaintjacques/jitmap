#include "../query_test.h"

#include <jitmap/query/compiler.h>
#include <jitmap/query/jit.h>
#include <jitmap/query/parser.h>

namespace jitmap {
namespace query {

class JitTest : public QueryTest {};

TEST_F(JitTest, CpuDetection) {
  auto engine = JitEngine();

  EXPECT_NE(engine.GetTargetCPU(), "");
  EXPECT_NE(engine.GetTargetFeatureString(), "");
  EXPECT_NE(engine.GetTargetTriple(), "");
}

void AssertQueryResult(const std::string& query_str,
                       std::vector<BitsetWordType> input_words,
                       BitsetWordType output_word) {
  auto engine = JitEngine();
  auto query = Query::Make("test_query", Parse(query_str, &_g_builder_));
  auto compiler = QueryIRCodeGen("jitest");
  compiler.Compile(query);
  engine.Compile(std::move(compiler));

  auto query_fn = engine.LookupUserQuery("test_query");
  ASSERT_NE(query_fn, nullptr);

  // Populate input bitmaps each with a repeated word.
  size_t n_bitmaps = input_words.size();
  std::vector<std::vector<BitsetWordType>> bitmaps;
  std::vector<const BitsetWordType*> inputs;
  for (size_t i = 0; i < n_bitmaps; i++) {
    auto repeated_word = input_words[i];
    bitmaps.emplace_back(kWordsPerContainers, repeated_word);
    inputs.emplace_back(bitmaps[i].data());
    EXPECT_THAT(bitmaps[i], testing::Each(repeated_word));
  }

  // Populate output bitmap
  std::vector<BitsetWordType> output(kWordsPerContainers, 0UL);
  EXPECT_THAT(output, testing::Each(0UL));

  query_fn(inputs.data(), output.data());
  EXPECT_THAT(output, testing::Each(output_word));
}

TEST_F(JitTest, CompileAndExecuteTest) {
  AssertQueryResult("!a", {0x00FFFF00}, 0xFF0000FF);
  AssertQueryResult("a & b", {0x00FFFF00, 0xFF0000FF}, 0x00000000);
  AssertQueryResult("a | b", {0x00FFFF00, 0xFF0000FF}, 0xFFFFFFFF);
  AssertQueryResult("a ^ b", {0x00FFFF00, 0xFF0000FF}, 0xFFFFFFFF);
}

}  // namespace query
}  // namespace jitmap
