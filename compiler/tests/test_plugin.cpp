// Unit test plugin v1 (Phase 112): manifest + op + kontrak.
// Eksekusi tersandbox diuji e2e (butuh clang + fork).
#include "compiler/plugin/Plugin.h"
#include <iostream>

using namespace nusa;
using namespace nusa::plugin;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int testManifestOk() {
  PluginManifest m;
  std::string err;
  CHECK(parseManifest(
      "{\"name\":\"calc\",\"version\":\"1.0\",\"ops\":[\"add\",\"upper\"]}",
      m, err));
  CHECK(m.present);
  CHECK(m.name == "calc");
  CHECK(m.version == "1.0");
  CHECK(m.ops.size() == 2 && m.ops[0] == "add" && m.ops[1] == "upper");
  std::cout << "testManifestOk OK\n";
  return 0;
}

static int testManifestRejects() {
  PluginManifest m;
  std::string err;
  CHECK(!parseManifest("{\"ops\":\"add\"}", m, err));   // bukan array
  CHECK(!parseManifest("{\"ops\":[1]}", m, err));       // bukan string
  CHECK(!parseManifest("{\"name\":7}", m, err));        // bukan string
  CHECK(!parseManifest("{", m, err));                   // terpotong
  CHECK(!parseManifest("", m, err));                    // kosong
  CHECK(!parseManifest("[\"add\"]", m, err));           // bukan objek
  // Kunci asing diabaikan; objek kosong sah.
  CHECK(parseManifest("{\"x\":{\"y\":[1,{\"z\":null}]},\"ops\":[]}", m, err));
  CHECK(m.present && m.ops.empty());
  std::cout << "testManifestRejects OK\n";
  return 0;
}

static int testExtractOp() {
  std::string op;
  std::string err;
  CHECK(extractOp("{\"op\":\"add\",\"a\":20}", op, err));
  CHECK(op == "add");
  // Nested + escape dilewati robust; "op" di mana pun top-level.
  CHECK(extractOp("{\"a\":{\"op\":\"x\"},\"op\":\"go\"}", op, err));
  CHECK(op == "go");
  CHECK(!extractOp("{\"a\":1}", op, err));       // tanpa op
  CHECK(!extractOp("{\"op\":7}", op, err));      // bukan string
  CHECK(!extractOp("[]", op, err));              // bukan objek
  CHECK(!extractOp("{", op, err));               // terpotong
  std::cout << "testExtractOp OK\n";
  return 0;
}

static int testRefusedPreExec() {
  // Penolakan tanpa manifest cocok / op asing: TANPA eksekusi
  // (nsPath fiktif — bila dieksekusi, verdict != refused).
  PluginManifest m;
  std::string err;
  CHECK(parseManifest("{\"ops\":[\"add\"]}", m, err));
  PluginLimits lim;
  PluginResult r1 =
      runPlugin("/nonexistent/plug.ns", m, lim, "{\"op\":\"del\"}");
  CHECK(r1.verdict == "refused");
  PluginResult r2 =
      runPlugin("/nonexistent/plug.ns", m, lim, "{\"a\":1}");
  CHECK(r2.verdict == "refused");
  std::cout << "testRefusedPreExec OK\n";
  return 0;
}

int main() {
  if (testManifestOk())
    return 1;
  if (testManifestRejects())
    return 1;
  if (testExtractOp())
    return 1;
  if (testRefusedPreExec())
    return 1;
  std::cout << "ALL PLUGIN TESTS PASSED\n";
  return 0;
}
