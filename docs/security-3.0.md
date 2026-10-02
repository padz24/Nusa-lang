# Security Audit — Phase 87 (beta 3.0.0)

Mandat: audit 9 area terhadap 6 kelas; perbaiki semua
critical/high sebelum rilis. Hasil: **0 critical, 0 high** —
1 medium DIPERBAIKI (S1); sisanya bersih/info. Rilis tak
terblokir dari sisi keamanan.

## 1. Matriks area × kelas (ringkas)

| Area | memory | traversal | confusion | tampering | unsafe-exec | crash |
|---|---|---|---|---|---|---|
| compiler | OK¹ | OK² | — | — | — | OK³ |
| runtime | OK⁴ | — | — | — | OK⁵ | OK³ |
| package manager | — | OK⁶ | OK⁷ | OK⁸ | OK⁹ | OK³ |
| registry | — | OK⁶ | OK⁷ | OK⁸ | — | — |
| FFI | — | — | — | — | OK¹⁰ | — |
| unsafe | N/A¹¹ | N/A | N/A | N/A | N/A | N/A |
| build system | — | — | — | — | OK⁹ | — |
| networking | — | — | — | — | OK¹² | — |
| dependency resolution | — | — | OK⁷ | OK⁸ | — | — |

¹ malloc codegen semua `emitOomCheck`; runtime malloc semua
cek-NULL (chan/crypto/net/sync/thread) → sentinel.
² `import ../..` resolve/tertolak bersih, tanpa crash
(probe); output build hanya ke `-o`/samping-sumber (by design).
³ Baterai adversarial: UTF-8-invalid, NUL, BOM, bigint,
string-100KB, file-kosong, direktori, output-tak-tulis,
import-cycle/chain-100, ident-200KB, baris-5MB, header-sampah
(bindgen) — semua rc∈{0,1}, 0 sinyal.
⁴ Loopback-only (`INADDR_LOOPBACK` di `rt_net.c`); recv cap
1 MiB; buf NULL→string-kosong.
⁵ Sandbox rlimits + verdict (e2e_playground 9/9); S1 di bawah.
⁶ Tarball `../` ditolak pre-extract + cache dibersihkan
(`e2e_sec`); GNU tar mensterilkan saat create — craft via
python (terbukti).
⁷ Dual path+version ditolak parse ("exactly one"); name
mismatch ditolak; cache per-registry (`regId` hash).
⁸ Checksum `verify` gagal pasca-tamper + sembuh via update
(`e2e_eco2`, hermetik Phase 80).
⁹ clang/tar/run/link via `shellQuote`/`shellQ` benar;
dibuktikan nama-file `a'b;touch PWNED;` tanpa eksekusi.
¹⁰ `extern`/[link] tanpa `native=true` ditolak + ringkasan
(`e2e_sec`); shim paket strcpy-ke-malloc tercatat aman.
¹¹ Bahasa TAK punya blok `unsafe` (lexer: nol keyword) —
area N/A by design; batas native = capability manifest.
¹² TLS verifikasi peer+hostname selalu-aktif (paket
referensi); `rpc_call` host-eksplisit pemanggil.

## 2. Temuan

- S1 (medium, DIPERBAIKI): `--memory-mb 0` fail-open
  (unlimited) di `sandbox`+`playground`, sementara timeout-0
  fail-closed — inkonsisten. Perbaikan: tolak `<= 0`
  (2 baris `main.cpp`); regresi `sec-limits*` di `e2e_sec`
  (suite 5→9 kasus). NOL e2e lama memakai 0.
- S2 (info): byte UTF-8-invalid di dalam string literal
  diterima check (lenien; perkakas lokal; output
  byte-apa-adanya). Bukan vuln memori (buffer panjang
  eksplisit).
- S3 (info): byte NUL di string diterima check; perilaku
  cetak = UB terdokumentasi (Appendix A spec-3.0).

## 3. Conformance

`e2e_sec.sh` 9/9 + `e2e_hardening.sh` + fuzz 0-crash
(seed 71/1/7/1337/99999/424242/30084) + full suite hijau.
