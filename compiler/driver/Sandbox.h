#pragma once
#include <string>

namespace nusa {
namespace sandbox {

// Batas eksekusi tak-terpercaya (Phase 42). 0 = tanpa batas
// (tidak disarankan untuk playground; CLI mengizinkan eksplisit).
struct SandboxLimits {
  int timeoutSec = 10; // wall-clock fase RUN (build di luar ini)
  long memoryMb = 256; // polling VmRSS runner (lihat bawah)
  long outputKb = 64;  // cap stdout+stderr gabungan
};

// Hasil: verdict ∈ {"ok","timeout","mem-limit","output-limit",
// "signal","build-fail"}.
// "mem-limit": VmRSS runner melebihi memoryMb (polling 50ms) → SIGKILL.
//   SENGAJA bukan RLIMIT_AS: di Bionic, RLIMIT_AS < ~1GB membuat
//   linker gagal memetakan CFI shadow saat startup (bahkan untuk
//   hello-world) — diverifikasi empiris. Polling + OOM-killer kernel
//   sebagai backstop didokumentasikan di docs/playground.md.
// "timeout": run melebihi timeoutSec (atau total supervisor).
// "signal": anak mati sinyal (code = signo).
// "build-fail": kompilasi gagal (output = log compiler, code = rc).
struct SandboxResult {
  std::string verdict = "build-fail";
  std::string output;
  int code = 1;
};

// Bangun nsPath lalu jalankan di direktori terisolasi (mkdtemp),
// stdin /dev/null, environment dikosongkan, stdout+stderr digabung
// dan dibatasi. Total waktu (termasuk build) juga dibatasi supervisor
// (timeoutSec + budget build) agar tak ada fase yang gantung.
SandboxResult runSandboxed(const std::string &nsPath,
                           const SandboxLimits &lim);

// Varian stdin (Phase 112, plugin): request bytes diumpankan ke stdin
// runner via berkas workdir (bukan /dev/null); batas + verdict sama.
// stdinData kosong == runSandboxed persis (jalur /dev/null lama).
SandboxResult runSandboxedInput(const std::string &nsPath,
                                const SandboxLimits &lim,
                                const std::string &stdinData);

} // namespace sandbox
} // namespace nusa
