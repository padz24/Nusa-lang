// Unit test bindgen v2.17.0 (Phase 66): subset C → extern fn.
#include "compiler/bindgen/Bindgen.h"
#include <iostream>

using namespace nusa::bindgen;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int testGolden() {
  BindResult r = generateBindings(
      "t.h", "long long answer(void);\n"
             "int add(int a, int b);\n"
             "void tick(void);\n"
             "unsigned count(unsigned n);\n"
             "size_t length(const char *s);\n"
             "_Bool ready(void);\n");
  CHECK(r.ok);
  CHECK(r.code == "extern fn answer() -> i64;\n"
                  "extern fn add(a: i64, b: i64) -> i64;\n"
                  "extern fn tick();\n"
                  "extern fn count(n: i64) -> i64;\n"
                  "extern fn length(s: string) -> i64;\n"
                  "extern fn ready() -> bool;\n");
  CHECK(r.warnings.empty());
  std::cout << "testGolden OK\n";
  return 0;
}

static int testSkips() {
  BindResult r = generateBindings(
      "t.h", "float ratio(float x);\n"
             "int *numbers(int n);\n"
             "const char *name(void);\n"
             "int printf(const char *fmt, ...);\n"
             "struct P { int x; };\n"
             "typedef int myint;\n"
             "static inline int one(void) { return 1; }\n"
             "int ok(void);\n");
  CHECK(r.ok);
  CHECK(r.code == "extern fn ok() -> i64;\n");
  // ratio, numbers, name, printf (struct/typedef/inline: diam-diam).
  CHECK(r.warnings.size() == 4);
  std::cout << "testSkips OK\n";
  return 0;
}

static int testEmpty() {
  BindResult r = generateBindings("t.h", "// nothing\n#define X 1\n");
  CHECK(!r.ok);
  BindResult r2 = generateBindings("t.h", "");
  CHECK(!r2.ok);
  std::cout << "testEmpty OK\n";
  return 0;
}

static int testExternC() {
  BindResult r = generateBindings("t.h", "extern \"C\" {\n"
                                        "long v(long x);\n"
                                        "}\n");
  CHECK(r.ok);
  CHECK(r.code == "extern fn v(x: i64) -> i64;\n");
  std::cout << "testExternC OK\n";
  return 0;
}

int main() {
  if (testGolden()) return 1;
  if (testSkips()) return 1;
  if (testEmpty()) return 1;
  if (testExternC()) return 1;
  std::cout << "ALL BINDGEN TESTS PASSED\n";
  return 0;
}
