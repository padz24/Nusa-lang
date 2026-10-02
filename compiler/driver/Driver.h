#pragma once
#include "compiler/ast/AST.h"
#include "compiler/driver/Modules.h"
#include <string>
#include <vector>

namespace nusa {

// Spesifikasi link native untuk FFI C (Phase 32): library (-l),
// direktori (-L), dan file objek/arsip langsung.
struct LinkSpec {
  std::vector<std::string> libs;
  std::vector<std::string> libDirs;
  std::vector<std::string> files;
};

// Target cross-compilation (Phase 33).
struct ResolvedTarget {
  std::string triple;
  bool isHost = true;
};

// Driver: orkestrasi tahapan compiler Nusa.
// v0.9.0: `build`, `run`, `check`, `test`, `fmt`, `doc`
// (+ `--version`, `--help`).
class Driver {
public:
  static const char *versionString();

  // Return code ala proses (0 = sukses).
  int runBuild(const std::string &inputPath, const std::string &outputPath,
               const LinkSpec &links = LinkSpec{},
               const std::string &target = "",
               const std::string &sysroot = "",
               const std::string &emit = "",
               const std::string &optLevel = "", bool fresh = false,
               bool debug = false, bool stats = false,
               bool pgoGenerate = false,
               const std::string &pgoUse = "");
  int runRun(const std::string &inputPath,
             const std::vector<std::string> &args = {});
  // Phase 65: benchmark N run (waktu dinding + maxRSS) ke stderr;
  // output program diteruskan. rc = rc program.
  int runBench(const std::string &inputPath, int runs,
               const std::vector<std::string> &args);
  // Phase 106: REPL AOT-loop (transkrip + runBuild + eksekusi).
  int runRepl();
  // v0.9.0: validasi tanpa codegen (load → typecheck).
  int runCheck(const std::string &inputPath);
  // Phase 119: trace terdistribusi — run (injeksi TRACE_ID) + check log.
  int runTraceRun(const std::string &inputPath,
                  const std::vector<std::string> &args);
  int runTraceCheck(const std::string &logPath);
  // v0.9.0: jalankan test `*_test.ns` (PASS = exit 0).
  int runTest(const std::vector<std::string> &paths);
  // v0.9.0: format file (tulis ulang) / --check (hanya periksa).
  int runFmt(const std::vector<std::string> &paths, bool checkOnly);
  // v0.9.0: dokumentasi API Markdown (stdout atau -o file).
  int runDoc(const std::string &inputPath, const std::string &outputPath,
             const std::string &format = "markdown");
  // Phase 38: dump token lexer ke stdout (kind|lexeme|line|col).
  int runLex(const std::string &inputPath);
  // Phase 72: dump AST parser ke stdout (s-expr `program.dump()`).
  int runAst(const std::string &inputPath);
  // Phase 39: rantai bootstrap (Stage 0 -> Stage 1 -> Stage 2 +
  // verifikasi reproduksibilitas). lexNsPath = komponen Nusa
  // (default selfhost/lex.ns); outDir = direktori artefak.
  int runBootstrap(const std::string &lexNsPath, const std::string &outDir);
  // Phase 73 (bootstrap 2.0): rantai multi-komponen; kind = lex|ast
  // (referensi Stage 0: dump token vs dump AST). runBootstrap = kind lex.
  // Phase 74: rebuildDir non-kosong = Stage 3 (rebuild + banding D2).
  int runBootstrapKind(const std::string &inputNsPath,
                       const std::string &outDir, const std::string &kind,
                       const std::string &rebuildDir = "");
  // Phase 42: jalankan program di sandbox (lihat Sandbox.h).
  // Mencetak output program + baris verdict; 0 bila verdict "ok".
  int runSandbox(const std::string &inputPath, int timeoutSec,
                 long memoryMb, long outputKb);
  // Phase 42: server playground HTTP loopback (lihat docs/playground.md).
  // Berjalan hingga SIGINT/SIGTERM. Port 0 = ephemeral (cetak aktual).
  int runPlayground(int port, int timeoutSec, long memoryMb, long outputKb);

  // Tahapan 1-4 bersama (load → typecheck) dengan pelaporan error.
  // Mengisi `merged` bila 0. Dipakai check/build/test.
  // modulesOut opsional: modul termuat (untuk dependency graph/fingerprint).
  int checkStages(const std::string &inputPath, Program &merged,
                  std::vector<LoadedModule> *modulesOut = nullptr);

  // Hapus artefak tergenerasi di bawah dir (Phase 62): *.ns.ll +
  // .nusa-cache/ (rekursif), target/ + dist/ (top-level). Idempoten.
  // Mengembalikan rc; mencetak ringkasan jumlah.
  int runClean(const std::string &dir);
  // Phase 66: hasilkan binding `extern fn` dari header C
  // (subset; lihat compiler/bindgen). Output "" = stdout.
  int runBindgen(const std::string &inputPath, const std::string &outputPath);
  // Phase 110: hasilkan header C dari API .ns (subset jujur-ABI;
  // lihat compiler/bindgen/Headers.h). Output "" = stdout.
  int runHeaders(const std::string &inputPath, const std::string &outputPath);
  // Phase 83: audit migrasi 2.x -> 3.0 atas file/dir (read-only).
  // Klasifikasi per file: compatible/deprecated/requires
  // migration/broken/unsupported (lihat docs/migration-3.0.md).
  // rc 1 bila requires-migration/broken > 0, selain itu 0.
  int runMigrate(const std::string &target);
  // Phase 112: jalankan plugin .ns tersandbox (kontrak v1).
  // request = satu objek JSON; rc = exit program bila ok, 1 bila
  // verdict lain/penolakan/manifest rusak.
  int runPlugin(const std::string &nsPath, const std::string &manifestPath,
                int timeoutSec, long memoryMb, long outputKb,
                const std::string &request);

  // Triple host clang ini (mis. "aarch64-unknown-linux-android24").
  static std::string hostTriple();
  // Resolusi target (Phase 33): alias ("linux-x86_64") atau triple
  // persis yang didukung. False + err (daftar target) bila tak dikenal.
  static bool resolveTarget(const std::string &arg, ResolvedTarget &out,
                            std::string &err);
  // Resolusi profil build (Phase 62): "dev" (O0), "debug" (O0+g),
  // "release" (O2). Mengisi opt/debug default; false + err bila asing.
  static bool resolveProfile(const std::string &arg, std::string &opt,
                              bool &debug, std::string &err);
};

std::string defaultOutputFor(const std::string &inputPath);
std::string readFileOrEmpty(const std::string &path, bool &ok);

} // namespace nusa
