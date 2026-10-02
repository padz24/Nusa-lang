// Unit test headers Nusa->C v3.11.0 (Phase 110): subset jujur-ABI,
// skip + warning beralasan, penolakan bool/closure/generik.
#include "compiler/bindgen/Headers.h"
#include <iostream>

using namespace nusa;
using namespace nusa::headers;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int testGoldenHeader() {
  HeaderResult r = generateHeader(
      "api.ns", "struct Point {\n"
                "    x: i64;\n"
                "    name: string;\n"
                "}\n"
                "enum E {\n"
                "    Some(i64),\n"
                "    None,\n"
                "}\n"
                "fn add(a: i64, b: i64) -> i64 {\n"
                "    return a + b;\n"
                "}\n"
                "fn greet(name: string) -> string {\n"
                "    return name;\n"
                "}\n"
                "fn main() {\n"
                "}\n");
  CHECK(r.ok);
  CHECK(r.warnings.empty());
  CHECK(r.code.find("#ifndef NUSA_API_NS") != std::string::npos);
  CHECK(r.code.find("#define NUSA_API_NS") != std::string::npos);
  CHECK(r.code.find("#include <stdint.h>") != std::string::npos);
  CHECK(r.code.find("int64_t x;") != std::string::npos);
  CHECK(r.code.find("const char * name;") != std::string::npos);
  CHECK(r.code.find("} Point;") != std::string::npos);
  CHECK(r.code.find("int64_t tag;") != std::string::npos);
  CHECK(r.code.find("int64_t v0_0; /* Some */") != std::string::npos);
  CHECK(r.code.find("} E;") != std::string::npos);
  CHECK(r.code.find("int64_t add(int64_t a, int64_t b);") !=
        std::string::npos);
  CHECK(r.code.find("const char * greet(const char * name);") !=
        std::string::npos);
  CHECK(r.code.find(" main(") == std::string::npos); // entry tak diekspor
  std::cout << "testGoldenHeader OK\n";
  return 0;
}

static int testRefusals() {
  // bool (i1 vs _Bool), closure, generik, metode -> warning + skip.
  HeaderResult r = generateHeader(
      "mix.ns", "fn isyes(b: bool) -> bool {\n"
                "    return b;\n"
                "}\n"
                "fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
                "    return f(x);\n"
                "}\n"
                "fn id<T>(x: T) -> T {\n"
                "    return x;\n"
                "}\n"
                "struct B {\n"
                "    ok: bool;\n"
                "}\n"
                "fn keep(n: i64) -> i64 {\n"
                "    return n;\n"
                "}\n"
                "fn main() {\n"
                "}\n");
  CHECK(r.ok); // keep() lolos
  CHECK(r.code.find("int64_t keep(int64_t n);") != std::string::npos);
  CHECK(r.code.find("isyes") == std::string::npos);
  CHECK(r.code.find("apply") == std::string::npos);
  CHECK(r.code.find("} B;") == std::string::npos);
  CHECK(r.warnings.size() >= 4);
  bool sawBool = false;
  for (const auto &w : r.warnings) {
    if (w.find("'bool'") != std::string::npos)
      sawBool = true;
  }
  CHECK(sawBool);
  std::cout << "testRefusals OK\n";
  return 0;
}

static int testEmptyFails() {
  // Tanpa API terekspor (hanya main) -> rc 1.
  HeaderResult r =
      generateHeader("empty.ns", "fn main() {\n    print(1);\n}\n");
  CHECK(!r.ok);
  CHECK(!r.error.empty());
  std::cout << "testEmptyFails OK\n";
  return 0;
}

static int testBadInput() {
  HeaderResult r = generateHeader("b.ns", "fn main() { @ }\n");
  CHECK(!r.ok);
  CHECK(!r.error.empty());
  std::cout << "testBadInput OK\n";
  return 0;
}

int main() {
  if (testGoldenHeader())
    return 1;
  if (testRefusals())
    return 1;
  if (testEmptyFails())
    return 1;
  if (testBadInput())
    return 1;
  std::cout << "ALL HEADERS TESTS PASSED\n";
  return 0;
}
