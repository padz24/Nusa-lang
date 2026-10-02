// Unit test builtins v0.9.0: typing rules + bentuk IR libc calls.
// Pipeline penuh (lex→typecheck→generate); nilai runtime di e2e_stdlib.sh.
#include "compiler/codegen/Codegen.h"
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
#include "compiler/semantic/Semantic.h"
#include "compiler/semantic/TypeChecker.h"
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

// Returns "" + errs terisi bila gagal sebelum/dalam generate.
static std::string fullIR(const std::string &src, bool &ok,
                          std::vector<SemanticError> &errs) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty()) {
    ok = false;
    return "";
  }
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  if (!pres.ok()) {
    ok = false;
    return "";
  }
  SemanticAnalyzer sema;
  auto serrs = sema.analyze(pres.program);
  if (!serrs.empty()) {
    ok = false;
    errs = serrs;
    return "";
  }
  TypeChecker checker;
  checker.check(pres.program);
  if (!checker.errors().empty()) {
    ok = false;
    errs = checker.errors();
    return "";
  }
  Codegen cg("nusa", "t.ns");
  std::string genErr;
  ok = cg.generate(pres.program, genErr);
  if (!ok)
    return "";
  return cg.emitIRToString();
}

static bool hasErr(const std::vector<SemanticError> &errs,
                   const std::string &sub) {
  for (const auto &e : errs)
    if (e.message.find(sub) != std::string::npos)
      return true;
  return false;
}

static int testValidBuiltins() {
  bool ok = false;
  std::vector<SemanticError> errs;
  std::string ir = fullIR("fn main() {\n"
                          "    let s = input();\n"
                          "    print(str_len(s));\n"
                          "    print(str_concat(s, \"!\"));\n"
                          "    print(parse_int(\"7\"));\n"
                          "    print(parse_int_valid(\"7\"));\n"
                          "    assert(true);\n"
                          "    print(file_exists(\"x\"));\n"
                          "    print(file_read(\"x\"));\n"
                          "    print(file_write(\"x\", \"y\"));\n"
                          "    let a = [1, 2];\n"
                          "    print(array_sum(a));\n"
                          "    print(array_min(a));\n"
                          "    print(array_max(a));\n"
                          "}\n",
                          ok, errs);
  CHECK(ok);
  CHECK(ir.find("declare i64 @getline") != std::string::npos);
  CHECK(ir.find("declare i64 @strlen") != std::string::npos);
  CHECK(ir.find("declare ptr @malloc") != std::string::npos);
  CHECK(ir.find("declare i64 @strtoll") != std::string::npos);
  CHECK(ir.find("declare i32 @sscanf") != std::string::npos);
  CHECK(ir.find("declare ptr @fopen") != std::string::npos);
  CHECK(ir.find("assertion failed at") != std::string::npos);
  CHECK(ir.find("as.cond") != std::string::npos); // loop array_sum
  std::cout << "testValidBuiltins OK\n";
  return 0;
}

static int testArityAndTypes() {
  struct Case {
    const char *src;
    const char *want;
  };
  Case cases[] = {
      {"fn main() { input(1); }", "wrong number of arguments for 'input'"},
      {"fn main() { assert(1); }", "requires a 'bool' argument"},
      {"fn main() { print(str_len(1)); }", "requires a 'string' argument"},
      {"fn main() { print(str_concat(\"a\")); }",
       "wrong number of arguments for 'str_concat'"},
      {"fn main() { print(parse_int(1)); }", "requires a 'string' argument"},
      {"fn main() { print(parse_int_valid(1)); }",
       "requires a 'string' argument"},
      {"fn main() { print(file_read(1)); }", "requires a 'string' argument"},
      {"fn main() { print(file_write(\"a\")); }",
       "wrong number of arguments for 'file_write'"},
      {"fn main() { print(file_exists(1)); }",
       "requires a 'string' argument"},
      {"fn main() { let a = [true]; print(array_sum(a)); }",
       "requires an '[i64]' argument"},
      {"fn main() { print(array_min(1)); }",
       "requires an '[i64]' argument"},
  };
  for (const auto &c : cases) {
    bool ok = true;
    std::vector<SemanticError> errs;
    fullIR(c.src, ok, errs);
    CHECK(!ok);
    if (!hasErr(errs, c.want)) {
      std::cerr << "missing error '" << c.want << "' for: " << c.src << "\n";
      for (const auto &e : errs)
        std::cerr << "  got: " << e.message << "\n";
      return 1;
    }
  }
  std::cout << "testArityAndTypes OK\n";
  return 0;
}

static int testReservedBuiltins() {
  bool ok = true;
  std::vector<SemanticError> errs;
  fullIR("fn input() -> i64 { return 1; }\nfn main() { }\n", ok, errs);
  CHECK(!ok);
  CHECK(hasErr(errs, "cannot declare function 'input'"));
  std::cout << "testReservedBuiltins OK\n";
  return 0;
}

// v1.9.0 (Phase 28): concurrency — typing + bentuk IR runtime calls.
static int testConcurrencyValid() {
  bool ok = false;
  std::vector<SemanticError> errs;
  std::string ir = fullIR("fn work() -> i64 {\n"
                          "    return 1;\n"
                          "}\n"
                          "fn main() {\n"
                          "    let h = thread_spawn(work);\n"
                          "    print(thread_join(h));\n"
                          "    let m = mutex_new();\n"
                          "    mutex_lock(m);\n"
                          "    print(mutex_try_lock(m));\n"
                          "    mutex_unlock(m);\n"
                          "    mutex_free(m);\n"
                          "    let c = chan_new();\n"
                          "    chan_send(c, 7);\n"
                          "    print(chan_recv(c));\n"
                          "    chan_free(c);\n"
                          "    let a = atomic_new(10);\n"
                          "    print(atomic_get(a));\n"
                          "    atomic_set(a, 20);\n"
                          "    print(atomic_add(a, 5));\n"
                          "    atomic_free(a);\n"
                          "    sleep_ms(1);\n"
                          "}\n",
                          ok, errs);
  CHECK(ok);
  CHECK(ir.find("declare i64 @nusa_thread_spawn") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_thread_join") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_mutex_new") != std::string::npos);
  CHECK(ir.find("declare void @nusa_mutex_lock") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_mutex_try_lock") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_chan_new") != std::string::npos);
  CHECK(ir.find("declare void @nusa_chan_send") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_chan_recv") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_atomic_new") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_atomic_add") != std::string::npos);
  CHECK(ir.find("declare void @nusa_sleep_ms") != std::string::npos);
  std::cout << "testConcurrencyValid OK\n";
  return 0;
}

// v3.19.0 (Phase 118): backpressure — bounded + try_send (bool).
static int testBackpressureValid() {
  bool ok = false;
  std::vector<SemanticError> errs;
  std::string ir = fullIR("fn main() {\n"
                          "    let c = chan_new_bounded(2);\n"
                          "    print(chan_try_send(c, 1));\n"
                          "    print(chan_try_send(c, 2));\n"
                          "    print(chan_try_send(c, 3));\n"
                          "    print(chan_recv(c));\n"
                          "    chan_free(c);\n"
                          "}\n",
                          ok, errs);
  CHECK(ok);
  CHECK(ir.find("declare i64 @nusa_chan_new_bounded") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_chan_try_send") != std::string::npos);
  std::cout << "testBackpressureValid OK\n";
  return 0;
}

// 2.0: channel bertag (bool/string) — typing + bentuk IR runtime calls.
static int testTaggedChannelsValid() {
  bool ok = false;
  std::vector<SemanticError> errs;
  std::string ir = fullIR("fn main() {\n"
                          "    let cb = chan_new_bool();\n"
                          "    chan_send_bool(cb, true);\n"
                          "    print(chan_recv_bool(cb));\n"
                          "    let cs = chan_new_str();\n"
                          "    chan_send_str(cs, \"hi\");\n"
                          "    print(chan_recv_str(cs));\n"
                          "}\n",
                          ok, errs);
  CHECK(ok);
  CHECK(ir.find("declare i64 @nusa_chan_new_tagged") != std::string::npos);
  CHECK(ir.find("declare void @nusa_chan_send_tagged") !=
        std::string::npos);
  CHECK(ir.find("declare i64 @nusa_chan_recv_tagged") != std::string::npos);
  std::cout << "testTaggedChannelsValid OK\n";
  return 0;
}

static int testConcurrencyTypes() {  struct Case {
    const char *src;
    const char *want;
  };
  Case cases[] = {
      {"fn main() { let h = thread_spawn(1); }",
       "requires a 'fn() -> i64' argument"},
      {"fn f(x: i64) -> i64 { return x; }\n"
       "fn main() { let h = thread_spawn(f); }",
       "requires a 'fn() -> i64' argument"},
      {"fn main() { thread_join(\"x\"); }",
       "an 'i64' handle argument"},
      {"fn main() { mutex_lock(1, 2); }",
       "wrong number of arguments for 'mutex_lock'"},
      {"fn main() { chan_send(1); }",
       "wrong number of arguments for 'chan_send'"},
      {"fn main() { print(chan_recv(\"c\")); }",
       "an 'i64' handle argument"},
      {"fn main() { chan_send_str(1, 2); }",
       "a 'string' value argument"},
      {"fn main() { chan_send_bool(1, 1); }",
       "a 'bool' value argument"},
      {"fn main() { print(chan_recv_bool(\"c\")); }",
       "an 'i64' handle argument"},
      {"fn main() { let a = atomic_new(\"x\"); }",
       "an 'i64' initial value"},
      {"fn main() { print(atomic_add(1)); }",
       "wrong number of arguments for 'atomic_add'"},
      {"fn main() { let c = chan_new_bounded(); }",
       "wrong number of arguments for 'chan_new_bounded'"},
      {"fn main() { let c = chan_new_bounded(\"x\"); }",
       "an 'i64' capacity argument"},
      {"fn main() { print(chan_try_send(1)); }",
       "wrong number of arguments for 'chan_try_send'"},
      {"fn main() { print(chan_try_send(1, \"x\")); }",
       "an 'i64' value argument"},
      {"fn main() { sleep_ms(\"x\"); }", "an 'i64' handle argument"},
      {"fn thread_spawn() -> i64 { return 1; }\nfn main() { }\n",
       "cannot declare function 'thread_spawn'"},
  };
  for (const auto &c : cases) {
    bool ok = true;
    std::vector<SemanticError> errs;
    fullIR(c.src, ok, errs);
    CHECK(!ok);
    if (!hasErr(errs, c.want)) {
      std::cerr << "missing error '" << c.want << "' for: " << c.src << "\n";
      for (const auto &e : errs)
        std::cerr << "  got: " << e.message << "\n";
      return 1;
    }
  }
  std::cout << "testConcurrencyTypes OK\n";
  return 0;
}

// v1.10.0 (Phase 29): stdlib 2.0 builtins — typing + IR decls.
static int testStdlib2Valid() {
  bool ok = false;
  std::vector<SemanticError> errs;
  std::string ir = fullIR("fn main() {\n"
                          "    print(str_char_at(\"hi\", 1));\n"
                          "    print(str_slice(\"hi\", 0, 2));\n"
                          "    print(time_now_ms());\n"
                          "    print(time_now_s());\n"
                          "    print(random_u64());\n"
                          "    print(process_argc());\n"
                          "    print(process_argv(0));\n"
                          "    print(process_env(\"X\"));\n"
                          "    print(process_cwd());\n"
                          "    print(tcp_connect(\"h\", 80));\n"
                          "    print(tcp_send(1, \"d\"));\n"
                          "    print(tcp_recv(1, 8));\n"
                          "    tcp_close(1);\n"
                          "    let srv = tcp_listen(80);\n"
                          "    print(tcp_accept(srv));\n"
                          "    sleep_ms(0);\n"
                          "}\n",
                          ok, errs);
  CHECK(ok);
  CHECK(ir.find("declare i64 @nusa_time_now_ms") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_random_u64") != std::string::npos);
  CHECK(ir.find("declare ptr @getenv") != std::string::npos);
  CHECK(ir.find("declare ptr @getcwd") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_tcp_connect") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_tcp_listen") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_tcp_accept") != std::string::npos);
  CHECK(ir.find("declare i64 @nusa_tcp_send") != std::string::npos);
  CHECK(ir.find("declare ptr @nusa_tcp_recv") != std::string::npos);
  CHECK(ir.find("nusa_argc") != std::string::npos);
  std::cout << "testStdlib2Valid OK\n";
  return 0;
}

static int testStdlib2Types() {
  struct Case {
    const char *src;
    const char *want;
  };
  Case cases[] = {
      {"fn main() { print(str_char_at(1, 0)); }",
       "a 'string' argument"},
      {"fn main() { print(str_slice(\"a\", 0)); }",
       "wrong number of arguments for 'str_slice'"},
      {"fn main() { print(time_now_ms(1)); }",
       "wrong number of arguments for 'time_now_ms'"},
      {"fn main() { process_exit(); }",
       "wrong number of arguments for 'process_exit'"},
      {"fn main() { print(process_env(1)); }",
       "a 'string' argument"},
      {"fn main() { print(process_argv(\"x\")); }",
       "an 'i64' index argument"},
      {"fn main() { print(tcp_connect(\"h\")); }",
       "wrong number of arguments for 'tcp_connect'"},
      {"fn main() { print(tcp_send(1, 2)); }",
       "a 'string' data argument"},
      {"fn main() { print(tcp_recv(1)); }",
       "wrong number of arguments for 'tcp_recv'"},
      {"fn main() { print(tcp_listen(\"80\")); }",
       "an 'i64' port argument"},
      {"fn main() { print(tcp_accept(\"s\")); }",
       "an 'i64' socket argument"},
      {"fn time_now_ms() -> i64 { return 1; }\nfn main() { }\n",
       "cannot declare function 'time_now_ms'"},
  };
  for (const auto &c : cases) {
    bool ok = true;
    std::vector<SemanticError> errs;
    fullIR(c.src, ok, errs);
    CHECK(!ok);
    if (!hasErr(errs, c.want)) {
      std::cerr << "missing error '" << c.want << "' for: " << c.src << "\n";
      for (const auto &e : errs)
        std::cerr << "  got: " << e.message << "\n";
      return 1;
    }
  }
  std::cout << "testStdlib2Types OK\n";
  return 0;
}

static int testCryptoValid() {
  bool ok = false;
  std::vector<SemanticError> errs;
  std::string ir = fullIR("fn main() {\n"
                          "    print(sha256_hex(\"abc\"));\n"
                          "    print(crypto_rand_hex(16));\n"
                          "}\n",
                          ok, errs);
  CHECK(ok);
  CHECK(ir.find("declare ptr @nusa_sha256_hex") != std::string::npos);
  CHECK(ir.find("declare ptr @nusa_crypto_rand_hex") != std::string::npos);
  std::cout << "testCryptoValid OK\n";
  return 0;
}

static int testCryptoTypes() {
  struct Case {
    const char *src;
    const char *want;
  };
  Case cases[] = {
      {"fn main() { print(sha256_hex(1)); }", "a 'string' argument"},
      {"fn main() { print(sha256_hex()); }",
       "wrong number of arguments for 'sha256_hex'"},
      {"fn main() { print(crypto_rand_hex(\"16\")); }",
       "an 'i64' argument"},
      {"fn main() { print(crypto_rand_hex(1, 2)); }",
       "wrong number of arguments for 'crypto_rand_hex'"},
      {"fn sha256_hex(s: string) -> string { return s; }\nfn main() { }\n",
       "cannot declare function 'sha256_hex'"},
  };
  for (const auto &c : cases) {
    bool ok = true;
    std::vector<SemanticError> errs;
    fullIR(c.src, ok, errs);
    CHECK(!ok);
    if (!hasErr(errs, c.want)) {
      std::cerr << "missing error '" << c.want << "' for: " << c.src << "\n";
      for (const auto &e : errs)
        std::cerr << "  got: " << e.message << "\n";
      return 1;
    }
  }
  std::cout << "testCryptoTypes OK\n";
  return 0;
}

int main() {
  if (testValidBuiltins()) return 1;
  if (testArityAndTypes()) return 1;
  if (testReservedBuiltins()) return 1;
  if (testConcurrencyValid()) return 1;
  if (testBackpressureValid()) return 1;
  if (testTaggedChannelsValid()) return 1;
  if (testConcurrencyTypes()) return 1;
  if (testStdlib2Valid()) return 1;
  if (testStdlib2Types()) return 1;
  if (testCryptoValid()) return 1;
  if (testCryptoTypes()) return 1;
  std::cout << "ALL BUILTIN TESTS PASSED\n";
  return 0;
}
