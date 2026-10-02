#pragma once
// Package manager Nusa (Phase 30, v1.11.0): manifest `nusa.toml`,
// resolver path-dependencies, lock file + checksum, `pkg` commands.
// Registry (publish/download) = Phase 31; versi-deps tercatat tapi
// belum ter-resolve (error jelas).
#include <string>
#include <vector>

namespace nusa {
class Driver;
namespace pkg {

// Versi compiler berjalan (dari macro build NUSA_VERSION).
std::string compilerVersion();

// Satu dependensi (§[dependencies]): path lokal ATAU versi registry.
struct Dep {
  std::string name;
  std::string path;    // non-kosong = path-dep (relatif ke manifest)
  std::string version; // non-kosong = registry-dep (Phase 31)
  bool isPath() const { return !path.empty(); }
};

struct Manifest {
  std::string name;
  std::string version;
  std::string compiler; // versi compiler minimum ("1.11.0")
  std::string target;   // opsional ("aarch64-linux-android")
  std::string description; // opsional (registry search)
  std::vector<std::string> features; // opsional, bebas
  std::string entry;                 // opsional (default main.ns|src/main.ns)
  std::vector<std::string> linkLibs;    // [link] libraries (Phase 32)
  std::vector<std::string> linkLibDirs; // [link] library-dirs
  std::vector<std::string> linkFiles;   // [link] files (relatif paket)
  // Izin paket (Phase 59, v2.10.0): default false bila tak dideklarasi.
  struct Capabilities {
    bool filesystem = false; // file_read/file_write/file_exists
    bool network = false;    // tcp_*/dial (std.net/http)
    bool process = false;    // process_exit/env/cwd
    bool environment = false; // dicadangkan (env terperinci)
    bool native = false;     // extern fn + [link] (FFI) — DIENFORCE
  };
  Capabilities caps;
  std::vector<Dep> deps;
  std::string dir; // direktori manifest (abs/relatif pembentuk)
};

// Ringkasan capability aktif ("filesystem network" / "(none)").
std::string capabilitiesSummary(const Manifest &m);

// Satu paket terkunci (§[[package]] di nusa.lock).
struct LockPkg {
  std::string name;
  std::string version;
  std::string path;     // path-dep (relatif ke manifest akar)
  std::string registry; // registry-dep: id registry sumber ("" = path)
  std::string checksum; // "sha256:<hex>" atas sumber paket
};

// Satu entri index registry (§[[version]] di index/<name>.toml).
struct IndexEntry {
  std::string version;
  std::string checksum; // "sha256:..." atas ISI paket (dirChecksum)
  std::string tarball;  // "sha256:..." atas file .tar.gz (transport)
  std::vector<std::string> deps; // "name req" per dependensi
};

// Parse nusa.toml (subset TOML: section, key = string/int/bool/array,
// inline-table). False + err bila gagal.
bool parseManifest(const std::string &path, Manifest &out, std::string &err);
// Tulis manifest (format kanonis). `emitCaps` (init): tulis
// [capabilities] eksplisit walau semua false (deklarasi terlihat).
bool writeManifest(const std::string &path, const Manifest &m,
                   std::string &err, bool emitCaps = false);
// Parse nusa.lock. False + err bila gagal.
bool parseLock(const std::string &path, std::vector<LockPkg> &out,
               std::string &err);
// Sama + baca pin compiler [meta] (Phase 61; "" bila tak ada).
bool parseLockFull(const std::string &path, std::vector<LockPkg> &out,
                   std::string *lockCompiler, std::string &err);
// Tulis lock (GENERATED header + [[package]] terurut nama).
bool writeLock(const std::string &path, const std::vector<LockPkg> &pkgs,
               std::string &err);

// Semver "M.m.p[-pre]": parse + bandingkan (-1/0/1) + minimum-check.
bool parseSemver(const std::string &s, int &maj, int &min, int &patch,
                 std::string &pre);
int cmpSemver(const std::string &a, const std::string &b); // -1/0/1
bool satisfiesMin(const std::string &have, const std::string &minimum);
// Requirement versi registry: "1.2.3" persis atau "^1.2.3" caret.
bool satisfiesReq(const std::string &version, const std::string &req);

// SHA-256 hex (LLVM Support) + checksum direktori paket (file *.ns +
// nusa.toml top-level, terurut nama): "sha256:<hex>".
std::string sha256Hex(const std::string &data);
std::string dirChecksum(const std::string &dir);
// Validator isi paket (Phase 59): true bila hanya file reguler
// top-level bernama waras; false + `badName` bila tidak.
bool packageDirSafe(const std::string &dir, std::string &badName);

// Cari manifest ke atas dari cwd (./nusa.toml, ../, ...). "" bila nihil.
std::string findManifestUp();

// Perintah `nusa pkg ...`. Mengembalikan exit code.
int runPkg(Driver &driver, const std::vector<std::string> &args);

// Project templates `nusa new <template> [name]` (Phase 43).
// Mengembalikan exit code.
int runNew(const std::vector<std::string> &args);

} // namespace pkg
} // namespace nusa
