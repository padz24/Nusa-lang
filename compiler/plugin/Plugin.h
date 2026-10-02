#pragma once
// Plugin Nusa v1 — milestone 1 (Phase 112): program `.ns` sebagai plugin
// request-response JSON tunggal via stdin/stdout, dieksekusi di bawah
// batas sandbox proses (`runSandboxedInput`). BUKAN eksekusi WASM
// (engine belum ada — jalur migrasi di docs/plugin-3.13.md).
// Kontrak: stdin = satu objek JSON {"op":"...", ...}; stdout =
// satu objek JSON {"ok":bool, ...}. Manifest (opsional) mendeklarasikan
// `ops` yang diizinkan; op di luar daftar DITOLAK tanpa eksekusi.
#include <string>
#include <vector>

namespace nusa {
namespace plugin {

struct PluginManifest {
  bool present = false;
  std::string name;
  std::string version;
  std::vector<std::string> ops; // op yang diizinkan (bila present)
};

// Parse manifest JSON subset: objek datar {"name":s,"version":s,
// "ops":[s,...]}; kunci asing diabaikan; tipe salah = error.
bool parseManifest(const std::string &text, PluginManifest &out,
                   std::string &err);

// Ambil "op" string top-level dari request (nilai lain dilewati
// robust termasuk nested + escape). Hilang/bukan-string = error.
bool extractOp(const std::string &request, std::string &op,
               std::string &err);

struct PluginResult {
  std::string verdict; // = verdict sandbox ("ok", "timeout", ...)
  int code = 1;        // exit program bila ok; 124/137/1 bila limit
  std::string output;  // stdout+stderr program (terbatas cap)
};

struct PluginLimits {
  int timeoutSec = 10;
  long memoryMb = 256;
  long outputKb = 64;
};

// Gate op (bila manifest.present) lalu eksekusi tersandbox.
// request dinormalisasi: newline akhir dijamin (untuk builtin input).
// verdict "refused" = ditolak pra-eksekusi (tak ada proses anak).
PluginResult runPlugin(const std::string &nsPath,
                       const PluginManifest &manifest,
                       const PluginLimits &lim, const std::string &request);

} // namespace plugin
} // namespace nusa
