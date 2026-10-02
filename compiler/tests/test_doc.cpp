// Unit test docgen v0.9.0: ekstraksi `///` + signature + intro modul.
#include "compiler/doc/DocGen.h"
#include <iostream>

using namespace nusa;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int testFnDocs() {
  DocResult r = generateDoc("m.ns", "/// Adds two numbers.\n"
                                    "/// Second line.\n"
                                    "fn add(a: i64, b: i64) -> i64 {\n"
                                    "    return a + b;\n"
                                    "}\n"
                                    "fn undoc() {\n"
                                    "}\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("# API — m.ns") != std::string::npos);
  CHECK(r.markdown.find("### `add(a: i64, b: i64) -> i64`") !=
        std::string::npos);
  CHECK(r.markdown.find("Adds two numbers.\nSecond line.") !=
        std::string::npos);
  CHECK(r.markdown.find("### `undoc() -> void`") != std::string::npos);
  CHECK(r.markdown.find("*No documentation.*") != std::string::npos);
  std::cout << "testFnDocs OK\n";
  return 0;
}

static int testStructDocs() {
  DocResult r = generateDoc("p.ns", "/// A person.\n"
                                    "struct Person {\n"
                                    "    name: string;\n"
                                    "    age: i64;\n"
                                    "}\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("### `struct Person`") != std::string::npos);
  CHECK(r.markdown.find("A person.") != std::string::npos);
  CHECK(r.markdown.find("| name | string |") != std::string::npos);
  CHECK(r.markdown.find("| age | i64 |") != std::string::npos);
  std::cout << "testStructDocs OK\n";
  return 0;
}

// v1.2.0 (Phase 21a): seksi Type Aliases.
static int testAliasDocs() {
  DocResult r = generateDoc("a.ns", "/// Numbers list.\n"
                                    "type Nums = [i64];\n"
                                    "fn main() { }\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("## Type Aliases") != std::string::npos);
  CHECK(r.markdown.find("### `type Nums = [i64]`") != std::string::npos);
  CHECK(r.markdown.find("Numbers list.") != std::string::npos);
  std::cout << "testAliasDocs OK\n";
  return 0;
}

// v1.2.0 (Phase 21c): signature generik.
static int testGenericDocs() {
  DocResult r = generateDoc("g.ns", "/// Identity.\n"
                                    "fn identity<T>(value: T) -> T {\n"
                                    "    return value;\n"
                                    "}\n"
                                    "fn main() { }\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("### `identity<T>(value: T) -> T`") !=
        std::string::npos);
  std::cout << "testGenericDocs OK\n";
  return 0;
}

// v1.2.0 (Phase 21d): header struct generik.
static int testGenericStructDocs() {
  DocResult r = generateDoc("s.ns", "/// A point.\n"
                                    "struct Point<T> {\n"
                                    "    x: T;\n"
                                    "    y: T;\n"
                                    "}\n"
                                    "fn main() { }\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("### `struct Point<T>`") != std::string::npos);
  CHECK(r.markdown.find("| x | T |") != std::string::npos);
  std::cout << "testGenericStructDocs OK\n";
  return 0;
}

// v1.3.0 (Phase 22): seksi Traits.
static int testTraitDocs() {
  DocResult r = generateDoc("t.ns", "/// Printable things.\n"
                                    "trait Printable {\n"
                                    "    fn print(self);\n"
                                    "}\n"
                                    "fn main() { }\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("## Traits") != std::string::npos);
  CHECK(r.markdown.find("### `trait Printable`") != std::string::npos);
  CHECK(r.markdown.find("Printable things.") != std::string::npos);
  std::cout << "testTraitDocs OK\n";
  return 0;
}

// v1.13.0 (Phase 32): seksi extern.
static int testExternDocs() {
  DocResult r = generateDoc("t.ns", "/// C string length.\n"
                                    "extern fn strlen(s: string) -> i64;\n"
                                    "fn main() { }\n");
  CHECK(r.ok);
  CHECK(r.markdown.find("## Extern Functions") != std::string::npos);
  CHECK(r.markdown.find("extern strlen(s: string) -> i64") !=
        std::string::npos);
  CHECK(r.markdown.find("C string length.") != std::string::npos);
  std::cout << "testExternDocs OK\n";
  return 0;
}

static int testModuleIntro() {
  DocResult r = generateDoc("m.ns", "/// Module intro here.\n"
                                    "\n"
                                    "/// Fn doc.\n"
                                    "fn f() {\n"
                                    "}\n");
  CHECK(r.ok);
  // Intro modul muncul sekali di atas; doc fn tidak ganda.
  size_t intro = r.markdown.find("Module intro here.");
  CHECK(intro != std::string::npos);
  CHECK(r.markdown.find("Module intro here.", intro + 1) == std::string::npos);
  CHECK(r.markdown.find("Fn doc.") != std::string::npos);
  std::cout << "testModuleIntro OK\n";
  return 0;
}

static int testDocError() {
  DocResult r = generateDoc("m.ns", "fn main() { @ }\n");
  CHECK(!r.ok);
  CHECK(!r.error.empty());
  std::cout << "testDocError OK\n";
  return 0;
}

// Phase 108 (3.9.0): metadata JSON — struktur + tipe + doc + escaping.
static int testDocJson() {
  bool ok = false;
  std::string err;
  std::string j = generateDocJson(
      "m.ns",
      "/// Adds \"two\" numbers.\n"
      "fn add(a: i64, b: i64) -> i64 {\n"
      "    return a + b;\n"
      "}\n"
      "struct Person {\n"
      "    name: string;\n"
      "}\n"
      "enum E {\n"
      "    Some(i64),\n"
      "    None,\n"
      "}\n",
      ok, err);
  CHECK(ok);
  CHECK(j.find("\"file\":\"m.ns\"") != std::string::npos);
  CHECK(j.find("\"name\":\"add\"") != std::string::npos);
  CHECK(j.find("{\"name\":\"a\",\"type\":\"i64\"}") != std::string::npos);
  CHECK(j.find("\"ret\":\"i64\"") != std::string::npos);
  CHECK(j.find("Adds \\\"two\\\" numbers.") != std::string::npos);
  CHECK(j.find("\"name\":\"Person\"") != std::string::npos);
  CHECK(j.find("{\"name\":\"name\",\"type\":\"string\"}") != std::string::npos);
  CHECK(j.find("\"name\":\"E\"") != std::string::npos);
  CHECK(j.find("{\"name\":\"Some\",\"payload\":[\"i64\"]}") != std::string::npos);
  CHECK(j.find("{\"name\":\"None\",\"payload\":[]}") != std::string::npos);
  std::cout << "testDocJson OK\n";
  return 0;
}

// Phase 108: JSON gagal sama seperti Markdown (error identik).
static int testDocJsonError() {
  bool ok = true;
  std::string err;
  std::string j =
      generateDocJson("b.ns", "fn main() { @ }\n", ok, err);
  CHECK(!ok);
  CHECK(j.empty());
  CHECK(!err.empty());
  DocResult r = generateDoc("b.ns", "fn main() { @ }\n");
  CHECK(!r.ok);
  CHECK(err == r.error);
  std::cout << "testDocJsonError OK\n";
  return 0;
}

int main() {
  if (testFnDocs()) return 1;
  if (testStructDocs()) return 1;
  if (testAliasDocs()) return 1;
  if (testGenericDocs()) return 1;
  if (testGenericStructDocs()) return 1;
  if (testTraitDocs()) return 1;
  if (testExternDocs()) return 1;
  if (testModuleIntro()) return 1;
  if (testDocError()) return 1;
  if (testDocJson()) return 1;
  if (testDocJsonError()) return 1;
  std::cout << "ALL DOC TESTS PASSED\n";
  return 0;
}
