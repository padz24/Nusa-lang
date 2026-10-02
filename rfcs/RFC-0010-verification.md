# RFC-0010: Formal Verification Foundation

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-11
- Menggantikan: —
- Digantikan oleh: —

## Summary

Meletakkan fondasi verifikasi bertahap untuk pipeline Nusa
(lexer → parser → semantic/checker → MIR → LLVM IR → runtime):
invarian eksplisit per tahap, property test deterministik,
fuzz harness berseed, dan differential test (O0 vs O2,
`run` vs `build`+exec). Bukan verifikasi formal penuh
(model checking/proof assistant) — itu follow-up.

## Motivation

Contoh yang sakit hari ini: keyakinan "opt tak mengubah
semantik" hanya dijaga satu bench sintetik (`e2e_opt.sh`);
fmt idempotence hanya golden kecil; determinisme checker
tak dikunci; crash-freedom atas input arbitrer hanya
sampel tangan (`e2e_stability.sh`). Tiap tahap butuh
kontrak tertulis + pengunci mesin yang murah dijalankan
di Termux.

## Design

Kontrak per tahap (normatif-informal; detail invariannya di
`docs/verification-2.22.md`):

- I-LEX totalitas: byte arbitrer → token atau error terbatas,
  tanpa crash (sinyal) dan tanpa hang.
- I-PARSE recovery: `parse()` selalu kembali; error terbatas;
  sinkronisasi top-level terminating (cap nesting ada).
- I-FMT idempotence: `fmt(fmt(x)) == fmt(x)`; status-ok stabil.
- I-CHECK determinisme: sumber sama → diagnostik sama persis.
- I-MIR verify: modul hasil builder lolos `verify()`;
  `optimizeModule` mempertahankan verify + tak menambah instr.
- I-IR llvm-verify: build sukses ⇒ `llvm::verifyModule` lolos
  (sudah dienforce `Codegen`; dikunci konformans).
- I-RUN diferensial: stdout+exit O0 ≡ O2; `run` ≡ `build`+exec.

Mesin (slice 1, fase ini):

- Unit `test_verify` (deterministik, seed tetap di-kode):
  totalitas lexer, idempotence fmt, determinisme parse/check,
  preservasi MIR-opt.
- Generator `compiler/tests/fuzz_gen.py` (stdlib saja,
  `--seed/--count`): program `.ns` bounded-terminating.
- Orkestrasi `compiler/tests/e2e_verify.sh`: diferensial
  korpus + fuzz loop (oracle: tanpa-crash, check deterministik,
  check-ok ⇒ run O0≡O2≡run-cmd).

## Alternatives

- LibFuzzer/AFL: ditolak untuk slice 1 (toolchain + waktu
  perangkat; harness berseed memberi 80% nilai tanpa infra).
- Proof assistant (Lean/Coq) untuk semantik: ditolak sekarang
  (biaya model ≫ manfaat; dibuka sebagai RFC lanjutan bila
  invarian slice 1 stabil).
- Snapshot test seluruh korpus: ditolak (rapuh terhadap
  perubahan pesan error yang sah; dipilih properti, bukan teks).

## Compatibility

Nol perubahan bahasa/IR/ABI. Murni aditif: 1 biner uji
+ 1 generator + 1 skrip e2e + docs. Budget CI: fuzz default
kecil (puluhan program), timeout per-invokasi.

## Tests

- `test_verify` hijau (5 properti).
- `e2e_verify.sh` hijau (diferensial korpus + fuzz + no-crash).
- Suite existing hijau (properti tak boleh mengubah perilaku).

## Specification

Spec bahasa tak berubah. Menyentuh `docs/compiler.md`
(+1 baris fase) dan `docs/verification-2.22.md` (baru).

## Discussion

- 2026-09-11: slice 1 dibatasi pada properti murah +
  diferensial level-O LLVM; bit-identik IR antar-run
  eksplisit NON-tujuan (path absolut bisa bocor ke IR).
