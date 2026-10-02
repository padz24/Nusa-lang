#include "compiler/driver/Driver.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/lsp/Lsp.h"
#include "compiler/pkg/Pkg.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

  // Dump escape untuk `nusa lex` kini di Driver::runLex (Phase 39)
  // agar `nusa bootstrap` memakai referensi Stage 0 yang sama persis.

static void printHelp() {
  std::printf("Nusa programming language v%s — Stable\n\n", NUSA_VERSION);
  std::printf("Usage:\n"
              "  nusa --version          Print version\n"
              "  nusa --help             Print this help\n"
              "  nusa build <file.ns> [-o <output>]   Compile to native executable\n"
              "    [--profile <dev|debug|release>] [--link <lib|file>]... [--link-dir <dir>]...\n"
              "    [--target <t>] [--sysroot <dir>] [--emit-ir|--emit-asm|-c]  Cross\n"
              "    [-O0|-O1|-O2|-O3|-Os|-Oz]  LLVM optimization (default O0)\n"
              "    [--fresh]  Ignore incremental cache\n"
              "    [-g|--debug]  DWARF debug info\n"
              "    [--stats]  Print compile-time breakdown (frontend/codegen/clang)\n"
              "    [--pgo-generate]  PGO: build instrumented binary\n"
              "    [--pgo-use <profdata>]  PGO: optimized build from profile\n"
              "  nusa run <file.ns> [args...]  Build and run (args via process_argv)\n"
              "  nusa bench <file.ns> [-n N] [-- args...]  Benchmark N runs (time + maxRSS)\n"
              "  nusa check <file.ns>   Validate without compiling\n"
              "  nusa trace run <file.ns> [args...]  Run with TRACE_ID injection\n"
              "  nusa trace check <trace.log>  Validate nusa-trace log\n"
              "  nusa repl                     Interactive read-eval-print loop\n"
  "  nusa migrate [PATH]    Audit 2.x code for 3.0 (compatible/\n"
  "                         deprecated/requires migration/broken/\n"
  "                         unsupported; default: .)\n"
              "  nusa clean [DIR]         Remove generated artifacts (*.ns.ll,\n"
              "                                  .nusa-cache/, target/, dist/)\n"
              "  nusa test [PATH...]    Run `*_test.ns` files (default: .)\n"
              "  nusa fmt [--check] <file.ns>...      Format source files\n"
              "  nusa doc <file.ns> [-o <out>] [--format markdown|json]  API docs/metadata\n"
  "  nusa lsp                         Language server (LSP over stdio)\n"
              "  nusa lex <file.ns>              Dump lexer tokens (debug)\n"
  "  nusa ast <file.ns>              Dump parser AST (debug)\n"
              "  nusa bindgen <header.h> [-o out.ns]  Generate extern fn bindings (C subset)\n"
              "  nusa headers <file.ns> [-o out.h]  Generate C header from Nusa API (ABI-honest subset)\n"
  "  nusa bootstrap [lex.ns] [-o DIR]  Stage 0->1->2 + repro check\n"
  "    [--as lex|ast]  Stage-0 reference kind (default lex)\n"
  "    [--rebuild DIR]  Stage 3: rebuild + compare (bootstrap full)\n"
  "  nusa sandbox <file.ns> [--timeout S] [--memory-mb M] [--output-kb K]\n"
  "                                  Run untrusted code under limits\n"
  "  nusa playground [--port P] [--timeout S] [--memory-mb M]\n"
  "                                  Local HTTP playground (127.0.0.1 only)\n"
  "  nusa plugin run <file.ns> [--manifest m.json] [--timeout S]\n"
  "                                  [--memory-mb M] [--output-kb K]\n"
  "                                  Sandboxed JSON plugin (stdin request)\n"
  "  nusa new <template> [name] [--dir DIR]  Scaffold a project\n"
  "  nusa new --list                 List project templates\n"
              "  nusa pkg <init|add|remove|update|build|publish|search|install|tree|verify|outdated|audit|sbom>  Package manager\n"
              "  nusa search <query> [--registry DIR]  Search registry\n"
              "  nusa install <name>[@req] [--registry DIR]  Add+update from registry\n"
              "  nusa publish [--registry DIR]  Publish current package\n"
              "  nusa tree                        Dependency graph\n"
              "  nusa verify [--registry DIR]     Re-verify lock checksums\n"
              "  nusa outdated [--registry DIR]   Newer registry versions\n"
              "  nusa audit                       Capability + checksum report\n"
              "  nusa sbom                        JSON supply-chain inventory (nusa-sbom/1)\n"
              "\n"
              "Examples:\n"
              "  nusa build examples/hello.ns\n"
              "  ./hello\n"
              "  nusa run examples/hello.ns\n"
              "  nusa check examples/hello.ns\n"
              "  nusa test tests/\n"
              "  nusa fmt --check examples/hello.ns\n"
              "  nusa pkg init myapp\n");
}

int main(int argc, char **argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) {
    printHelp();
    return 1;
  }
  if (args[0] == "--version" || args[0] == "-V") {
    std::printf("%s\n", nusa::Driver::versionString());
    return 0;
  }
  if (args[0] == "--help" || args[0] == "-h" || args[0] == "help") {
    printHelp();
    return 0;
  }
  nusa::Driver driver;
  if (args[0] == "build") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa build' needs a file.ns argument\n");
      return 1;
    }
    std::string input = args[1];
    std::string output;
    nusa::LinkSpec links;
    std::string target, sysroot, emit, opt;
    bool fresh = false;
    bool debug = false;
    bool stats = false;
    bool pgoGenerate = false;
    std::string pgoUse;
    // Profil (Phase 62): default; flag eksplisit menang urutan-bebas.
    std::string profOpt;
    bool profDebug = false;
    bool haveProfile = false;
    bool optExplicit = false;
    bool debugExplicit = false;
    for (size_t i = 2; i < args.size(); i++) {
      if (args[i] == "-o" && i + 1 < args.size()) {
        output = args[++i];
      } else if (args[i] == "--profile" && i + 1 < args.size()) {
        std::string perr;
        if (!nusa::Driver::resolveProfile(args[++i], profOpt, profDebug,
                                           perr)) {
          std::fprintf(stderr, "error: %s\n", perr.c_str());
          return 1;
        }
        haveProfile = true;
      } else if (args[i] == "--link" && i + 1 < args.size()) {
        // Nama library (-l) atau path file (.a/.o/.so).
        std::string item = args[++i];
        bool isFile = item.find('/') != std::string::npos ||
                      item.find('\\') != std::string::npos ||
                      (item.size() > 2 && item.compare(item.size() - 2, 2,
                                                       ".a") == 0) ||
                      (item.size() > 2 && item.compare(item.size() - 2, 2,
                                                       ".o") == 0) ||
                      (item.size() > 3 && item.compare(item.size() - 3, 3,
                                                       ".so") == 0);
        if (isFile)
          links.files.push_back(item);
        else
          links.libs.push_back(item);
      } else if (args[i] == "--link-dir" && i + 1 < args.size()) {
        links.libDirs.push_back(args[++i]);
      } else if (args[i] == "--target" && i + 1 < args.size()) {
        target = args[++i];
      } else if (args[i] == "--sysroot" && i + 1 < args.size()) {
        sysroot = args[++i];
      } else if (args[i] == "--emit-ir") {
        emit = "ir";
      } else if (args[i] == "--emit-asm") {
        emit = "asm";
      } else if (args[i] == "-c" || args[i] == "--emit-obj") {
        emit = "obj";
      } else if (args[i] == "--fresh") {
        fresh = true;
      } else if (args[i] == "--stats") {
        stats = true;
      } else if (args[i] == "-g" || args[i] == "--debug") {
        debug = true;
        debugExplicit = true;
      } else if (args[i] == "--pgo-generate") {
        pgoGenerate = true;
      } else if (args[i] == "--pgo-use" && i + 1 < args.size()) {
        pgoUse = args[++i];
      } else if (args[i].rfind("--pgo-use=", 0) == 0) {
        pgoUse = args[i].substr(10);
      } else if (args[i].size() == 3 && args[i][0] == '-' &&
                 args[i][1] == 'O' &&
                 (args[i][2] == '0' || args[i][2] == '1' ||
                  args[i][2] == '2' || args[i][2] == '3' ||
                  args[i][2] == 's' || args[i][2] == 'z')) {
        opt = args[i].substr(2, 1);
        optExplicit = true;
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    if (haveProfile) {
      if (!optExplicit)
        opt = profOpt;
      if (!debugExplicit)
        debug = profDebug;
    }
    return driver.runBuild(input, output, links, target, sysroot, emit,
                           opt, fresh, debug, stats, pgoGenerate, pgoUse);
  }
  if (args[0] == "run") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa run' needs a file.ns argument\n");
      return 1;
    }
    // --target untuk run: validasi saja (eksekusi hanya host).
    // Dikutip dari argumen program agar tak diteruskan.
    std::vector<std::string> pargs;
    for (size_t i = 2; i < args.size(); i++) {
      if (args[i] == "--target" && i + 1 < args.size()) {
        nusa::ResolvedTarget rt;
        std::string terr;
        if (!nusa::Driver::resolveTarget(args[++i], rt, terr)) {
          std::fprintf(stderr, "error: %s\n", terr.c_str());
          return 1;
        }
        if (!rt.isHost) {
          std::fprintf(stderr,
                       "error: cannot run target '%s' (not the host)\n",
                       rt.triple.c_str());
          std::fprintf(stderr, "help: build it instead: nusa build %s "
                               "--target %s\n",
                       args[1].c_str(), rt.triple.c_str());
          return 1;
        }
      } else {
        pargs.push_back(args[i]);
      }
    }
    return driver.runRun(args[1], pargs);
  }
  if (args[0] == "check") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa check' needs a file.ns argument\n");
      return 1;
    }
    return driver.runCheck(args[1]);
  }
  if (args[0] == "trace") {
    // Tracing terdistribusi v1 (Phase 119): `run` (injeksi TRACE_ID)
    // dan `check` (validasi log). `inspect` = follow-up.
    if (args.size() < 2 ||
        (args[1] != "run" && args[1] != "check")) {
      std::fprintf(stderr,
                   "error: usage: nusa trace run <file.ns> [args...] | "
                   "nusa trace check <trace.log>\n");
      return 1;
    }
    if (args[1] == "check") {
      if (args.size() != 3) {
        std::fprintf(stderr, "error: 'nusa trace check' needs a log file\n");
        return 1;
      }
      return driver.runTraceCheck(args[2]);
    }
    if (args.size() < 3) {
      std::fprintf(stderr, "error: 'nusa trace run' needs a file.ns argument\n");
      return 1;
    }
    std::vector<std::string> pargs(args.begin() + 3, args.end());
    return driver.runTraceRun(args[2], pargs);
  }
  if (args[0] == "repl") {
    if (args.size() > 1) {
      std::fprintf(stderr, "error: 'nusa repl' takes no arguments\n");
      return 1;
    }
    return driver.runRepl();
  }
  if (args[0] == "migrate") {
    if (args.size() > 2) {
      std::fprintf(stderr, "error: 'nusa migrate' takes at most one PATH\n");
      return 1;
    }
    std::string target = args.size() > 1 ? args[1] : ".";
    return driver.runMigrate(target);
  }
  if (args[0] == "bench") {
    // Benchmark N run: waktu dinding + maxRSS (statistik ke stderr,
    // output program diteruskan). `nusa bench prog.ns [-n N] [-- args]`.
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa bench' needs a file.ns argument\n");
      return 1;
    }
    std::string input = args[1];
    int runs = 5;
    std::vector<std::string> pargs;
    bool dashdash = false;
    for (size_t i = 2; i < args.size(); i++) {
      if (!dashdash && args[i] == "--") {
        dashdash = true;
      } else if (!dashdash && args[i] == "-n" && i + 1 < args.size()) {
        runs = std::atoi(args[++i].c_str());
        if (runs < 1) {
          std::fprintf(stderr, "error: 'nusa bench' needs -n >= 1\n");
          return 1;
        }
      } else if (!dashdash && !args[i].empty() && args[i][0] == '-') {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      } else {
        pargs.push_back(args[i]);
      }
    }
    return driver.runBench(input, runs, pargs);
  }
  if (args[0] == "clean") {
    // Artefak tergenerasi (Phase 62): default direktori kerja.
    std::string dir = ".";
    for (size_t i = 1; i < args.size(); i++) {
      if (!args[i].empty() && args[i][0] == '-') {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      } else if (dir == ".") {
        dir = args[i];
      } else {
        std::fprintf(stderr, "error: unexpected argument '%s'\n",
                     args[i].c_str());
        return 1;
      }
    }
    return driver.runClean(dir);
  }
  if (args[0] == "test") {
    std::vector<std::string> paths(args.begin() + 1, args.end());
    return driver.runTest(paths);
  }
  if (args[0] == "fmt") {
    bool checkOnly = false;
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i] == "--check") {
        checkOnly = true;
      } else if (!args[i].empty() && args[i][0] == '-') {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      } else {
        paths.push_back(args[i]);
      }
    }
    return driver.runFmt(paths, checkOnly);
  }
  if (args[0] == "doc") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa doc' needs a file.ns argument\n");
      return 1;
    }
    std::string input = args[1];
    std::string output;
    std::string format = "markdown";
    for (size_t i = 2; i < args.size(); i++) {
      if (args[i] == "-o" && i + 1 < args.size()) {
        output = args[++i];
      } else if (args[i] == "--format" && i + 1 < args.size()) {
        format = args[++i];
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    return driver.runDoc(input, output, format);
  }
  if (args[0] == "pkg") {
    std::vector<std::string> pargs(args.begin() + 1, args.end());
    return nusa::pkg::runPkg(driver, pargs);
  }
  // Project templates (Phase 43): nusa new <template> [name].
  if (args[0] == "new") {
    std::vector<std::string> pargs(args.begin() + 1, args.end());
    return nusa::pkg::runNew(pargs);
  }
  if (args[0] == "lsp") {
    return nusa::lsp::run();
  }
  // Debug lexer (Phase 38): dump token untuk differential test
  // melawan selfhost/lex.ns. Format: kind|lexeme|line|col.
  if (args[0] == "lex") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa lex' needs a file.ns argument\n");
      return 1;
    }
    return driver.runLex(args[1]);
  }
  // Debug parser (Phase 72): dump AST untuk differential test
  // melawan selfhost/parse.ns. Format: s-expr satu baris.
  if (args[0] == "ast") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa ast' needs a file.ns argument\n");
      return 1;
    }
    return driver.runAst(args[1]);
  }
  if (args[0] == "bindgen") {
    // Binding C header → extern fn (Phase 66, subset).
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa bindgen' needs a header.h argument\n");
      return 1;
    }
    std::string input = args[1];
    std::string output;
    for (size_t i = 2; i < args.size(); i++) {
      if (args[i] == "-o" && i + 1 < args.size()) {
        output = args[++i];
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    return driver.runBindgen(input, output);
  }
  if (args[0] == "headers") {
    // Header C dari API .ns (Phase 110, subset jujur-ABI).
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa headers' needs a file.ns argument\n");
      return 1;
    }
    std::string input = args[1];
    std::string output;
    for (size_t i = 2; i < args.size(); i++) {
      if (args[i] == "-o" && i + 1 < args.size()) {
        output = args[++i];
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    return driver.runHeaders(input, output);
  }
  // Rantai bootstrap (Phase 39): Stage 0 -> Stage 1 -> Stage 2 +
  // verifikasi reproduksibilitas. Lihat docs/bootstrap.md.
  if (args[0] == "bootstrap") {
    std::string lexNs;
    std::string outDir;
    std::string asKind = "lex";
    std::string rebuildDir;
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i] == "-o" && i + 1 < args.size()) {
        outDir = args[++i];
      } else if (args[i] == "--rebuild" && i + 1 < args.size()) {
        rebuildDir = args[++i];
      } else if (args[i] == "--as" && i + 1 < args.size()) {
        asKind = args[++i];
        if (asKind != "lex" && asKind != "ast") {
          std::fprintf(stderr, "error: unknown option value '--as %s'\n",
                       asKind.c_str());
          std::fprintf(stderr, "help: use --as lex|ast\n");
          return 1;
        }
      } else if (!args[i].empty() && args[i][0] == '-') {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      } else if (lexNs.empty()) {
        lexNs = args[i];
      } else {
        std::fprintf(stderr, "error: unexpected argument '%s'\n",
                     args[i].c_str());
        return 1;
      }
    }
    if (lexNs.empty()) {
      // Default: selfhost/lex.ns di samping instalasi (<exe>/../selfhost)
      // atau cwd (pengembangan).
      const char *cands[] = {"selfhost/lex.ns", "../selfhost/lex.ns",
                             nullptr};
      // Cari relatif terhadap exe dulu via /proc/self/exe (Linux/Android).
      char exeBuf[4096];
      ssize_t n =
          readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
      std::string exeDir;
      if (n > 0) {
        exeBuf[n] = '\0';
        std::string p(exeBuf);
        size_t s = p.find_last_of('/');
        if (s != std::string::npos)
          exeDir = p.substr(0, s);
      }
      std::vector<std::string> tried;
      if (!exeDir.empty())
        tried.push_back(exeDir + "/../selfhost/lex.ns");
      for (const char **c = cands; *c; c++)
        tried.push_back(*c);
      for (const auto &t : tried) {
        std::ifstream f(t, std::ios::binary);
        if (f) {
          lexNs = t;
          break;
        }
      }
      if (lexNs.empty()) {
        std::fprintf(stderr,
                     "error: cannot find selfhost/lex.ns (tried:");
        for (const auto &t : tried)
          std::fprintf(stderr, " %s", t.c_str());
        std::fprintf(stderr, ")\nhelp: pass it explicitly: "
                             "nusa bootstrap <path>/lex.ns\n");
        return 1;
      }
    }
    return driver.runBootstrapKind(lexNs, outDir, asKind, rebuildDir);
  }
  // Top-level registry CLI (Phase 31): search/install/publish.
  // Inspection CLI (Phase 60): tree/verify/outdated/audit.
  // Supply-chain (Phase 113): sbom.
  if (args[0] == "search" || args[0] == "install" ||
      args[0] == "publish" || args[0] == "tree" || args[0] == "verify" ||
      args[0] == "outdated" || args[0] == "audit" || args[0] == "sbom") {
    std::vector<std::string> pargs;
    pargs.push_back(args[0]);
    pargs.insert(pargs.end(), args.begin() + 1, args.end());
    return nusa::pkg::runPkg(driver, pargs);
  }
  // Sandbox tak-terpercaya (Phase 42): lihat docs/playground.md.
  if (args[0] == "sandbox") {
    if (args.size() < 2) {
      std::fprintf(stderr, "error: 'nusa sandbox' needs a file.ns argument\n");
      return 1;
    }
    std::string input = args[1];
    int timeoutSec = 10;
    long memoryMb = 256;
    long outputKb = 64;
    for (size_t i = 2; i < args.size(); i++) {
      if (args[i] == "--timeout" && i + 1 < args.size()) {
        timeoutSec = atoi(args[++i].c_str());
      } else if (args[i] == "--memory-mb" && i + 1 < args.size()) {
        memoryMb = atol(args[++i].c_str());
      } else if (args[i] == "--output-kb" && i + 1 < args.size()) {
        outputKb = atol(args[++i].c_str());
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    if (timeoutSec <= 0 || memoryMb <= 0 || outputKb <= 0) {
      std::fprintf(stderr, "error: limits must be positive numbers\n");
      return 1;
    }
    return driver.runSandbox(input, timeoutSec, memoryMb, outputKb);
  }
  // Playground HTTP loopback (Phase 42).
  if (args[0] == "playground") {
    int port = 8080;
    int timeoutSec = 5;
    long memoryMb = 256;
    long outputKb = 64;
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i] == "--port" && i + 1 < args.size()) {
        port = atoi(args[++i].c_str());
      } else if (args[i] == "--timeout" && i + 1 < args.size()) {
        timeoutSec = atoi(args[++i].c_str());
      } else if (args[i] == "--memory-mb" && i + 1 < args.size()) {
        memoryMb = atol(args[++i].c_str());
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    if (port < 0 || port > 65535 || timeoutSec <= 0 || memoryMb <= 0) {
      std::fprintf(stderr, "error: bad playground limits\n");
      return 1;
    }
    return driver.runPlayground(port, timeoutSec, memoryMb, outputKb);
  }
  if (args[0] == "plugin") {
    // Plugin tersandbox v1 (Phase 112): subcommand `run` saja.
    if (args.size() < 2 || args[1] != "run") {
      std::fprintf(stderr,
                   "error: usage: nusa plugin run <file.ns> [--manifest "
                   "m.json] [--timeout S] [--memory-mb M] [--output-kb K]\n");
      return 1;
    }
    if (args.size() < 3) {
      std::fprintf(stderr, "error: 'nusa plugin run' needs a file.ns argument\n");
      return 1;
    }
    std::string input = args[2];
    std::string manifest;
    int timeoutSec = 10;
    long memoryMb = 256;
    long outputKb = 64;
    for (size_t i = 3; i < args.size(); i++) {
      if (args[i] == "--manifest" && i + 1 < args.size()) {
        manifest = args[++i];
      } else if (args[i] == "--timeout" && i + 1 < args.size()) {
        timeoutSec = atoi(args[++i].c_str());
      } else if (args[i] == "--memory-mb" && i + 1 < args.size()) {
        memoryMb = atol(args[++i].c_str());
      } else if (args[i] == "--output-kb" && i + 1 < args.size()) {
        outputKb = atol(args[++i].c_str());
      } else {
        std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
        return 1;
      }
    }
    if (timeoutSec <= 0 || memoryMb <= 0 || outputKb <= 0) {
      std::fprintf(stderr, "error: limits must be positive numbers\n");
      return 1;
    }
    // Request = seluruh stdin (piped JSON).
    std::string request;
    {
      std::ostringstream ss;
      ss << std::cin.rdbuf();
      request = ss.str();
    }
    return driver.runPlugin(input, manifest, timeoutSec, memoryMb, outputKb,
                            request);
  }
  std::fprintf(stderr, "error: unknown command '%s'\n", args[0].c_str());
  printHelp();
  return 1;
}
