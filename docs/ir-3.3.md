# Nusa IR 2.0 — Milestone 1: Analisis Control-Flow MIR (Phase 102)

Status: slice implementasi. Mandat master Phase 102 (AST → HIR →
MIR → Optimized MIR → LLVM IR + SSA/CFG/data-flow/lifetime/escape/
alias + verifier + contoh `nusa internal-ir-check`). NOL rewrite,
NOL rewire pipeline (aturan: tanpa design review).

## 1. Cakupan milestone (disengaja sempit)

- Verifier MIR (Phase 52) hanya properti lokal: terminator,
  def-use, target-ada, arity/tipe call, return. TAK ADA konsep
  penerus/pendahulu/terjangkau — block mati lolos diam-diam,
  padahal DCE (Phase 53) buta-block.
- Dipilih: infrastruktur CFG + eliminasi block tak-terjangkau.
  Bukan: HIR, SSA penuh, data-flow/lifetime/escape/alias,
  lowering AST→MIR, rewire pipeline (butuh design review).

## 2. Perubahan

- Baru `compiler/mir/Cfg.{h,cpp}` (`nusa::mir`, std-only,
  deterministik): `successors`, `predecessors`, `reachable`
  (BFS dari entry; entry hilang → kosong — verifier yang
  melapor), `rpo` (DFS iteratif; tanpa rekursi).
- `compiler/mir/Opt.{h,cpp}`: `eliminateUnreachableBlocks`
  (partisi sekali jalan; entry-hilang → diam; aman: block
  terbuang tak dirujuk block hidup mana pun menurut definisi),
  field `OptStats::blocksRemoved`, dipanggil sekali di awal
  `optimizeFunction` (sebelum loop fold+DCE).
- `CMakeLists.txt`: `Cfg.cpp` ke lib; `e2e_opt.sh`: `Cfg.cpp`
  ke kompilasi bench standalone (temuan saat verifikasi —
  link error tanpa itu).
- Regresi: `testCfg` (diamond + self-loop hidup + dead),
  `testEliminateUnreachableBlocks` (buang 2 block, verify
  pasca-bersih, idempoten, entry-hilang diam).

## 3. Diverifikasi

- Unit `test_mir` 13/13 (11 lama tak tersentuh angkanya +
  2 baru); `e2e_mir` 2/2; `e2e_opt` 4/4 (bench sintetik 3x:
  6002 → 2 instruksi, ~20ms — identik baseline 2.4.0 karena
  workload satu-block; verify pra/pasca ok).
- Pipeline `.ns` tak tersentuh (MIR belum rewire — by design);
  program `.ns` + IR `@main` OK.
- `nusa internal-ir-check file.ns` DITUNDA jujur: butuh lowering
  AST→MIR (dua jalur codegen divergen tanpa design review =
  dilarang aturan). CLI datang bersama lowering (Phase 103+).

## 4. Ditunda jujur

- HIR, SSA, data-flow/lifetime/escape/alias analysis.
- Pengetatan verifier atas reachable (LLVM sendiri mengizinkan
  block mati — verifikasi strict ditolak mengikuti preseden itu).
- Lowering + rewire + CLI `internal-ir-check`.
