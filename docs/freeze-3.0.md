# Nusa Language Freeze — Design (Phase 81, kandidat spesifikasi 3.0)

Status: implementasi slice ini. Mandat master: PHASE 81
(freeze syntax, type semantics, memory semantics, module
semantics, error semantics, concurrency semantics; hanya bug
fix + carefully approved improvements; tujuan = Nusa 3.0
specification candidate). Spec bahasa TAK BERUBAH.

## 1. Penomoran (keputusan tercatat)

Rilis ini BUKAN 3.0 (final di Phase 82+, beta di 84).
Versi compiler tetap **2.30.1**. Artefak Phase 81 = kandidat
spesifikasi + enforcement, nol perubahan bahasa/IR/ABI.
Bump versi ditunda sampai beta (menghindari klaim 3.0 prematur).

## 2. Basis freeze

- Normatif: `docs/language-spec-2.0.md` (289 baris, 15 topik +
  Appendix UB) + `e2e_spec2.sh` (6 kasus, hijau baseline).
- Delta 2.1→2.30 = murni library/paket/harness (stdlib,
  registry, server, dist, stabilitas); NOL sintaks/semantik
  baru sejak 2.0 (parser/checker/codegen tak tersentuh sejak
  09-11, diverifikasi Phase 80).
- Kandidat 3.0 = spec 2.0 DIKUNCI + lampiran freeze (§5 di
  dokumen kandidat), bukan rewrite.

## 3. Freeze scope (6 area, pemetaan ke spec)

- syntax → spec §1–§3 + `docs/syntax.md`.
- type semantics → spec §4,§8,§9,§10 + `types.md`.
- memory semantics → spec §5 + `memory-model.md` + Appendix A.
- module semantics → spec §7 + `modules.md`.
- error semantics → spec §6,§14 + `error-handling.md`.
- concurrency semantics → spec §11 + `concurrency.md`.

FFI/compile-time/macro/async = tetap di luar kontrak stabil
(UB/reserved, Appendix A); paket std non-normatif (§13).

## 4. Aturan perubahan selama freeze

- DILARANG: keyword/grammar baru, tipe baru, ownership/GC
  baru, modul/paket semantik baru, error shape baru,
  primitif konkurensi baru — tanpa RFC khusus.
- BOLEH: bugfix crash/hang/miscompile + perbaikan redaksional
  spec (klarifikasi tanpa mengubah makna), masing-masing
  tercatat di kandidat §6 + CHANGELOG.
- Slice ini: NOL bugfix bahasa ditemukan (sweep hijau);
  NOL improvement disetujui (antre ke RFC Phase 82+).

## 5. Enforcement (harness)

- `e2e_freeze.sh` (baru): (a) spec kandidat ada + 6 area
  terpetakan eksplisit; (b) `e2e_spec2` hijau; (c) guard
  sentuh-bahasa: `Parser.cpp/TypeChecker.cpp/Codegen.cpp/
  Lexer.cpp` mtime/baseline tak berubah dalam slice
  (dokumentatif — bukan ganti review).
- Ambang korpus `e2e_compat` tetap (81 file) sebagai jaring
  regresi semantik.

## 6. Baseline 2.30.1 (diukur ulang, Termux AArch64)

- `e2e_spec2` 6/6; `e2e_compat` 2/2 (46 ok + 35 lib).
- Full `ctest -j2`: 80/80 hijau (1230 dtk; final).

## 7. Dokumentasi

- Baru: `docs/language-spec-3.0-candidate.md` (kandidat,
  menunjuk 2.0 + lampiran freeze).
- File ini + CHANGELOG [Unreleased-3.0-candidate] (tanpa bump).
- Website regen (no drift).

## 8. Acceptance criteria

- Nol perubahan parser/checker/codegen/lexer.
- Kandidat memetakan 6/6 area + UB + conformance.
- `e2e_freeze` + `e2e_spec2` + `e2e_compat` hijau.
- Website no drift; `.ns.ll` bersih.
