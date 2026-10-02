# Verification Foundation — Design (Phase 71, target 2.22.0)

Status: RFC-0010 Proposed → Implementation slice 1
(properti + fuzz berseed + diferensial). Bukan proof penuh.
Spec bahasa tak berubah.

## 1. Invarian per tahap (kontrak)

- I-LEX totalitas: input byte arbitrer (termasuk NUL/non-UTF8)
  selalu selesai → token atau error TERBATAS; tak pernah
  sinyal-crash; `errors()` di-cap (ada `testLexerErrorCap`).
- I-PARSE recovery: `parse()` selalu kembali; error terbatas;
  `synchronizeTopLevel` terminating; nesting di-cap
  (`too deeply nested`).
- I-FMT idempotence: `fmt(fmt(x)) == fmt(x)`; `ok` stabil
  (reformat hasil-ok tetap ok + identik).
- I-CHECK determinisme: sumber sama → urutan diagnostik
  (kode+pesan) sama persis, dua run berurutan.
- I-MIR verify: modul hasil `MirBuilder` lolos `verify()`;
  `optimizeModule` mempertahankan verify dan tak menambah
  jumlah instr (`after <= before`).
- I-IR llvm-verify: setiap build sukses telah melewati
  `llvm::verifyModule` (enforce di `Codegen`; e2e mengunci
  lewat build korpus yang sukses).
- I-RUN diferensial: program check-ok yang terminating ⇒
  `stdout+exit(O0) == stdout+exit(O2)` dan
  `nusa run == nusa build+exec` (O0).

NON-tujuan eksplisit: bit-identik `.ll` antar-run
(path absolut boleh bocor); proof mekanis semantik.

## 2. Peta implementasi

- Unit `compiler/tests/test_verify.cpp` (deterministik penuh;
  keacakan semu = LCG seed tetap di-kode, bukan waktu):
  1. `lexTotality`: 256 byte tunggal + 4 KiB soup LCG +
     input kosong → selesai, error terbatas.
  2. `fmtIdempotent`: korpus ~8 sumber (fn/struct/enum/
     match/const/komentar) → ok ⇒ reformat identik + ok.
  3. `parseDeterministic` + 4. `checkDeterministic`:
     korpus ok + error → dump/diagnostik identik 2 run.
  5. `mirOptPreserves`: modul builder (foldable + cfg) →
     verify pra; optimize; verify pasca; instr tak naik.
- Generator `compiler/tests/fuzz_gen.py` (stdlib saja):
  `fuzz_gen.py <seed> <count> <outdir>` → `prog_<i>.ns`
  bounded-terminating (loop countdown literal, tanpa input,
  tanpa rekursi; print nilai terhitung ⇒ output deterministik).
- Orkestrasi `compiler/tests/e2e_verify.sh <nusa-bin>`
  (`NUSA_VERIFY_FUZZ_N`, default 40; `timeout` per-invokasi):
  diferensial O0≡O2 korpus inline; run≡build+exec; fuzz loop
  (oracle: rc>128 = CRASH→gagal; check 2× rc sama;
  check-ok ⇒ run O0≡O2≡run-cmd 2×).
- CMake: `test_verify` + `e2e_verify`.

## 3. DITUNDA (butuh RFC lanjutan)

LibFuzzer/AFL, proof assistant, snapshot pesan-error,
bit-identik IR, verifikasi backend-LLVM itu sendiri,
concurrency nondeterminism oracle (thread/channel butuh
model happens-before — di luar slice 1).

## 4. Keamanan (ringkas)

Fuzz berjalan sebagai skrip shell biasa atas biner yang sama
+ `timeout`; tanpa eksekusi kode asing; generator murni
Python stdlib; tak ada jaringan; artefak di `mktemp -d`.

## 5. Acceptance criteria

- `test_verify` hijau (5 properti); `e2e_verify.sh` hijau
  (diferensial + fuzz default + no-crash); suite lain hijau;
  bench identik; `.ns.ll` dibersihkan.
