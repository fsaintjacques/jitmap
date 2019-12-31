#include "../query_test.h"

#include <atomic>

#include <jitmap/query/compiler.h>
#include <jitmap/query/jit.h>
#include <jitmap/query/parser.h>

namespace jitmap {
namespace query {

class JitTest : public QueryTest {
 public:
  void AssertQueryResult(const std::string& query_expr,
                         std::vector<BitsetWordType> input_words,
                         BitsetWordType output_word) {
    auto query_name = "query_" + std::to_string(id++);
    auto query_fn = CompileAndLookup(query_expr, query_name);
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

  DenseEvalFn CompileAndLookup(std::string_view query_expr,
                               const std::string& query_name) {
    auto query = Query::Make(query_name, Parse(query_expr));
    auto compiler = QueryIRCodeGen("module_" + std::to_string(id++));
    compiler.Compile(query);
    engine_.Compile(std::move(compiler));
    return engine_.LookupUserQuery(query_name);
  }

 protected:
  std::atomic<uint32_t> id = 0;
  JitEngine engine_;
};

TEST_F(JitTest, CpuDetection) {
  EXPECT_NE(engine_.GetTargetCPU(), "");
  EXPECT_NE(engine_.GetTargetTriple(), "");
}

TEST_F(JitTest, CompileAndExecuteTest) {
  BitsetWordType full = 0xFFFFFFFF;
  BitsetWordType empty = 0x0;

  BitsetWordType a = 0b00010010110101011001001011010101;
  BitsetWordType b = 0b11001000110101011101010101011011;
  BitsetWordType c = 0b00000001110101111110101001111000;
  BitsetWordType d = 0b11111111111110101111111100000011;
  BitsetWordType e = 0b11111110100100001110000011010110;

  AssertQueryResult("!a", {a}, ~a);
  AssertQueryResult("a & b", {a, b}, a & b);
  AssertQueryResult("a | b", {a, b}, a | b);
  AssertQueryResult("a ^ b", {a, b}, a ^ b);

  AssertQueryResult("full ^ b", {full, b}, full ^ b);
  AssertQueryResult("empty | !empty", {empty, empty}, full);

  AssertQueryResult("a & b & c & d & e", {a, b, c, d, e}, a & b & c & d & e);
  AssertQueryResult("a | b | c | d | e", {a, b, c, d, e}, a | b | c | d | e);

  // Complex re-use of inputs
  AssertQueryResult("(a | b) & (((!a & c) | (d & b)) ^ (!e & b))", {a, b, c, d, e},
                    (a | b) & (((~a & c) | (d & b)) ^ (~e & b)));
}

}  // namespace query
}  // namespace jitmap
