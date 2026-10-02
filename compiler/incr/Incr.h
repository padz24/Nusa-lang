#pragma once
// Incremental compilation (Phase 35, v1.16.0): dependency graph,
// fingerprint sumber + flag, dan cache artefak berbasis hash IR.
//
// Model (lihat docs/incremental.md):
//  - frontend (load→check→IR) selalu jalan (murah, ~10% build);
//  - clang (mahal, ~85%) dilewati bila IR identik dengan build lalu.
#include <string>
#include <vector>

namespace nusa {
struct LoadedModule;
namespace incr {

// Satu simpul dependency graph.
struct ModuleNode {
  std::string path;    // absolut ternormalisasi (identitas)
  std::string display; // untuk pesan
  std::string sha;     // sha256 isi file ("missing:..." bila tak terbaca)
  std::vector<std::string> imports; // nama modul yang diimpor
};

// Bangun graph dari modul termuat (urutan load dipertahankan).
// Edge: pencocokan nama-basename terhadap modul termuat (best-effort;
// fingerprint memakai SET, bukan edge — graph untuk pelaporan).
std::vector<ModuleNode> buildGraph(const std::vector<LoadedModule> &modules);

// Fingerprint satu build: semua input yang memengaruhi artefak.
struct Fingerprint {
  std::string compiler; // sha256 binary nusa (perubahan compiler → rebuild)
  std::string target;   // triple ("" = host)
  std::string opt;      // level -O ("0"..)
  std::string emit;     // ""/"ir"/"asm"/"obj"
  std::string sysroot;
  bool debug = false; // -g (Phase 37)
  std::string pgo; // "" | "generate" | "use:<sha-profdata>" (Phase 104)
  std::vector<std::string> links; // deskriptor ternormalisasi + hash file
  std::vector<ModuleNode> modules; // terurut path
  std::string ir;                  // sha256 IR terakhir ("" = belum ada)
  std::string out; // output path apa adanya (Phase 126: sidik per-dir
                   // dibagi multi-output; tanpa ini output kedua yang
                   // IR+flag-nya sama dinyatakan Fresh basi)
};

std::string shaFile(const std::string &path); // "" bila gagal
std::string shaCompiler(); // sha256 /proc/self/exe ("unknown:" + alasan)
std::string cacheDirFor(const std::string &outputPath); // dirname + /.nusa-cache
std::string fingerprintPath(const std::string &cacheDir);

bool readFingerprint(const std::string &path, Fingerprint &out,
                     std::string &err);
bool writeFingerprint(const std::string &path, const Fingerprint &fp,
                      std::string &err);
// Sama bila semua input sama (ir diabaikan — dibandingkan terpisah).
bool sameInputs(const Fingerprint &a, const Fingerprint &b);
// Sama bila flag build sama (compiler/target/opt/emit/sysroot/links);
// modul diabaikan (perubahan komentar ikut hemat bila IR sama).
bool sameFlags(const Fingerprint &a, const Fingerprint &b);
// Nama modul yang berubah (untuk laporan "rebuilding: ...").
std::vector<std::string> changedModules(const Fingerprint &oldFp,
                                        const Fingerprint &cur);

} // namespace incr
} // namespace nusa
