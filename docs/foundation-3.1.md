# Nusa 3.1 Foundation — Audit (Phase 100)

Status: audit slice. Mandat master Phase 100: stability, compatibility,
performance atas 10 area (compiler, runtime, stdlib, package manager,
registry, LSP, debugger, formatter, FFI, cross compilation). NOL
perubahan semantik bahasa (freeze 3.0 dipertahankan; spec 3.0 normatif
tak tersentuh). Satu BUGFIX formatter (diizinkan: non-semantik,
output program byte-identik).

## 1. Baseline lingkungan (CI ini)

- `llvm-config --version` = 21.1.8; `clang` = 21.1.8
  (target aarch64-unknown-linux-android24); `uname -m` = aarch64.
- `nusa --version` = `3.0.0 (LLVM 21.1.8, AArch64 Android/Termux)`.
- `ctest -N` (build/) = 87 test; `e2e_release` 6/6 hijau pra-perubahan.
- Bench `bench/suite.sh` (Termux AArch64, satu run; varian perangkat):

| metrik | nilai |
|---|---|
| compile hello / nusa10 / perf2000 / parse | 5.41 / 2.33 / 11.03 / 4.35 dtk |
| size hello / parse | 5816 / 96576 B |
| incr cold / warm | 2.93 / 0.55 dtk (≈5.3×) |
| startup hello | 86.5 ms |
| run fib/loopsum/strcat/matchdispatch/closureapply (median) | 101.9 / 149.2 / 201.5 / 110.4 / 230.1 ms |
| pkg tree / build | 0.88 / 3.23 dtk |
| LSP initialize | 403.8 ms |
| biner compiler | 1624136 B |

Angka acuan perangkat-spesifik (bukan klaim perbandingan).

## 2. Temuan FMT-1 (BUG NYATA, DIPERBAIKI di slice ini)

Cabang `LParen` formatter (`compiler/fmt/Formatter.cpp`) selalu
menghapus spasi sebelum `(` (aturan untuk call `f(`), sehingga
grouping sesudah operator menempel: `a - (a / b) * b` → `a -(a / b)
* b`; `= (a)` → `=(a)`; `b * (a + 1)` → `b *(a + 1)`. Idempoten
(stabil setelah 1x format) dan semantik program identik, tetapi
gaya salah + inkonsisten dengan `[` literal (`= [1]` dilindungi
sejak v1.1.0). Korban di korpus: `selfhost/lex.ns` (3 baris
`col +(…)`), `std/math.ns` (3 baris) — keduanya diformat dengan
binary-buggy dan ter-commit apa adanya.

Perbaikan: `(` grouping sesudah `+ - * / = , : return` beri spasi
(cermin `[`); call (`f(`), unary (`-(a)`, dibedakan via konteks
token sebelum operator), dan operator perbandingan/panah
(`< > <= >= == != ->`, status quo 3.0.0) tetap menempel.
Pengecualian perbandingan disengaja: `>` ambigu dengan penutup
generik berjarak (`fn f < T: Show >(v)` di
`tutorials/04-generics-traits/main.ns`, tak dikenali pemindai
`genericBrackets` karena isi `:`) — memberi spasi merusak
`e2e_ecosystem` (terbukti saat verifikasi, lalu dipersempit).
Quirk warisan `a < (b)` → `a <(b)` dipertahankan, bukan regresi.

Regresi: unit baru `testParenGroupingSpacing` di
`compiler/tests/test_fmt.cpp` (aritmetika + `=` + unary + call
generik); kanonisasi ulang `selfhost/lex.ns` + `std/math.ns`
(hanya diff spacing).

## 3. Diverifikasi

- `test_fmt` 18/18 (17 lama + 1 baru).
- `e2e_selfhost` 5/5, `e2e_parse` 5/5, `e2e_bootstrap` 10/10,
  `e2e_tooling` PASS, `e2e_stdlib` PASS, `e2e_stdlib3` 9/9,
  `e2e_release` 6/6, `e2e_realworld` 7/7, `e2e_ecosystem` 23/23.
- `fmt --check` korpus: 87 file, drift 7 → 5 (2 diperbaiki slice
  ini; sisanya §4).
- Program `.ns` + IR `@main` OK (spot `hello.ns` via `e2e_release`).

## 4. Ditunda jujur (bukan blocker; calon slice/RFC berikut)

- `selfhost/parse.ns`: reflow struct-literal multi-baris (perilaku
  formatter pra-3.0, di luar pola `OP (`; tak disentuh).
- `std/csv.ns`: satu baris kosong; `std/logging.ns`: indentasi
  komentar `//` — kosmetik pra-3.0.
- `tests/string2_test.ns`: formatter menulis ulang spasi di dalam
  string literal (`"  hi"` → `" hi`) — perlu investigasi khusus
  (potensi mutasi konten string); SENGAJA tak dikanonisasi.
- `tutorials/04-generics-traits/main.ns`: generik berjarak
  `f < T: Show >` tak terdeteksi (isi `:`) — quirk pra-3.0.
- Temuan F1–F6 (`docs/realworld-3.0.md`), S2–S3
  (`docs/security-3.0.md`): tetap calon RFC pasca-3.0.
