// Unit test incremental v1.16.0: fingerprint, graph, perbandingan.
#include "compiler/incr/Incr.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace nusa::incr;
namespace fs = std::filesystem;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static std::string tmpdir() {
  char tmpl[] = "/tmp/nusaincrXXXXXX";
  if (!mkdtemp(tmpl))
    return "";
  return tmpl;
}

static void write(const std::string &path, const std::string &data) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << data;
}

static Fingerprint sample() {
  Fingerprint fp;
  fp.compiler = "sha256:aa";
  fp.target = "x86_64-linux-gnu";
  fp.opt = "2";
  fp.emit = "";
  fp.sysroot = "";
  fp.links = {"lib:m"};
  fp.modules = {ModuleNode{"/p/a.ns", "/p/a.ns", "sha256:01", {}},
                ModuleNode{"/p/b.ns", "/p/b.ns", "sha256:02", {}}};
  fp.ir = "sha256:ff";
  return fp;
}

static int testRoundtrip() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  Fingerprint fp = sample();
  std::string err;
  CHECK(writeFingerprint(d + "/fp", fp, err));
  Fingerprint back;
  CHECK(readFingerprint(d + "/fp", back, err));
  CHECK(back.compiler == fp.compiler);
  CHECK(back.target == fp.target);
  CHECK(back.opt == fp.opt);
  CHECK(back.emit == fp.emit);
  CHECK(back.sysroot == fp.sysroot);
  CHECK(back.links == fp.links);
  CHECK(back.ir == fp.ir);
  CHECK(back.modules.size() == 2);
  CHECK(back.modules[0].path == "/p/a.ns");
  CHECK(back.modules[0].sha == "sha256:01");
  CHECK(back.modules[1].path == "/p/b.ns");
  std::cout << "testRoundtrip OK\n";
  return 0;
}

static int testCompare() {
  Fingerprint a = sample();
  Fingerprint b = sample();
  CHECK(sameInputs(a, b));
  CHECK(sameFlags(a, b));
  b.opt = "0";
  CHECK(!sameInputs(a, b));
  CHECK(!sameFlags(a, b));
  b = sample();
  b.modules[0].sha = "sha256:99";
  CHECK(!sameInputs(a, b));
  CHECK(sameFlags(a, b)); // sumber beda, flag sama
  b = sample();
  b.ir = "sha256:00";
  CHECK(sameInputs(a, b)); // ir diabaikan sameInputs (dibandingkan terpisah)
  CHECK(sameFlags(a, b));
  std::cout << "testCompare OK\n";
  return 0;
}

static int testChanged() {
  Fingerprint a = sample();
  Fingerprint b = sample();
  CHECK(changedModules(a, b).empty());
  b.modules[0].sha = "sha256:99";
  auto ch = changedModules(a, b);
  CHECK(ch.size() == 1 && ch[0] == "~/p/a.ns");
  b.modules.push_back(ModuleNode{"/p/c.ns", "/p/c.ns", "sha256:03", {}});
  ch = changedModules(a, b);
  CHECK(ch.size() == 2);
  std::cout << "testChanged OK\n";
  return 0;
}

static int testSha() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  write(d + "/f.ns", "fn main() { }\n");
  std::string h1 = shaFile(d + "/f.ns");
  CHECK(!h1.empty());
  CHECK(shaFile(d + "/nope.ns").empty());
  CHECK(h1.rfind("sha256:", 0) == 0);
  CHECK(!shaCompiler().empty());
  CHECK(cacheDirFor("prog") == "./.nusa-cache");
  CHECK(cacheDirFor("/tmp/prog") == "/tmp/.nusa-cache");
  CHECK(cacheDirFor("a/b/prog") == "a/b/.nusa-cache");
  std::cout << "testSha OK\n";
  return 0;
}

int main() {
  if (testRoundtrip()) return 1;
  if (testCompare()) return 1;
  if (testChanged()) return 1;
  if (testSha()) return 1;
  std::cout << "ALL INCR TESTS PASSED\n";
  return 0;
}
