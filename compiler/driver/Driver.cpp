#include "compiler/driver/Driver.h"
#include "compiler/ast/AST.h"
#include "compiler/incr/Incr.h"
#include <llvm/TargetParser/Host.h>
#include "compiler/codegen/Codegen.h"
#include "compiler/bindgen/Bindgen.h"
#include "compiler/bindgen/Headers.h"
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/doc/DocGen.h"
#include "compiler/driver/Modules.h"
#include "compiler/fmt/Formatter.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
#include "compiler/semantic/Semantic.h"
#include "compiler/semantic/TypeChecker.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

namespace nusa {

const char *Driver::versionString() {
  static std::string cached =
      std::string("nusa version ") + NUSA_VERSION +
      " (LLVM 21.1.8, AArch64 Android/Termux)";
  return cached.c_str();
}

std::string defaultOutputFor(const std::string &inputPath) {
  // hello.ns -> ./hello ; main.ns -> ./main
  std::string base = inputPath;
  // strip direktori
  size_t slash = base.find_last_of("/\\");
  if (slash != std::string::npos)
    base = base.substr(slash + 1);
  // strip ekstensi
  size_t dot = base.rfind('.');
  if (dot != std::string::npos)
    base = base.substr(0, dot);
  if (base.empty())
    base = "a.out";
  return "./" + base;
}

std::string readFileOrEmpty(const std::string &path, bool &ok) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    ok = false;
    return "";
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  ok = true;
  return ss.str();
}

static bool hasNsExtension(const std::string &path) {
  if (path.size() < 3)
    return false;
  return path.compare(path.size() - 3, 3, ".ns") == 0;
}

static std::string shellQuote(const std::string &s) {
  std::string o = "'";
  for (char c : s) {
    if (c == '\'')
      o += "'\\''";
    else
      o.push_back(c);
  }
  o += "'";
  return o;
}

std::string Driver::hostTriple() {
  return llvm::sys::getDefaultTargetTriple();
}

bool Driver::resolveProfile(const std::string &arg, std::string &opt,
                            bool &debug, std::string &err) {
  // Bundel flag bernama (Phase 62, docs/build-2.13.md). Flag eksplisit
  // di CLI menang (diterapkan pemanggil sesudah ini).
  if (arg == "dev") {
    opt = "0";
    debug = false;
    return true;
  }
  if (arg == "debug") {
    opt = "0";
    debug = true;
    return true;
  }
  if (arg == "release") {
    opt = "2";
    debug = false;
    return true;
  }
  err = "unknown profile '" + arg +
        "'. Supported: dev (O0), debug (O0+g), release (O2)";
  return false;
}

int Driver::runClean(const std::string &dir) {
  // Artefak tergenerasi (Phase 62): *.ns.ll + .nusa-cache/ rekursif,
  // target/ + dist/ top-level. Sumber/manifest/lock tak tersentuh.
  namespace fs = std::filesystem;
  std::error_code ec;
  std::string root = dir.empty() ? "." : dir;
  if (!fs::is_directory(root, ec) || ec) {
    std::fprintf(stderr, "error: not a directory '%s'\n", root.c_str());
    return 1;
  }
  int removed = 0;
  auto it = fs::recursive_directory_iterator(
      root, fs::directory_options::skip_permission_denied, ec);
  auto end = fs::recursive_directory_iterator();
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code ec2;
    std::string fn = it->path().filename().string();
    if (it->is_directory(ec2) && !ec2) {
      bool top = (it.depth() == 0);
      if (fn == ".nusa-cache" || (top && (fn == "target" || fn == "dist"))) {
        fs::remove_all(it->path(), ec2);
        if (!ec2) {
          removed++;
          it.disable_recursion_pending();
        }
      }
    } else if (!ec2 && fn.size() > 6 &&
               fn.compare(fn.size() - 6, 6, ".ns.ll") == 0) {
      fs::remove(it->path(), ec2);
      if (!ec2)
        removed++;
    }
  }
  if (ec) {
    std::fprintf(stderr, "error: clean walk failed in '%s'\n", root.c_str());
    return 1;
  }
  std::printf("Cleaned %s (%d removed)\n", root.c_str(), removed);
  return 0;
}

// Subsistem runtime Nusa -> nama kapabilitas (Phase 111, WASI gate).
// Daftar prefix CEMIN needRt di runBuild (simbol `@nusa_*`): positif
// di sini = program memakai kapabilitas tersebut.
static const struct {
  const char *prefix;
  const char *cap;
} kRtCaps[] = {
    {"@nusa_thread_", "threads"}, {"@nusa_mutex_", "mutexes"},
    {"@nusa_chan_", "channels"},  {"@nusa_atomic_", "atomics"},
    {"@nusa_sleep_", "sleep"},     {"@nusa_time_", "time"},
    {"@nusa_random_", "random"},   {"@nusa_process_", "processes"},
    {"@nusa_tcp_", "networking"},  {"@nusa_sha256_", "crypto"},
    {"@nusa_crypto_", "crypto"},
};

// Pindai teks IR -> daftar kapabilitas terpakai (unik, urutan tabel).
static std::vector<std::string> usedRtCaps(const std::string &ll) {
  std::vector<std::string> out;
  for (const auto &e : kRtCaps) {
    if (ll.find(e.prefix) == std::string::npos)
      continue;
    bool dup = false;
    for (const auto &c : out) {
      if (c == e.cap) {
        dup = true;
        break;
      }
    }
    if (!dup)
      out.push_back(e.cap);
  }
  return out;
}

// Phase 83: audit migrasi 2.x -> 3.0 (lihat docs/migration-3.0.md).
// Read-only: pipeline check per file + pindai marker + manifest native.
// Klasifikasi jujur: check hijau = compatible (+deprecated bila pakai
// permukaan legacy); check gagal + idiom 1.x = requires migration;
// check gagal lain = broken; paket native=true = unsupported.
namespace migrate {
namespace fs = std::filesystem;

static bool hasNsExtension(const std::string &p) {
  return p.size() > 3 && p.compare(p.size() - 3, 3, ".ns") == 0;
}

// Hapus komentar-// dan literal-string agar pindai marker tak
// false-positive pada teks dokumentasi/contoh.
static std::string stripCommentsStrings(const std::string &src) {
  std::string out;
  out.reserve(src.size());
  size_t i = 0;
  while (i < src.size()) {
    if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
      while (i < src.size() && src[i] != '\n')
        i++;
    } else if (src[i] == '"') {
      out += ' ';
      i++;
      while (i < src.size() && src[i] != '"') {
        if (src[i] == '\\' && i + 1 < src.size())
          i += 2;
        else if (src[i] == '\n')
          break;
        else
          i++;
      }
      if (i < src.size())
        i++;
    } else {
      out += src[i++];
    }
  }
  return out;
}

static bool hasLegacyChan(const std::string &code) {
  static const std::regex re(
      "(^|[^A-Za-z0-9_])chan_(new|send|recv|free)\\s*\\(");
  return std::regex_search(code, re);
}

static bool hasOldIdiom(const std::string &code) {
  static const std::regex re(
      "(\\bnone\\s*\\(|\\bok\\s*\\(|\\berr\\s*\\(|"
      "\\.(value|has_value|is_ok)\\b|"
      "\\bstruct\\s+(Option|Result)\\b|"
      "\\b(async|await|macro)\\b)");
  return std::regex_search(code, re);
}

// True bila file berada di paket bermanifest native=true
// (menelusuri ke atas maks 4 level dari direktori file).
static bool inNativePackage(const std::string &filePath) {
  fs::path dir = fs::path(filePath).parent_path();
  for (int up = 0; up < 4 && !dir.empty(); up++) {
    bool ok = false;
    std::string toml =
        readFileOrEmpty((dir / "nusa.toml").string(), ok);
    if (ok) {
      // Tanpa flag (?m) (tak didukung ECMAScript): jangkar manual.
      static const std::regex re("(^|\n)\\s*native\\s*=\\s*true(\\s|#|$)");
      if (std::regex_search(toml, re))
        return true;
    }
    if (dir.has_parent_path())
      dir = dir.parent_path();
    else
      break;
  }
  return false;
}

// Jalankan checkStages dengan stderr ditangkap (diagnostik milik
// file ini saja, bukan tercampur dengan laporan migrate).
static int checkQuiet(Driver &drv, const std::string &path,
                      std::string &diag) {
  std::fflush(nullptr);
  int saved = dup(2);
  if (saved < 0)
    return 1;
  FILE *tmp = std::tmpfile();
  if (!tmp) {
    close(saved);
    return 1;
  }
  int tfd = fileno(tmp);
  if (dup2(tfd, 2) < 0) {
    std::fclose(tmp);
    close(saved);
    return 1;
  }
  Program merged;
  int rc = drv.checkStages(path, merged);
  std::fflush(nullptr);
  dup2(saved, 2);
  close(saved);
  std::rewind(tmp);
  diag.clear();
  char buf[4096];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof buf, tmp)) > 0)
    diag.append(buf, n);
  std::fclose(tmp);
  return rc;
}

static bool isLibraryDiag(const std::string &diag) {
  return diag.find("no 'fn main' found") != std::string::npos;
}

} // namespace migrate

int Driver::runMigrate(const std::string &target) {
  namespace fs = std::filesystem;
  std::string root = target.empty() ? "." : target;
  std::error_code ec;
  std::vector<std::string> files;
  if (fs::is_regular_file(root, ec) && !ec) {
    if (root.size() < 3 || root.compare(root.size() - 3, 3, ".ns") != 0) {
      std::fprintf(stderr, "error: input file must have '.ns' extension: %s\n",
                   root.c_str());
      return 1;
    }
    files.push_back(root);
  } else if (fs::is_directory(root, ec) && !ec) {
    auto it = fs::recursive_directory_iterator(
        root, fs::directory_options::skip_permission_denied, ec);
    auto end = fs::recursive_directory_iterator();
    for (; !ec && it != end; it.increment(ec)) {
      std::error_code ec2;
      if (it->is_directory(ec2) && !ec2) {
        std::string fn = it->path().filename().string();
        if (fn == "build" || fn == "website" || fn == "target" ||
            fn == "dist" || fn == ".nusa-cache" || fn == ".git")
          it.disable_recursion_pending();
        continue;
      }
      if (ec2)
        continue;
      std::string p = it->path().string();
      if (migrate::hasNsExtension(p))
        files.push_back(p);
    }
    if (ec) {
      std::fprintf(stderr, "error: migrate walk failed in '%s'\n",
                   root.c_str());
      return 1;
    }
    std::sort(files.begin(), files.end());
  } else {
    std::fprintf(stderr, "error: no such file or directory '%s'\n",
                 root.c_str());
    return 1;
  }

  long nCompat = 0, nDeprec = 0, nMig = 0, nBroken = 0, nUnsup = 0;
  for (const auto &f : files) {
    bool ok = false;
    std::string src = readFileOrEmpty(f, ok);
    if (!ok) {
      std::printf("%s: broken (unreadable)\n", f.c_str());
      nBroken++;
      continue;
    }
    std::string code = migrate::stripCommentsStrings(src);
    std::string diag;
    int rc = migrate::checkQuiet(*this, f, diag);
    std::string cat, note;
    if (rc == 0) {
      note = "program";
      if (migrate::inNativePackage(f)) {
        cat = "unsupported";
        nUnsup++;
      } else if (migrate::hasLegacyChan(code)) {
        cat = "deprecated";
        nDeprec++;
      } else {
        cat = "compatible";
        nCompat++;
      }
    } else if (migrate::isLibraryDiag(diag)) {
      note = "library";
      if (migrate::inNativePackage(f)) {
        cat = "unsupported";
        nUnsup++;
      } else if (migrate::hasLegacyChan(code)) {
        cat = "deprecated";
        nDeprec++;
      } else {
        cat = "compatible";
        nCompat++;
      }
    } else if (migrate::hasOldIdiom(code)) {
      cat = "requires migration";
      note = "check-failed";
      nMig++;
    } else {
      cat = "broken";
      note = "check-failed";
      nBroken++;
    }
    std::printf("%s: %s (%s)\n", f.c_str(), cat.c_str(), note.c_str());
  }
  std::printf("migrate: %ld compatible, %ld deprecated, %ld requires "
              "migration, %ld broken, %ld unsupported (%lu files)\n",
              nCompat, nDeprec, nMig, nBroken, nUnsup, files.size());
  return (nMig > 0 || nBroken > 0) ? 1 : 0;
}

int Driver::runBindgen(const std::string &inputPath,
                       const std::string &outputPath) {
  if (inputPath.size() < 3 ||
      inputPath.compare(inputPath.size() - 2, 2, ".h") != 0) {
    std::fprintf(stderr, "error: input file must have '.h' extension: %s\n",
                 inputPath.c_str());
    return 1;
  }
  bool ok = false;
  std::string source = readFileOrEmpty(inputPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read file '%s'\n", inputPath.c_str());
    return 1;
  }
  bindgen::BindResult r = bindgen::generateBindings(inputPath, source);
  for (const auto &w : r.warnings)
    std::fprintf(stderr, "warning: %s\n", w.c_str());
  if (!r.ok) {
    std::fprintf(stderr, "error: %s: %s\n", inputPath.c_str(),
                 r.error.c_str());
    return 1;
  }
  if (outputPath.empty()) {
    std::fwrite(r.code.c_str(), 1, r.code.size(), stdout);
  } else {
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
      std::fprintf(stderr, "error: cannot write file '%s'\n",
                   outputPath.c_str());
      return 1;
    }
    out << r.code;
    out.close();
    if (!out) {
      std::fprintf(stderr, "error: cannot write file '%s'\n",
                   outputPath.c_str());
      return 1;
    }
    std::fprintf(stderr, "Generated %s -> %s\n", inputPath.c_str(),
                 outputPath.c_str());
  }
  return 0;
}

int Driver::runHeaders(const std::string &inputPath,
                       const std::string &outputPath) {
  if (inputPath.size() < 4 ||
      inputPath.compare(inputPath.size() - 3, 3, ".ns") != 0) {
    std::fprintf(stderr, "error: input file must have '.ns' extension: %s\n",
                 inputPath.c_str());
    return 1;
  }
  bool ok = false;
  std::string source = readFileOrEmpty(inputPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read file '%s'\n",
                 inputPath.c_str());
    return 1;
  }
  headers::HeaderResult r = headers::generateHeader(inputPath, source);
  for (const auto &w : r.warnings)
    std::fprintf(stderr, "warning: %s\n", w.c_str());
  if (!r.ok) {
    std::fprintf(stderr, "error: %s: %s\n", inputPath.c_str(),
                 r.error.c_str());
    return 1;
  }
  if (outputPath.empty()) {
    std::fwrite(r.code.c_str(), 1, r.code.size(), stdout);
  } else {
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
      std::fprintf(stderr, "error: cannot write file '%s'\n",
                   outputPath.c_str());
      return 1;
    }
    out << r.code;
    out.close();
    if (!out) {
      std::fprintf(stderr, "error: cannot write file '%s'\n",
                   outputPath.c_str());
      return 1;
    }
    std::fprintf(stderr, "Generated %s -> %s\n", inputPath.c_str(),
                 outputPath.c_str());
  }
  return 0;
}

bool Driver::resolveTarget(const std::string &arg, ResolvedTarget &out,
                           std::string &err) {
  // Matriks target (Phase 33, docs/targets.md; wasm32 Phase 67):
  // LP64 ELF + wasm32 bare-metal (ILP32: komputasi murni saja,
  // lihat docs/wasm-2.18.md). Windows/macOS dievaluasi-ditolak.
  struct Entry {
    const char *alias;
    const char *triple;
  };
  static const Entry supported[] = {
      {"android-aarch64", nullptr}, // triple = host clang ini
      {"linux-aarch64", "aarch64-linux-gnu"},
      {"linux-x86_64", "x86_64-linux-gnu"},
      {"wasm32", "wasm32-unknown-unknown"},
  };
  std::string host = hostTriple();
  for (const auto &e : supported) {
    if (arg == e.alias) {
      out.triple = e.triple ? e.triple : host;
      out.isHost = (e.triple == nullptr) || (out.triple == host);
      return true;
    }
  }
  // Triple mentah yang cocok salah satu entri (tanpa alias).
  for (const auto &e : supported) {
    if (e.triple && arg == e.triple) {
      out.triple = arg;
      out.isHost = (arg == host);
      return true;
    }
  }
  if (arg == host) {
    out.triple = host;
    out.isHost = true;
    return true;
  }
  err = "unknown target '" + arg +
        "'. Supported: android-aarch64 (host), linux-aarch64, "
        "linux-x86_64, wasm32 (plus their triples). Windows/macOS are "
        "evaluated but unsupported (see docs/targets.md)";
  return false;
}

// Lokasi runtime C (Phase 28): `<exe>/../runtime/nusa_rt.c` (repo-dev:
// `build/../runtime`; instalasi: `bin/../runtime`), lalu
// `./runtime/nusa_rt.c` (cwd). "" bila tak ketemu (link tanpa runtime;
// program tanpa builtin concurrency tetap jalan).
// Objek runtime precompiled (Phase 34): `<exe>/nusa_rt.o` (sisi binary;
// ditaruh CMake), lalu `./nusa_rt.o` (cwd). "" bila nihil (fallback
// ke kompilasi runtime/nusa_rt.c per build).
static std::string findRuntimeObject() {
  std::string exe = ModuleLoader::exeDir();
  if (!exe.empty()) {
    std::string cand = exe + "/nusa_rt.o";
    std::ifstream f(cand);
    if (f.good())
      return cand;
  }
  {
    std::ifstream f("./nusa_rt.o");
    if (f.good())
      return "./nusa_rt.o";
  }
  return "";
}

static std::string findRuntimeFile() {  const char *name = "runtime/nusa_rt.c";
  std::string exeStd = ModuleLoader::exeStdDir(); // `<exe>/../std`
  if (!exeStd.empty()) {
    std::string cand = exeStd + "/../runtime/nusa_rt.c";
    std::ifstream f(cand);
    if (f.good())
      return cand;
  }
  {
    std::string cand = std::string("./") + name;
    std::ifstream f(cand);
    if (f.good())
      return cand;
  }
  if (const char *np = std::getenv("NUSA_PATH")) {
    std::string paths(np);
    size_t start = 0;
    while (start <= paths.size()) {
      size_t sep = paths.find(':', start);
      std::string dir =
          paths.substr(start, sep == std::string::npos ? sep : sep - start);
      if (!dir.empty()) {
        std::string cand = dir + "/" + name;
        std::ifstream f(cand);
        if (f.good())
          return cand;
      }
      if (sep == std::string::npos)
        break;
      start = sep + 1;
    }
  }
  return "";
}

int Driver::checkStages(const std::string &inputPath, Program &merged,
                         std::vector<LoadedModule> *modulesOut) {
  if (!hasNsExtension(inputPath)) {
    std::fprintf(stderr, "error: input file must have '.ns' extension: %s\n",
                 inputPath.c_str());
    return 1;
  }
  // 1-2. Load + Lex + Parse semua modul transitif (diagnostic per file).
  // (File dibaca oleh ModuleLoader; error baca dilaporkan dengan konteks
  // modul pengimpor.)
  ModuleLoader loader;
  std::vector<LoadedModule> modules;
  std::vector<ModuleError> loadErrors;
  bool loaded = loader.load(inputPath, merged, modules, loadErrors);
  // Engine diagnostic per file (snippet/caret dari file yang tepat).
  std::unordered_map<std::string, DiagnosticEngine> diags;
  for (auto &m : modules)
    diags.emplace(m.display, DiagnosticEngine(m.lines));
  auto reportModuleError = [&](const ModuleError &e) {
    if (e.verbatim) {
      std::fprintf(stderr, "%s\n", e.message.c_str());
      return;
    }
    auto it = diags.find(e.loc.file);
    if (it == diags.end()) {
      std::fprintf(stderr, "%s:%d:%d: error: %s\n", e.loc.file.c_str(),
                   e.loc.line, e.loc.column, e.message.c_str());
      if (!e.hint.empty())
        std::fprintf(stderr, "help: %s\n", e.hint.c_str());
      return;
    }
    it->second.reportParseError(
        ParseError{e.loc, e.message, e.hint, e.length});
  };
  for (const auto &e : loadErrors)
    reportModuleError(e);
  if (!loaded)
    return 1;

  // 3. Semantic analysis (namespaces, scopes, structs) — program gabungan.
  int stageErrors = 0;
  SemanticAnalyzer sema;
  for (const auto &e : sema.analyze(merged)) {
    reportModuleError(
        ModuleError{e.loc, e.message, e.hint, false, e.length});
    stageErrors++;
  }
  if (stageErrors)
    return 1;

  // 4. Type checking (anotasi, kompatibilitas, inferensi, struct, calls)
  TypeChecker checker;
  checker.check(merged);
  for (const auto &e : checker.errors()) {
    reportModuleError(
        ModuleError{e.loc, e.message, e.hint, false, e.length});
    stageErrors++;
  }
  if (stageErrors)
    return 1;
  if (modulesOut)
    *modulesOut = std::move(modules);
  return 0;
}

int Driver::runCheck(const std::string &inputPath) {
  Program merged;
  int rc = checkStages(inputPath, merged);
  if (rc != 0)
    return rc;
  std::printf("Checked %s: no errors\n", inputPath.c_str());
  return 0;
}

// Phase 119 (3.20.0): `nusa trace run` — bangkitkan TRACE_ID
// (ms + acak, cermin std/trace) lalu run biasa dengan env tersebut
// agar seluruh span/event program berbagi ID (propagasi).
int Driver::runTraceRun(const std::string &inputPath,
                        const std::vector<std::string> &args) {
  auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
  // Acak murah dari /dev/urandom (gagal = fallback counter waktu).
  unsigned long long rnd = (unsigned long long)nowMs * 2654435761ULL;
  FILE *rf = std::fopen("/dev/urandom", "rb");
  if (rf) {
    unsigned long long v = 0;
    if (std::fread(&v, 1, sizeof(v), rf) == sizeof(v))
      rnd ^= v;
    std::fclose(rf);
  }
  char idbuf[64];
  std::snprintf(idbuf, sizeof(idbuf), "tr-%lld-%llx",
                (long long)nowMs, rnd & 0xffffffffULL);
  std::string tmpExe = inputPath + ".run.out";
  int rc = runBuild(inputPath, tmpExe);
  if (rc != 0)
    return rc;
  std::string execPath = tmpExe;
  if (execPath.find('/') == std::string::npos)
    execPath = "./" + execPath;
  // Injeksi env via prefix shell (cermin runRun + shellQuote).
  std::string cmd =
      std::string("TRACE_ID=") + shellQuote(idbuf) + " " + shellQuote(execPath);
  for (const auto &a : args)
    cmd += " " + shellQuote(a);
  std::fprintf(stderr, "trace: request id %s\n", idbuf);
  int sysRc = std::system(cmd.c_str());
  std::remove(tmpExe.c_str());
  if (sysRc == -1)
    return 1;
  if (WIFEXITED(sysRc))
    return WEXITSTATUS(sysRc);
  return 1;
}

// Phase 119: `nusa trace check` — validasi log trace nusa (satu objek
// per baris, "type" pertama). BUKAN validator JSON umum: kunci =
// bentuk yang diemisikan std/trace (terdokumentasi).
int Driver::runTraceCheck(const std::string &logPath) {
  bool ok = false;
  std::string data = readFileOrEmpty(logPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read file '%s'\n", logPath.c_str());
    return 1;
  }
  long nLine = 0, nBegin = 0, nEnd = 0, nEvent = 0, nBad = 0;
  std::string firstBad;
  size_t pos = 0;
  auto needs = [&](const std::string &line, const char *k) {
    return line.find(std::string("\"") + k + "\":") != std::string::npos;
  };
  while (pos <= data.size()) {
    size_t nl = data.find('\n', pos);
    std::string line =
        (nl == std::string::npos) ? data.substr(pos) : data.substr(pos, nl - pos);
    pos = (nl == std::string::npos) ? data.size() + 1 : nl + 1;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;
    nLine++;
    const char *kind = nullptr;
    if (line.rfind("{\"type\":\"begin\",", 0) == 0)
      kind = "begin";
    else if (line.rfind("{\"type\":\"end\",", 0) == 0)
      kind = "end";
    else if (line.rfind("{\"type\":\"event\",", 0) == 0)
      kind = "event";
    bool good = (kind != nullptr) && needs(line, "id") && needs(line, "op");
    if (good) {
      if (!strcmp(kind, "begin"))
        good = needs(line, "t");
      else if (!strcmp(kind, "end"))
        good = needs(line, "ms");
      else
        good = needs(line, "status") && needs(line, "note");
    }
    if (!good) {
      nBad++;
      if (firstBad.empty())
        firstBad = "line " + std::to_string(nLine);
      continue;
    }
    if (!strcmp(kind, "begin"))
      nBegin++;
    else if (!strcmp(kind, "end"))
      nEnd++;
    else
      nEvent++;
  }
  std::printf("trace: %ld lines (%ld begin, %ld end, %ld event), %ld invalid",
              nLine, nBegin, nEnd, nEvent, nBad);
  if (nBad > 0)
    std::printf(" (first: %s)", firstBad.c_str());
  std::printf("\n");
  return nBad > 0 ? 1 : 0;
}

// Fingerprint build saat ini (Phase 35) dari flag + modul + IR.
// pgo: "" | "generate" | "use:<sha-profdata>" (Phase 104 — mode PGO
// adalah flag: biner instrumented vs teroptimasi tak boleh
// tertukar cache).
static incr::Fingerprint
currentFingerprint(const std::string &triple, const std::string &optLevel,
                   const std::string &emit, const std::string &sysroot,
                   const LinkSpec &links,
                   const std::vector<LoadedModule> &modules,
                   const std::string &llPath, bool debug,
                   const std::string &pgo, const std::string &outPath) {
  incr::Fingerprint cur;
  cur.compiler = incr::shaCompiler();
  cur.target = triple;
  cur.opt = optLevel;
  cur.emit = emit;
  cur.sysroot = sysroot;
  cur.debug = debug;
  cur.pgo = pgo;
  cur.out = outPath;
  for (const auto &f : links.files)
    cur.links.push_back("file:" + f + ":" + incr::shaFile(f));
  for (const auto &d : links.libDirs)
    cur.links.push_back("dir:" + d);
  for (const auto &l : links.libs)
    cur.links.push_back("lib:" + l);
  std::sort(cur.links.begin(), cur.links.end());
  cur.modules = incr::buildGraph(modules);
  cur.ir = incr::shaFile(llPath);
  return cur;
}

int Driver::runBuild(const std::string &inputPath,
                     const std::string &outputPathArg,
                     const LinkSpec &links, const std::string &target,
                     const std::string &sysroot, const std::string &emit,
                     const std::string &optLevel, bool fresh, bool debug,
                     bool stats, bool pgoGenerate,
                     const std::string &pgoUse) {
  // PGO (Phase 104, milestone 1): validasi dini sebelum kerja mahal.
  // `--profile` sudah milik Phase 62 → namespace `--pgo-*` (stabilitas CLI).
  if (pgoGenerate && !pgoUse.empty()) {
    std::fprintf(stderr, "error: --pgo-generate and --pgo-use are exclusive "
                         "(generate a profile first, then consume it)\n");
    return 1;
  }
  std::string pgoSha;
  if (!pgoUse.empty()) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_regular_file(pgoUse, ec) || ec) {
      std::fprintf(stderr, "error: PGO profile '%s' not found\n",
                   pgoUse.c_str());
      std::fprintf(stderr, "help: build instrumented (--pgo-generate), run "
                           "the workload, merge with "
                           "`llvm-profdata merge -o out.profdata *.profraw`\n");
      return 1;
    }
    pgoSha = incr::shaFile(pgoUse);
  }
  std::string pgoFp = pgoGenerate ? "generate"
                                  : (pgoUse.empty() ? "" : "use:" + pgoSha);
  auto nowMs = [] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  };
  long long t0 = nowMs();
  Program merged;
  std::vector<LoadedModule> modules;
  int rc = checkStages(inputPath, merged, &modules);
  if (rc != 0)
    return rc;
  long long tFront = nowMs();

  // Target cross-compilation (Phase 33).
  std::string triple;
  bool isHost = true;
  if (!target.empty()) {
    ResolvedTarget rt;
    std::string terr;
    if (!resolveTarget(target, rt, terr)) {
      std::fprintf(stderr, "error: %s\n", terr.c_str());
      return 1;
    }
    triple = rt.triple;
    isHost = rt.isHost;
  }
  if (emit != "" && emit != "ir" && emit != "asm" && emit != "obj") {
    std::fprintf(stderr, "error: unknown emit mode '%s'\n", emit.c_str());
    return 1;
  }
  // Link cross tanpa sysroot pasti gagal di ld — tolak dini dengan
  // arahan (bukan dump linker mentah).
  if (!isHost && emit.empty() && sysroot.empty()) {
    std::fprintf(stderr,
                 "error: linking for target '%s' needs --sysroot <dir>\n",
                 triple.c_str());
    std::fprintf(stderr, "help: compile-only with -c / --emit-ir / "
                         "--emit-asm, or pass a target sysroot\n");
    return 1;
  }

  // Nama output dulu (dibutuhkan cacheDir di bawah).
  auto stemOf = [](const std::string &p) {
    size_t slash = p.find_last_of("/\\");
    std::string base =
        (slash == std::string::npos) ? p : p.substr(slash + 1);
    size_t dot = base.rfind('.');
    if (dot != std::string::npos)
      base = base.substr(0, dot);
    std::string dir =
        (slash == std::string::npos) ? "" : p.substr(0, slash + 1);
    return dir + base;
  };
  std::string outPath = outputPathArg;
  if (outPath.empty()) {
    if (emit == "asm")
      outPath = stemOf(inputPath) + ".s";
    else if (emit == "obj")
      outPath = stemOf(inputPath) + ".o";
    else
      outPath = defaultOutputFor(inputPath);
  }

  // 5. Codegen -> LLVM IR
  Codegen codegen("nusa", inputPath);
  if (!triple.empty())
    codegen.setTargetTriple(triple);
  if (debug)
    codegen.setDebug(true);
  std::string genErr;
  if (!codegen.generate(merged, genErr)) {
    // genErr sudah diformat lengkap (file:line:col: error: ... + help).
    std::fprintf(stderr, "%s\n", genErr.c_str());
    return 1;
  }

  std::string llPath = inputPath + ".ll";
  // Jika input tanpa direktori, hasilkan .ll di cwd agar mudah diinspeksi.
  // (inputPath + ".ll" sudah tepat untuk kedua kasus.)
  if (emit == "ir" && !outputPathArg.empty())
    llPath = outputPathArg;
  if (!codegen.emitIRToFile(llPath, genErr)) {
    std::fprintf(stderr, "error: %s\n", genErr.c_str());
    return 1;
  }
  long long tCode = nowMs();
  if (emit == "ir") {
    std::fprintf(stderr, "Emitted %s\n", llPath.c_str());
    return 0;
  }

  // WASI capability gate (Phase 111, milestone 1): program yang memakai
  // subsistem runtime tanpa pemetaan WASI ditolak eksplisit saat LINK
  // (bukan dump linker). Inspeksi (--emit-ir/-asm/-c) selalu diizinkan;
  // penolakan berlaku dengan/tanpa --sysroot (sysroot tak menyediakan
  // threads/jaringan). Komputasi murni (+ libc host) tetap lewat ke
  // pesan --sysroot biasa bila sysroot absen.
  if (!triple.empty() && triple.rfind("wasm32", 0) == 0 && emit.empty()) {
    bool ok = false;
    std::string ll = readFileOrEmpty(llPath, ok);
    if (ok) {
      std::vector<std::string> caps = usedRtCaps(ll);
      if (!caps.empty()) {
        std::string list;
        for (size_t i = 0; i < caps.size(); i++) {
          if (i)
            list += ", ";
          list += caps[i];
        }
        std::fprintf(stderr,
                     "error: target '%s' cannot link program using: %s\n",
                     triple.c_str(), list.c_str());
        std::fprintf(stderr, "help: no WASI mapping for these Nusa "
                             "capabilities yet (see docs/wasi-3.12.md); "
                             "pure-compute programs link with "
                             "--sysroot <wasi-sysroot>\n");
        return 1;
      }
    }
  }

  // 5b. Incremental (Phase 35): IR identik + output ada → lewati
  // clang (mahal). Frontend murah selalu jalan (benar). Perbandingan
  // IR (bukan input) membuat perubahan komentar/format ikut hemat;
  // aman pula bila compiler nondeterministik (IR beda → rebuild).
  // --stats (Phase 65) selalu mengukur build penuh: lewati cache agar
  // angka bermakna (fingerprint tetap ditulis ulang di bawah).
  bool useCache = !fresh && !stats;
  {
    namespace fs = std::filesystem;
    incr::Fingerprint cur = currentFingerprint(
        triple, optLevel, emit, sysroot, links, modules, llPath, debug,
        pgoFp, outPath);
    std::string cacheDir = incr::cacheDirFor(outPath);
    std::string fpPath = incr::fingerprintPath(cacheDir);
    if (useCache) {
      incr::Fingerprint old;
      std::string ferr;
      std::error_code ec;
      bool outExists = fs::is_regular_file(outPath, ec) && !ec;
      bool haveOld = incr::readFingerprint(fpPath, old, ferr);
      // Hit: flag sama + IR sama + output ada (sumber boleh beda —
      // mis. komentar — selama IR identik).
      if (outExists && haveOld && !cur.ir.empty() && old.ir == cur.ir &&
          incr::sameFlags(old, cur)) {
        std::fprintf(stderr, "Fresh %s (cached)\n", outPath.c_str());
        return 0;
      }
      if (haveOld) {
        if (!incr::sameFlags(old, cur)) {
          std::fprintf(stderr, "rebuilding: flags changed\n");
        } else {
          auto changed = incr::changedModules(old, cur);
          std::string msg;
          for (size_t i = 0; i < changed.size() && i < 5; i++) {
            if (i)
              msg += ", ";
            msg += changed[i];
          }
          if (changed.size() > 5)
            msg += ", ...";
          if (!msg.empty())
            std::fprintf(stderr, "rebuilding: %s\n", msg.c_str());
        }
      }
    }
    // (Fingerprint ditulis usai clang sukses — lihat bawah.)
  }

  // 6. clang: LLVM IR -> native (host atau --target).
  std::string cmd = "clang " + shellQuote(llPath);
  // Level optimasi LLVM (Phase 34): default O0 = perilaku lama.
  if (!optLevel.empty()) {
    if (optLevel != "0" && optLevel != "1" && optLevel != "2" &&
        optLevel != "3" && optLevel != "s" && optLevel != "z") {
      std::fprintf(stderr, "error: unknown opt level '-O%s' (want 0/1/2/3/s/z)\n",
                   optLevel.c_str());
      return 1;
    }
    cmd += " -O" + optLevel;
  }
  if (debug)
    cmd += " -g";
  // PGO (Phase 104): instrumentasi vs konsumsi profil (LLVM backend).
  if (pgoGenerate)
    cmd += " -fprofile-generate";
  if (!pgoUse.empty())
    cmd += " -fprofile-use=" + shellQuote(pgoUse);
  if (!triple.empty())
    cmd += " --target=" + shellQuote(triple);
  if (!sysroot.empty())
    cmd += " --sysroot=" + shellQuote(sysroot);
  if (emit == "asm")
    cmd += " -S";
  else if (emit == "obj")
    cmd += " -c";
  std::string rt = findRuntimeFile();
  // Objek precompiled di sisi binary (Phase 34) diutamakan atas
  // sumber .c (menghemat kompilasi ulang ~0.8 dtk per build).
  std::string rtObj = findRuntimeObject();
  // Mode compile-only (-c/-S): tanpa link — runtime C tak diikutkan
  // (satu input → satu output; no linker involved).
  bool linkMode = emit.empty();
  // Runtime minimal (Phase 68): lewati TOTAL bila IR tak merujuk
  // simbol fungsi runtime. Arah aman: positif-palsu = link seperti
  // dulu; negatif-palsu mustahil (ref → simbol `@nusa_*`).
  bool needRt = false;
  if (linkMode) {
    bool ok = false;
    std::string ll = readFileOrEmpty(llPath, ok);
    static const char *kRtSyms[] = {
        "@nusa_thread_", "@nusa_mutex_", "@nusa_chan_",
        "@nusa_atomic_", "@nusa_sleep_", "@nusa_time_",
        "@nusa_random_", "@nusa_process_", "@nusa_tcp_",
        "@nusa_sha256_", "@nusa_crypto_", // Phase 76 (RFC-0011)
        "@nusa_alloc", // Phase 123 (seam alokator heap program)
    };
    if (ok) {
      for (const char *s : kRtSyms) {
        if (ll.find(s) != std::string::npos) {
          needRt = true;
          break;
        }
      }
    } else {
      needRt = true; // tak terbaca: link seperti dulu (aman)
    }
  }
  if (linkMode && needRt && !rtObj.empty())
    cmd += " " + shellQuote(rtObj);
  else if (linkMode && needRt && !rt.empty())
    cmd += " " + shellQuote(rt);
  // Link FFI C (Phase 32): file, -L, -l (shell-quoted).
  for (const auto &f : links.files)
    cmd += " " + shellQuote(f);
  for (const auto &d : links.libDirs)
    cmd += " -L" + shellQuote(d);
  for (const auto &l : links.libs)
    cmd += " -l" + shellQuote(l);
  // GC section (Phase 68): buang fungsi runtime tak terpakai.
  if (linkMode)
    cmd += " -Wl,--gc-sections";
  cmd += " -o " + shellQuote(outPath) + " -pthread -Wno-override-module 2>&1";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    std::fprintf(stderr, "error: failed to invoke clang\n");
    return 1;
  }
  std::string clangOut;
  char buf[4096];
  while (std::fgets(buf, sizeof(buf), pipe))
    clangOut += buf;
  int clangRc = pclose(pipe);
  if (clangRc != 0) {
    std::fprintf(stderr, "error: clang failed (code %d):\n%s\n", clangRc,
                 clangOut.c_str());
    return 1;
  }
  long long tClang = nowMs();

  std::fprintf(stderr, "Built %s -> %s\n", inputPath.c_str(), outPath.c_str());
  // Catat fingerprint (Phase 35) untuk build berikutnya.
  {
    namespace fs = std::filesystem;
    incr::Fingerprint cur = currentFingerprint(
        triple, optLevel, emit, sysroot, links, modules, llPath, debug,
        pgoFp, outPath);
    std::string cacheDir = incr::cacheDirFor(outPath);
    std::error_code ec;
    fs::create_directories(cacheDir, ec);
    std::string ferr;
    if (!incr::writeFingerprint(incr::fingerprintPath(cacheDir), cur,
                                ferr)) {
      // Cache best-effort: kegagalan tulis tak menggagalkan build.
      std::fprintf(stderr, "warning: %s\n", ferr.c_str());
    }
  }
  if (stats) {
    namespace fs = std::filesystem;
    std::error_code ec1, ec2;
    // Cache hit ("Fresh (cached)") keluar dini di atas; stats hanya
    // untuk build penuh (clang jalan) — konsisten & jujur.
    auto llBytes = fs::file_size(llPath, ec1);
    auto exeBytes = fs::file_size(outPath, ec2);
    std::fprintf(stderr,
                 "build-stats: frontend %lldms codegen %lldms clang %lldms "
                 "total %lldms ir %llub exe %llub\n",
                 tFront - t0, tCode - tFront, tClang - tCode, tClang - t0,
                 ec1 ? 0ULL : (unsigned long long)llBytes,
                 ec2 ? 0ULL : (unsigned long long)exeBytes);
  }
  return 0;
}

int Driver::runRun(const std::string &inputPath,
                    const std::vector<std::string> &args) {
  // Build ke file temporer lalu eksekusi.
  std::string tmpExe = inputPath + ".run.out";
  int rc = runBuild(inputPath, tmpExe);
  if (rc != 0)
    return rc;
  // system() memakai /bin/sh: path relatif tanpa '/' tidak dicari di cwd
  // (PATH), jadi awali './' bila perlu.
  std::string execPath = tmpExe;
  if (execPath.find('/') == std::string::npos)
    execPath = "./" + execPath;
  // Teruskan argumen CLI ke program (v1.10.0: process_argc/argv).
  std::string cmd = shellQuote(execPath);
  for (const auto &a : args)
    cmd += " " + shellQuote(a);
  int sysRc = std::system(cmd.c_str());
  std::remove(tmpExe.c_str());
  if (sysRc == -1)
    return 1;
  if (WIFEXITED(sysRc))
    return WEXITSTATUS(sysRc);
  return 1;
}

int Driver::runBench(const std::string &inputPath, int runs,
                     const std::vector<std::string> &args) {
  // Bangun --fresh sekali (angka run tak terkontaminasi cache log),
  // lalu N run via fork+wait4: waktu dinding + maxRSS anak.
  // Output program diteruskan (stdio diwarisi); statistik ke stderr.
  if (runs < 1) {
    std::fprintf(stderr, "error: 'nusa bench' needs -n >= 1\n");
    return 1;
  }
  if (!hasNsExtension(inputPath)) {
    std::fprintf(stderr, "error: input file must have '.ns' extension: %s\n",
                 inputPath.c_str());
    return 1;
  }
  std::string tmpExe = inputPath + ".bench.out";
  int rc = runBuild(inputPath, tmpExe, LinkSpec{}, "", "", "", "", true);
  if (rc != 0)
    return rc;
  std::string execPath = tmpExe;
  if (execPath.find('/') == std::string::npos)
    execPath = "./" + execPath;
  std::vector<long long> us;
  std::vector<long> rss;
  int progRc = 0;
  for (int i = 0; i < runs; i++) {
    auto t0 = std::chrono::steady_clock::now();
    pid_t pid = fork();
    if (pid < 0) {
      std::fprintf(stderr, "error: fork failed for benchmark run %d\n", i + 1);
      std::remove(tmpExe.c_str());
      return 1;
    }
    if (pid == 0) {
      // Anak: bangun argv (execPath + args) lalu exec.
      std::vector<char *> argv;
      argv.push_back(const_cast<char *>(execPath.c_str()));
      for (const auto &a : args)
        argv.push_back(const_cast<char *>(a.c_str()));
      argv.push_back(nullptr);
      execv(execPath.c_str(), argv.data());
      _exit(127);
    }
    int status = 0;
    struct rusage ru;
    if (wait4(pid, &status, 0, &ru) < 0) {
      std::fprintf(stderr, "error: wait failed for benchmark run %d\n", i + 1);
      std::remove(tmpExe.c_str());
      return 1;
    }
    auto t1 = std::chrono::steady_clock::now();
    us.push_back(
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
            .count());
    rss.push_back(ru.ru_maxrss); // KiB (Linux/Bionic)
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) != 0 && progRc == 0)
        progRc = WEXITSTATUS(status);
    } else if (progRc == 0) {
      progRc = 1; // sinyal/crash
    }
  }
  std::remove(tmpExe.c_str());
  std::sort(us.begin(), us.end());
  long long mn = us.front(), md = us[us.size() / 2], mx = us.back();
  long mrss = 0;
  for (long r : rss)
    mrss = r > mrss ? r : mrss;
  std::fprintf(stderr, "bench: %d runs min %.1fms median %.1fms max %.1fms",
               runs, mn / 1000.0, md / 1000.0, mx / 1000.0);
  std::fprintf(stderr, " maxrss %ldKiB\n", mrss);
  return progRc;
}

int Driver::runFmt(const std::vector<std::string> &paths, bool checkOnly) {
  if (paths.empty()) {
    std::fprintf(stderr, "error: 'nusa fmt' needs at least one file.ns\n");
    return 1;
  }
  bool changedAny = false;
  for (const auto &path : paths) {
    if (!hasNsExtension(path)) {
      std::fprintf(stderr,
                   "error: input file must have '.ns' extension: %s\n",
                   path.c_str());
      return 1;
    }
    bool ok = false;
    std::string source = readFileOrEmpty(path, ok);
    if (!ok) {
      std::fprintf(stderr, "error: cannot read file '%s'\n", path.c_str());
      return 1;
    }
    FormatResult r = formatSource(path, source);
    if (!r.ok) {
      std::fprintf(stderr, "%s\n", r.error.c_str());
      return 1;
    }
    if (r.text == source)
      continue;
    changedAny = true;
    if (checkOnly) {
      std::printf("would reformat %s\n", path.c_str());
      continue;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      std::fprintf(stderr, "error: cannot write file '%s'\n", path.c_str());
      return 1;
    }
    out << r.text;
    out.close();
    if (!out) {
      std::fprintf(stderr, "error: cannot write file '%s'\n", path.c_str());
      return 1;
    }
    std::printf("formatted %s\n", path.c_str());
  }
  return (checkOnly && changedAny) ? 1 : 0;
}

int Driver::runDoc(const std::string &inputPath,
                   const std::string &outputPath,
                   const std::string &format) {
  if (!hasNsExtension(inputPath)) {
    std::fprintf(stderr, "error: input file must have '.ns' extension: %s\n",
                 inputPath.c_str());
    return 1;
  }
  if (format != "markdown" && format != "json") {
    std::fprintf(stderr, "error: unknown doc format '%s' (want markdown|json)\n",
                 format.c_str());
    return 1;
  }
  bool ok = false;
  std::string source = readFileOrEmpty(inputPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read file '%s'\n",
                 inputPath.c_str());
    return 1;
  }
  std::string text;
  if (format == "json") {
    std::string derr;
    bool dok = false;
    text = generateDocJson(inputPath, source, dok, derr);
    if (!dok) {
      std::fprintf(stderr, "%s\n", derr.c_str());
      return 1;
    }
    text += "\n";
  } else {
    DocResult r = generateDoc(inputPath, source);
    if (!r.ok) {
      std::fprintf(stderr, "%s\n", r.error.c_str());
      return 1;
    }
    text = r.markdown;
  }
  if (outputPath.empty()) {
    std::printf("%s", text.c_str());
    return 0;
  }
  std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::fprintf(stderr, "error: cannot write file '%s'\n",
                 outputPath.c_str());
    return 1;
  }
  out << text;
  return 0;
}

// Escape dump untuk `nusa lex` (Phase 38): identik dengan yang dipakai
// selfhost/lex.ns (kontrak di docs/selfhosting.md).
static std::string lexDumpEscape(const std::string &s) {
  std::string o;
  for (unsigned char c : s) {
    if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\t')
      o += "\\t";
    else if (c == '\r')
      o += "\\r";
    else if (c == '\0')
      o += "\\0";
    else if (c == '|')
      o += "\\|";
    else
      o.push_back(static_cast<char>(c));
  }
  return o;
}

int Driver::runLex(const std::string &inputPath) {
  bool ok = false;
  std::string source = readFileOrEmpty(inputPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read '%s'\n", inputPath.c_str());
    return 1;
  }
  Lexer lx(inputPath, source);
  std::vector<Token> toks = lx.tokenize();
  for (const auto &t : toks) {
    std::printf("%s|%s|%d|%d\n", tokenKindName(t.kind).c_str(),
                lexDumpEscape(t.lexeme).c_str(), t.loc.line, t.loc.column);
  }
  if (!lx.errors().empty()) {
    for (const auto &e : lx.errors())
      std::fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  return 0;
}

// Dump AST (Phase 72): parse file tunggal (tanpa merge modul —
// cermin Parser agar `selfhost/parse.ns` punya oracle byte-identik)
// lalu cetak `program.dump()`.
int Driver::runAst(const std::string &inputPath) {
  bool ok = false;
  std::string source = readFileOrEmpty(inputPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read '%s'\n", inputPath.c_str());
    return 1;
  }
  Lexer lx(inputPath, source);
  std::vector<Token> toks = lx.tokenize();
  if (!lx.errors().empty()) {
    for (const auto &e : lx.errors())
      std::fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  std::vector<std::string> lines = DiagnosticEngine::splitLines(source);
  Parser parser(inputPath, std::move(toks), lines);
  ParseResult pres = parser.parse();
  if (!pres.ok()) {
    DiagnosticEngine diag(lines);
    for (const auto &e : pres.errors)
      diag.reportParseError(e);
    return 1;
  }
  std::printf("%s\n", pres.program.dump().c_str());
  return 0;
}

// Menjalankan perintah shell dan menangkap stdout-nya (rc via WEXITSTATUS).
static int runCapture(const std::string &cmd, std::string &out) {
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return -1;
  char buf[4096];
  while (std::fgets(buf, sizeof(buf), pipe))
    out += buf;
  int rc = pclose(pipe);
  if (WIFEXITED(rc))
    return WEXITSTATUS(rc);
  return 1;
}

int Driver::runBootstrap(const std::string &lexNsPath,
                         const std::string &outDirArg) {
  return runBootstrapKind(lexNsPath, outDirArg, "lex");
}

// Referensi Stage 0 per kind (Phase 73, bootstrap 2.0): "lex" = dump
// token (cermin runLex), "ast" = dump AST (cermin runAst + "\n").
// false + err bila komponen tak valid menurut Stage 0.
static bool stage0Ref(const std::string &kind, const std::string &path,
                      const std::string &source, std::string &ref,
                      std::string &err) {
  Lexer lx(path, source);
  std::vector<Token> toks = lx.tokenize();
  if (!lx.errors().empty()) {
    err = "Stage 0 lex of '" + path + "' has errors";
    return false;
  }
  if (kind == "ast") {
    std::vector<std::string> lines = DiagnosticEngine::splitLines(source);
    Parser parser(path, std::move(toks), lines);
    ParseResult pres = parser.parse();
    if (!pres.ok()) {
      err = "Stage 0 parse of '" + path + "' has errors";
      return false;
    }
    ref = pres.program.dump();
    ref += "\n";
    return true;
  }
  for (const auto &t : toks) {
    ref += tokenKindName(t.kind);
    ref += "|";
    ref += lexDumpEscape(t.lexeme);
    ref += "|" + std::to_string(t.loc.line) + "|" +
           std::to_string(t.loc.column) + "\n";
  }
  return true;
}

static std::string stageBaseName(const std::string &nsPath) {
  std::string base = nsPath;
  size_t slash = base.find_last_of("/\\");
  if (slash != std::string::npos)
    base = base.substr(slash + 1);
  if (base.size() > 3 && base.compare(base.size() - 3, 3, ".ns") == 0)
    base = base.substr(0, base.size() - 3);
  if (base.empty())
    base = "stage";
  return base;
}

int Driver::runBootstrapKind(const std::string &inputNsPath,
                             const std::string &outDirArg,
                             const std::string &kind,
                             const std::string &rebuildDir) {
  // Rantai bootstrap (Phase 39 lexer; Phase 73 multi-komponen,
  // docs/bootstrap.md):
  //   Stage 0: compiler C++ ini (referensi per kind: lex/ast).
  //   Stage 1: Stage 0 mengkompilasi komponen Nusa -> <base>_stage1.
  //   Stage 2: biner Nusa memproses dirinya sendiri (self-application).
  // Validasi: Stage 2 == Stage 0 byte-identik +
  // determinisme (Stage 2 dijalankan 2x, hash sama).
  if (kind != "lex" && kind != "ast") {
    std::fprintf(stderr, "error: unknown bootstrap kind '%s'\n",
                 kind.c_str());
    std::fprintf(stderr, "help: use --as lex|ast\n");
    return 1;
  }
  if (!hasNsExtension(inputNsPath)) {
    std::fprintf(stderr,
                 "error: bootstrap input must have '.ns' extension: %s\n",
                 inputNsPath.c_str());
    return 1;
  }
  bool ok = false;
  readFileOrEmpty(inputNsPath, ok);
  if (!ok) {
    std::fprintf(stderr, "error: cannot read '%s'\n", inputNsPath.c_str());
    std::fprintf(stderr, "help: default is <nusa-root>/selfhost/lex.ns\n");
    return 1;
  }
  std::string outDir = outDirArg.empty() ? std::string("bootstrap-out")
                                         : outDirArg;
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  if (ec) {
    std::fprintf(stderr, "error: cannot create directory '%s': %s\n",
                 outDir.c_str(), ec.message().c_str());
    return 1;
  }
  std::string stage1 = outDir + "/" + stageBaseName(inputNsPath) + "_stage1";

  std::printf("bootstrap: Stage 0: %s\n", versionString());
  std::printf("bootstrap: Stage 1: compiling %s -> %s\n", inputNsPath.c_str(),
              stage1.c_str());
  // fresh=true: rantai harus build nyata (bukan cache basi).
  int rc = runBuild(inputNsPath, stage1, LinkSpec{}, "", "", "", "", true);
  if (rc != 0) {
    std::fprintf(stderr, "bootstrap: Stage 1 FAILED (build rc=%d)\n", rc);
    return 1;
  }

  // Referensi Stage 0 atas komponen itu sendiri (self-application =
  // kasus terkuat: komponen memproses dirinya).
  bool rok = false;
  std::string source = readFileOrEmpty(inputNsPath, rok);
  std::string ref, referr;
  if (!stage0Ref(kind, inputNsPath, source, ref, referr)) {
    std::fprintf(stderr, "bootstrap: %s\n", referr.c_str());
    return 1;
  }

  // Stage 2: biner Nusa (2x untuk determinisme).
  std::string exe = stage1;
  if (exe.find('/') == std::string::npos)
    exe = "./" + exe;
  std::string out2a, out2b;
  int rc2a = runCapture(shellQuote(exe) + " " + shellQuote(inputNsPath), out2a);
  if (rc2a != 0) {
    std::fprintf(stderr, "bootstrap: Stage 2 FAILED (rc=%d)\n", rc2a);
    return 1;
  }
  int rc2b = runCapture(shellQuote(exe) + " " + shellQuote(inputNsPath), out2b);
  if (rc2b != 0 || out2a != out2b) {
    std::fprintf(stderr,
                 "bootstrap: Stage 2 non-deterministic (rc=%d, equal=%d)\n",
                 rc2b, (int)(out2a == out2b));
    return 1;
  }
  std::printf("bootstrap: Stage 2: %s ran twice, %zu bytes, deterministic\n",
              stage1.c_str(), out2a.size());

  if (out2a != ref) {
    // Tampilkan beda pertama agar dapat didiagnosis.
    size_t i = 0;
    while (i < out2a.size() && i < ref.size() && out2a[i] == ref[i])
      i++;
    size_t ls = out2a.rfind('\n', i > 0 ? i - 1 : 0);
    std::fprintf(stderr,
                 "bootstrap: MISMATCH at byte %zu (stage2=%zu, stage0=%zu):\n"
                 "  stage2: %.80s\n  stage0: %.80s\n",
                 i, out2a.size(), ref.size(), out2a.c_str() + (ls == std::string::npos ? 0 : ls + 1),
                 ref.c_str() + (ref.rfind('\n', i > 0 ? i - 1 : 0) == std::string::npos ? 0 : ref.rfind('\n', i > 0 ? i - 1 : 0) + 1));
    return 1;
  }
  std::printf("bootstrap: PASS: Stage 2 output == Stage 0 output (%zu bytes)\n",
              ref.size());
  if (!rebuildDir.empty()) {
    // Stage 3 (Phase 74, bootstrap full): rebuild Stage 1 dari nol
    // (--fresh, direktori lain) lalu 1x run; hasilnya harus == D2
    // (reproduksibilitas rebuild: tak ada bocoran waktu/path ke perilaku).
    std::error_code rec;
    std::filesystem::create_directories(rebuildDir, rec);
    if (rec) {
      std::fprintf(stderr, "error: cannot create directory '%s': %s\n",
                   rebuildDir.c_str(), rec.message().c_str());
      return 1;
    }
    std::string stage3 =
        rebuildDir + "/" + stageBaseName(inputNsPath) + "_stage1";
    std::printf("bootstrap: Stage 3: rebuilding %s -> %s\n",
                inputNsPath.c_str(), stage3.c_str());
    int rc3b =
        runBuild(inputNsPath, stage3, LinkSpec{}, "", "", "", "", true);
    if (rc3b != 0) {
      std::fprintf(stderr, "bootstrap: Stage 3 FAILED (build rc=%d)\n", rc3b);
      return 1;
    }
    std::string exe3 = stage3;
    if (exe3.find('/') == std::string::npos)
      exe3 = "./" + exe3;
    std::string out3;
    int rc3 =
        runCapture(shellQuote(exe3) + " " + shellQuote(inputNsPath), out3);
    if (rc3 != 0) {
      std::fprintf(stderr, "bootstrap: Stage 3 FAILED (rc=%d)\n", rc3);
      return 1;
    }
    std::printf("bootstrap: Stage 3: %s ran, %zu bytes\n", stage3.c_str(),
                out3.size());
    if (out3 != out2a) {
      size_t i = 0;
      while (i < out3.size() && i < out2a.size() && out3[i] == out2a[i])
        i++;
      size_t ls = out3.rfind('\n', i > 0 ? i - 1 : 0);
      std::fprintf(stderr,
                   "bootstrap: REBUILD MISMATCH at byte %zu (stage3=%zu, "
                   "stage2=%zu):\n"
                   "  stage3: %.80s\n  stage2: %.80s\n",
                   i, out3.size(), out2a.size(),
                   out3.c_str() + (ls == std::string::npos ? 0 : ls + 1),
                   out2a.c_str() + (out2a.rfind('\n', i > 0 ? i - 1 : 0) == std::string::npos ? 0 : out2a.rfind('\n', i > 0 ? i - 1 : 0) + 1));
      return 1;
    }
    std::printf("bootstrap: REBUILD PASS: Stage 3 output == Stage 2 output "
                "(%zu bytes)\n",
                out3.size());
  }
  std::printf("bootstrap: artifacts in %s/\n", outDir.c_str());
  return 0;
}

} // namespace nusa
