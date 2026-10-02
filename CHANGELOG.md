# Changelog Nusa

## [Unreleased: 3.2-diff] — Phase 126: DIFFERENTIAL TESTING (milestone 1)

Matriks 5×10 + TEMUAN bug cache (ditutup).

- `e2e_diff` 5/5 (default/O0-Oz/-g/profil; stdout+exit identik).
- Bug: Fresh basi lintas-output se-dir → `Fingerprint::out`.
- Diverifikasi: diff + incr + verify + release hijau.
- Batas: `docs/diff-3.27.md` (run host-only; rebuild-compiler no).

## [Unreleased: 3.2-verify] — Phase 125: COMPILER VERIFICATION (milestone 1)

Properti metamorfik fmt-menjaga-AST (model-check/proof ditolak
beralasan). Properti MENANGKAP bug string-mutasi (diperbaiki).

- `e2e_fmtprop` 3/3 (det + korpus 94 + fuzz 25) + regresi unit.
- Cleanup formatter sadar-string/komentar (isi tak tersentuh).
- Diverifikasi: fmt/verify/release hijau.
- Evaluasi metode: `docs/verify-3.26.md`.

## [Unreleased: 3.2-latency] — Phase 124: REAL-TIME/LOW-LATENCY (milestone 1)

Profil ukur + aturan (tanpa flag; tanpa klaim hard real-time).

- Vonis: lunak-terdokumentasi YA, hard TIDAK (butuh bukti formal).
- `bench/latency.sh` (jitter ping-pong; acuan 90/100/119/29ms)
  + aturan 5 profil + `e2e_latency` 3/3.
- Diverifikasi: latency + release hijau.
- Batas: `docs/latency-3.25.md` (prioritas/WCET = follow-up).

## [Unreleased: 3.2-min] — Phase 123: EMBEDDED/FREESTANDING (milestone 1)

Seam alokator weak (bukan freestanding penuh — tercatat).

- `nusa_alloc`/`nusa_free` (default malloc/free; timpa via
  `--link`); 5 situs codegen dialihkan; deteksi minimal + ukuran
  utuh (hello 5816 B).
- Terbukti: penghitung (==1) + NULL (OOM bersih).
- Regresi: `e2e_min` +2; kunci simbol `e2e_runtime` diperbarui
  sadar (+chan_new_bounded/try_send Fase 118 yang tertinggal +
  nusa_alloc/free weak).
- Diverifikasi: runtime/memsafe/concurrency/stdlib/release hijau.
- Batas: `docs/min-3.24.md` (entry/no_std = follow-up).

## [Unreleased: 3.2-simd] — Phase 122: SIMD/VECTOR TYPES (milestone 1)

RFC tipe vektor (aturan: fitur bahasa via RFC dulu).
TANPA implementasi; autovek LLVM terbukti nol (bukan janji).

- Baru `rfcs/RFC-0013-vectors.md` (Proposed): `vector<i64,N>`
  slice-1, elementwise, index fail-fast, reduksi, penurunan
  `<N x i64>`, ABI per-target, alternatif ditolak berbukti.
- Bukti: loop reduksi + `array_sum` di `-O2` = 0 instruksi SIMD.
- Diverifikasi: `e2e_rfc` (13 RFC); korpus `vector` = C++ saja.
- Batas: Accepted menunggu sintaks-N + ABI + prototipe run.

## [Unreleased: 3.2-parallel] — Phase 121: PARALLEL COMPUTING (milestone 1)

Loop paralel level-pustaka (tanpa sintaks; SIMD = Phase 122).

- `std/parallel.ns`: `par_map_sum` (block statik, deterministik).
- Regresi: `tests/parallel_test.ns` + `e2e_parallel` 3/3.
- Diverifikasi: concurrency/stress/release hijau.
- Batas + tunda: `docs/parallel-3.22.md`.

## [Unreleased: 3.2-gpu] — Phase 120: GPU/ACCELERATOR (milestone 1)

Eksplorasi + seam vendor-netral (terbukti di perangkat: 1/1/GPU).

- Fakta env + batas FFI (tanpa pointer) → desain seam paket.
- `packages/opencl/`: query angka (platform/device/kelas) via
  dlopen; wrapper + manifest + README.
- Regresi: `e2e_gpu` 2/2 (kontrak struktural + konsistensi).
- Diverifikasi: ffi + release hijau.
- Tunda + terima: `docs/gpu-3.21.md` (nama/buffer/kernel).

## [Unreleased: 3.2-disttool] — Phase 119: DISTRIBUTED TOOLING (milestone 1)

Tracing v1 (`inspect` = follow-up desain).

- `std/trace.ns`: request ID (env/propagasi atau bangkitan),
  span/event JSON sebaris; `nusa trace run` (injeksi ID) +
  `nusa trace check` (validator bentuk + ringkasan).
- Regresi: `tests/trace_test.ns` + `e2e_trace` 4/4.
- Diverifikasi: tooling + release hijau.
- Batas + tunda: `docs/dist-tooling-3.20.md`.

## [Unreleased: 3.2-dist] — Phase 118: DISTRIBUTED PROGRAMMING (milestone 1)

Backpressure (sisanya sudah ada sejak 2.28–2.30).

- `chan_new_bounded` + `chan_try_send` (runtime/checker/codegen/
  LSP); cap≤0 fail-fast; OOM cermin drop.
- Temuan: grup void codegen tanpa-guard → guard eksplisit
  (print-macet/segfault pada builtin bernilai baru).
- Regresi: unit + `e2e_concurrency` +3.
- Diverifikasi: concurrency/stress/memsafe/release/test hijau.
- Ditunda: bertag/timeout-send/select/escape (prasyarat).

## [Unreleased: 3.2-actor] — Phase 117: ACTOR/MESSAGE-PASSING (milestone 1)

Pustaka murni-Nusa (tanpa sintaks, tanpa RFC). Sintaks `actor`
butuh RFC (bukan slice ini).

- `std/actor.ns`: `ActorRef{mb,lock}` + `spawn/call/cast`,
  handler fungsional menjawab-sendiri; triple atomik via mutex.
- Regresi: `tests/actor_test.ns` + `e2e_actor` 3/3 (stres 4×25
  = tepat 100).
- Diverifikasi: concurrency/stress/stdlib3/release hijau.
- Batas + tunda: `docs/actors-3.18.md`.

## [Unreleased: 3.2-concurrency] — Phase 116: SAFE CONCURRENCY MODEL (milestone 1)

Model share-nothing + handle; TUTUP TOCTOU double-join konkuren
(double-free + 0 diam-diam → klaim atomik, kalah fail-fast).

- `runtime/rt_thread.c`: claim-first cermin `*_free`; NOL ABI.
- Model + vonis send/sync: `docs/concurrency-safe-3.17.md`.
- Regresi: `s_racejoin` 3x di `e2e_stress`.
- Diverifikasi: concurrency/memsafe/async/stress/release hijau.
- Ditunda: free-vs-pakai balapan (refcounting), ABA, select.

## [Unreleased: 3.2-memory] — Phase 115: MEMORY MODEL 3.0 (milestone 1)

Audit 8 area. NOL semantik; disiplin alokasi bersih (temuan kode
nihil) — nilai = matriks + residual + kunci.

- `docs/memory-3.16.md`: stack/heap/ownership/borrow/refs/
  concurrency/FFI/OOM + register ABA/rekursi/contract.
- Kunci: `oom-coverage` (`malloc`==`oom-check` di IR).
- Diverifikasi: memsafe + release hijau.
- Ditunda: id-generasional/probe/borrow/`unsafe` (RFC/desain).

## [Unreleased: 3.2-chain] — Phase 114: SUPPLY-CHAIN SECURITY (milestone 1)

Preseden terlihat + build beku. Tanpa kripto baru.

- Audit: registry pertama menang (by-design); pin mengunci sumber
  pasca-update; jendela = resolusi segar + fetch-diam build.
- Warning shadowing multi-registry (termasuk transitif) +
  `pkg build --frozen` (tolak fetch saat build).
- Regresi: `e2e_supply` 4/4 (frozen ×3, shadow + checksum).
- Diverifikasi: pkg/registry/eco2/release hijau.
- Fakta + tunda: `docs/supplychain-3.15.md`.

## [Unreleased: 3.2-supply] — Phase 113: SECURITY-FIRST PACKAGE ECOSYSTEM (milestone 1)

SBOM tanpa kriptografi baru. Signatures butuh primitif asimetris
teraudit (ditunda beralasan); publishers/vuln butuh feed (Phase 114).

- `nusa sbom` (+`pkg sbom`): `nusa-sbom/1` dari manifest+lock
  (checksum, capabilities, cached, `"signatures":false` eksplisit).
- Regresi: `e2e_sbom` 3/3 (fixture, JSON valid, determinisme).
- Diverifikasi: eco2 + release hijau.
- Rasional + batas: `docs/supply-3.14.md`.

## [Unreleased: 3.2-plugin] — Phase 112: PLUGIN SYSTEM (milestone 1)

Kontrak `nusa-plugin/1` tersandbox-proses (WASM = migrasi).

- `nusa plugin run` (manifest opsional + gate op pra-eksekusi +
  batas sandbox); `runSandboxedInput` (stdin); parser JSON subset.
- Regresi: `test_plugin` 4/4 + `e2e_plugin` 5/5 (roundtrip 42,
  refusal, timeout, build-fail).
- Diverifikasi: playground/sec/release hijau (jalur lama utuh).
- Model + batas: `docs/plugin-3.13.md`.

## [Unreleased: 3.2-wasi] — Phase 111: WASI/SANDBOXED EXECUTION (milestone 1)

Model kapabilitas eksplisit (link/exec wasm penuh butuh sysroot
+ engine yang belum ada di CI — tercatat + kriteria terima).

- Gate: link `wasm32*` memakai 10 kapabilitas runtime → tolak
  bernama (±sysroot); murni → sysroot-message lama; inspeksi bebas.
- Regresi: `e2e_wasi` 4/4; `e2e_cross` tak tersentuh (byte-stabil).
- Model keamanan + matriks: `docs/wasi-3.12.md`.
- Diverifikasi: wasi + cross + release hijau.

## [Unreleased: 3.2-ffi] — Phase 110: NATIVE INTEROPERABILITY 3.0 (milestone 1)

`nusa headers`: Nusa → C (kebalikan bindgen). C++ tetap di luar
(subsystem terpisah, tanpa komitmen).

- Subset jujur-ABI (i64/string/void + struct/enum datar);
  bool/closure/generik/metode ditolak eksplisit + warning.
- Terbukti link C↔Nusa (`42/7 p/7 0`); resep embedding di header.
- Regresi: `test_headers` 4/4 + `e2e_headers` 4/4.
- Diverifikasi: bindgen/abi/release hijau.
- Ditunda: C++, objek-tanpa-main, `abi-check` CLI.

## [Unreleased: 3.2-abi] — Phase 109: ABI & BINARY INTERFACE (milestone 1)

Spesifikasi ABI atas codegen 3.0.0 (NOL perubahan codegen).

- Baru `docs/abi-3.10.md`: CC default, primitif (bool=i1,
  string=ptr), struct non-packed, enum {tag:i64,slot},
  closure {ptr,ptr}+trampolin, mangling, batas Nusa/C/platform.
- Kunci mesin `e2e_abi` 6/6 (struct/enum/mangle/closure/ffi/cc).
- Diverifikasi: `e2e_abi` + `e2e_release` hijau.

## [Unreleased: 3.2-reflect] — Phase 108: REFLECTION & INTROSPECTION (milestone 1)

Evaluasi + metadata tooling. Runtime reflection TIDAK dibangun
(beralasan); `typeof` level-bahasa ditunda ke RFC.

- `nusa doc --format json`: metadata mesin (fungsi/struct/alias/
  const/enum/trait/extern + tipe + doc) dari data yang sama
  dengan Markdown; flag opt-in, Markdown byte-identik.
- Regresi: `test_doc` 11/11 + `e2e_tooling` JSON.
- Diverifikasi: tooling + `e2e_release` hijau.
- Skema + vonis: `docs/reflection-3.9.md`.

## [Unreleased: 3.2-metapro] — Phase 107: ADVANCED METAPROGRAMMING (milestone 1)

RFC makro deklaratif (aturan: fitur bahasa via RFC dulu).
TANPA implementasi; ambang Phase 69 tetap berlaku.

- Baru `rfcs/RFC-0012-macros.md` (Proposed): sintaks
  pattern→template, argumen = fragmen terparse, substitusi
  AST, hygiene definisi-vs-call-site, budget 64×65536,
  determinisme, stack ekspansi di diagnostik; prosedural
  ditunda (butuh API+sandbox Phase 137/138).
- Diverifikasi: `e2e_rfc` 3/3 (12 RFC); korpus `macro` =
  tabel-keyword saja (nol konstruksi).
- Batas: Accepted menunggu ≥2 pola nyata + review + tooling.

## [Unreleased: 3.2-repl] — Phase 106: NUSA REPL (milestone 1)

`nusa repl`: AOT-loop (transkrip sesi → runBuild → eksekusi).
Infrastruktur sama; tanpa bahasa kedua; tanpa JIT.

- Konvensi: `;`/`{`/`}` = statement; ekspresi → `print(...)`;
  `:help`/`:reset`/`:quit`; gagal tak meracuni sesi; prompt
  hanya TTY; blok multi-baris sadar-string.
- Semantik putar-ulang (jujur AOT): tiap giliran mengeksekusi
  seluruh sesi — didokumentasikan + dikunci e2e.
- Regresi: `e2e_repl` 5/5. Batas: tanpa fn-def/timeout/readline,
  biaya rebuild per baris (JIT follow-up).
- Diverifikasi: `e2e_repl` + `e2e_release` hijau.

## [Unreleased: 3.2-jit] — Phase 105: JIT/AOT ARCHITECTURE (milestone 1)

Arsitektur eksekusi, bukan subsystem. AOT tetap satu-satunya jalur
(NOL perilaku). LLVM/Termux terbukti mampu ORC LLJIT in-process.

- Pagar kapabilitas `test_orc` (ctest): modul JIT + simbol proses.
- `docs/execution-3.6.md`: seam JIT, opsi simbol runtime, state
  inkremental, interpreter ditunda beralasan.
- Diverifikasi: `test_orc` + `e2e_release` hijau.

## [Unreleased: 3.2-pgo] — Phase 104: PROFILE-GUIDED OPTIMIZATION (milestone 1)

Alur PGO via clang (LLVM tetap optimizer). Deviasi tercatat:
`--profile` milik Phase 62 → namespace `--pgo-*`.

- `build --pgo-generate` (`-fprofile-generate`) + `build
  --pgo-use=<profdata>` (`-fprofile-use=`; Exec pre-check +
  arahan merge); `pkg build` meneruskan; kombinasi ditolak.
- Cache safety: `Fingerprint::pgo` (`""|generate|use:<sha>`)
  — biner base/instrumented/opt tak tertukar.
- Regresi: `e2e_pgo` 4/4 (loop fib 6765 identik, missing,
  exclusive, cache).
- Diverifikasi: `test_incr`/`test_pkg` + `e2e_release` hijau.
- Reproducibility: PGO menggugurkan jaminan repro (§61) —
  aturan di `docs/pgo-3.5.md`.

## [Unreleased: 3.2-opt] — Phase 103: ADVANCED OPTIMIZER (milestone 1)

Inlining MIR. NOL rewire; LLVM tetap optimizer utama. Propagasi
konstanta sudah ada (fold per-fungsi); devirtualisasi N/A.

- `Opt::inlineSmallCalls`: callee satu-block (≤32 instr, bukan
  caller, arity/void cocok) disalin ke situs call; id di-remap,
  slot di-rename `.inlN`, hasil ret disubstitusi; sekali jalan.
- Regresi: 3 unit baru (`test_mir` 16/16).
- Diverifikasi: `e2e_mir` + `e2e_opt` (reduksi bench identik) +
  `e2e_release` hijau; `.ns` + IR OK.
- Ditunda: multi-block/adaptif/dead-fn/loop/escape (prasyarat).

## [Unreleased: 3.2-ir] — Phase 102: NUSA IR 2.0 (milestone 1)

Analisis control-flow MIR. NOL rewire pipeline; NOL HIR/SSA penuh
(butuh design review). Semantik `.ns` tak berubah.

- Baru `compiler/mir/Cfg.{h,cpp}`: successors/predecessors/
  reachable/rpo (std-only, deterministik, tanpa rekursi).
- `Opt::eliminateUnreachableBlocks` (+ `OptStats::blocksRemoved`),
  sekali di awal `optimizeFunction`; `e2e_opt.sh` link `Cfg.cpp`
  (temuan link-error bench standalone).
- Regresi: `testCfg` + `testEliminateUnreachableBlocks`
  (`test_mir` 13/13).
- Diverifikasi: `e2e_mir` + `e2e_opt` (bench identik 2.4.0) hijau;
  `.ns` + IR OK.
- Ditunda jujur: HIR/SSA/data-flow/lifetime/escape/alias,
  lowering+rewire, CLI `internal-ir-check` (butuh lowering).

## [Unreleased: 3.2-frontend] — Phase 101: COMPILER FRONTEND 2.0 (milestone 1)

Akurasi diagnostik. NOL semantik; NOL rewrite. Bukti pemilihan:
frontend ≤8% build (`--stats`; clang >90%) → micro-opt ditolak;
recovery/fuzz-101 (72/72)/adversarial bersih; HIR tak ada
(ranah Phase 102).

- 17 string user-facing berstempel versi basi → dibersihkan
  (`TypeChecker.cpp` 15 situs, `Parser.cpp` hint `expected
  statement` → contoh aktual, `Pkg.cpp` audit).
  Prefix pesan dipertahankan (seluruh e2e/unit mencocokkan prefix).
- Regresi: `testStmtHintListsCurrentForms` (`test_parser`).
- Diverifikasi: unit parser/typecheck/pkg + e2e alias/fn/traits/
  eco2/diagnostics/error/hardening/tooling + `e2e_release` 6/6
  hijau; `.ns` + IR OK.
- Ditunda: quirk fmt warisan, HIR, incremental parsing (Phase 102+).

## [Unreleased: 3.1-foundation] — Phase 100: NUSA 3.1 FOUNDATION (audit)

Audit 10 area (compiler/runtime/stdlib/pkg/registry/LSP/debugger/
formatter/FFI/cross). Freeze 3.0 dipertahankan: NOL semantik
baru; satu BUGFIX formatter non-semantik. Versi tetap 3.0.0.

- Bugfix FMT-1: `(` grouping sesudah operator menempel
  (`a - (b)` → `a -(b)`, `= (a)` → `=(a)` di
  `compiler/fmt/Formatter.cpp`); kini spasi cermin `[`
  (`+ - * / = , : return`; call/unary menempel;
  perbandingan/panah status quo — lihat `docs/foundation-3.1.md`).
- Regresi: unit `testParenGroupingSpacing` (`test_fmt` 18/18);
  kanonisasi `selfhost/lex.ns` + `std/math.ns` (diff spacing saja).
- Baseline: `bench/suite.sh` 35 metrik `perf:` (incr ≈5.3×;
  startup 86.5 ms; LSP init 403.8 ms; angka perangkat-spesifik).
- Diverifikasi: fmt/selfhost/parse/bootstrap/tooling/stdlib/
  stdlib3/release/realworld/ecosystem hijau; `fmt --check`
  korpus 87 file (drift 7→5, sisa didokumentasikan §4).
- Ditunda jujur: reflow struct `parse.ns`, kosmetik csv/logging,
  tulis-ulang spasi string `string2_test.ns` (butuh investigasi,
  tak disentuh), generik berjarak tutorial-04, F1–F6/S2–S3.

## [3.0.0] — Phase 89: NUSA 3.0.0 FINAL

Rilis final stabil. NOL perubahan bahasa/arsitektur/API/format
sejak beta; delta rc.1 → final = hapus suffix + gate + record.

- Label: `NUSA_VERSION_STR 3.0.0`; help `RC` → `Stable`;
  `project()` = `3.0.0` (deb `3.0.0-1`).
- Baru: `compiler/tests/e2e_release.sh` (baner final eksak +
  DoD artefak §44 + tooling + build-all + IR `@main`) +
  registrasi `e2e_release`; `docs/release-3.0.md` (DoD 35/35).
- Jaga regresi: `e2e_beta`/`e2e_rc` terima final
  (build-all/freeze dipertahankan); `e2e_new`/`e2e_repro`
  tanpa ubah; stdlib header → `v3.0.0`.
- Diverifikasi: `e2e_release` 6/6 + full `ctest -j2` 87/87
  hijau (1939 dtk); website regen + `--check` no-drift.

## [3.0.0-rc.1] — Phase 88: NUSA 3.0 RELEASE CANDIDATE

Rilis kandidat pertama. NOL perubahan bahasa/arsitektur/API/
format sejak beta (beku); delta = label `3.0.0-rc.1` + gate RC
+ record + website regen. rc.2+ hanya bila diperlukan.

- Label: `NUSA_VERSION_STR 3.0.0-rc.1`; help `Beta` → `RC`
  (`main.cpp`); `project()` tetap `3.0.0` (deb `3.0.0-1`).
- Baru: `compiler/tests/e2e_rc.sh` (baner eksak + freeze +
  build-all + spot-run + IR `@main` + bersih) + registrasi
  `e2e_rc` di CMake; `docs/release-3.0-rc.1.md`.
- Jaga regresi: `e2e_beta` banner `beta|rc.*` (build-all
  dipertahankan); `e2e_new`/`e2e_repro` lolos tanpa ubah
  (suffix-aware/dinamis); stdlib header → `v3.0.0-rc.1`.
- Diverifikasi: `e2e_rc` 5/5 + `e2e_beta` 4/4 + full `ctest -j2`
  86/86 hijau (1165 dtk); website regen + `--check` no-drift.

## [Unreleased: 3.0-security] — Phase 87: SECURITY AUDIT

Audit 9 area × 6 kelas: 0 critical, 0 high; 1 medium
diperbaiki (S1); 2 info. Rilis tak terblokir.

- Fix S1: `--memory-mb 0` fail-open → ditolak (`sandbox` +
  `playground`); regresi `sec-limits*` (`e2e_sec` 5→9).
- Baru: `docs/security-3.0.md` (matriks + temuan S1–S3).
- Probing: injeksi-shell via nama-file (bersih), dual
  path+version (ditolak), adversarial 15+ input (0 sinyal).
- Diverifikasi: sec 9/9 + full suite hijau; website regen.

## [Unreleased: 3.0-perf] — Phase 86: PERFORMANCE VALIDATION

Suite benchmark resmi (`bash bench/suite.sh`): 8 dimensi
(compile/incremental/size/startup/runtime/memory/pkg/LSP).
NOL perubahan bahasa/arsitektur (freeze); NOL klaim tanpa
angka (§4 `docs/perf-3.0.md`).

- Baru: `bench/suite.sh` (35 metrik `perf:`) + `docs/perf-3.0.md`
  + `e2e_perf.sh` (4 kasus: suite/dims/incr/size).
- Angka beta: incr 5.5×; startup 42.8 ms; LSP ~200 ms;
  semua ≈ baseline 2.30.1 dalam varian perangkat.
- Diverifikasi: perf 4/4 + full suite hijau; website regen.

## [Unreleased: 3.0-realworld] — Phase 85: REAL-WORLD VALIDATION

Enam proyek nyata murni-Nusa (`realworld/`): wc, todohttp,
kvstore, nsvet, nstok, csvsum. NOL perubahan bahasa (freeze);
satu BUGFIX compiler (diizinkan): `file_exists` basi (F4).

- Bugfix F4: slot `fe.res` tak-diinit → `file_exists` bisa
  true-palsu pada panggilan berulang satu situs; regresi
  `filesloop` di `e2e_stdlib.sh`.
- Baru: `e2e_realworld.sh` (7 kasus); korpus 81→87 file
  (`e2e_compat`/`e2e_beta` mencakup `realworld/`;
  `e2e_migrate` 82/3/0/0/2).
- Temuan desain F1–F6 di `docs/realworld-3.0.md` (calon RFC
  pasca-3.0, bukan blocker).
- Diverifikasi: realworld 7/7 + full suite hijau; website regen.

## [3.0.0-beta] — Phase 84: NUSA 3.0 BETA

Rilis beta. Freeze: semantik bahasa, arsitektur compiler, API
stdlib, format paket. NOL perubahan bahasa sejak 2.30.1
(label versi + harness saja).

### Catatan rilis
- `nusa --version` → `3.0.0-beta`; help → Beta; stdlib
  header v3.0.0-beta; paket `.deb` tetap numerik `3.0.0-1`.
- Template `nusa new` mencatat `compiler = "3.0.0-beta"`;
  lock lama (2.x) diminta `pkg update` (perbandingan semver
  sadar-pra-rilis).
- Conformance baru: `e2e_beta.sh` (baner versi + build
  seluruh korpus runnable).

### Baterai beta (hasil diisi pasca-verifikasi final)
- Full `ctest -j2`: 83/83 hijau (1056 dtk).
- Fuzz: seed bawaan 52/52 + 424242 52/52 + seed beta 30084
  (0 mismatch/0 crash/0 hang, 40 program).
- Bench: closureapply 1.91 s / 6776 B / 0.15 s; biner
  1623920 B; parse.ns 3.4 s (≈ baseline 2.30.1).
- Real-world: `e2e_beta` build-all 46 program hijau.

## [Unreleased: 3.0-audit] — Phase 83: COMPATIBILITY AUDIT

Audit korpus 2.x: 76 compatible, 3 deprecated (channel
tanpa-tag), 0 requires migration, 0 broken, 2 unsupported
(paket native tls/sqlite) — 81 file. NOL migrasi bahasa.

- Baru: `nusa migrate [PATH]` (audit read-only 5 kategori,
  exit 1 bila requires-migration/broken > 0).
- Baru: `docs/migration-3.0.md` (guide + tabel audit) +
  `e2e_migrate.sh` (3 kasus).
- Diverifikasi: migrate + full suite hijau; website regen.

## [Unreleased: 3.0-final] — Phase 82: NUSA 3.0 SPECIFICATION

Spesifikasi final `docs/language-spec-3.0.md` (normatif,
19 topik mandated, independen implementasi §21); NOL
perubahan sintaks/semantik (ratifikasi freeze 2.30.1).

- Baru: `e2e_spec3.sh` (7 kasus: markers/sections-19/split/
  independence/traceability/probe-wrap/probe-reject).
- Pointer normatif dialihkan 2.0 → 3.0 (`spec.md`);
  kandidat Phase 81 diarsipkan.
- Diverifikasi: spec3 + spec2 + freeze + compat hijau;
  website regen.

## [Unreleased: 3.0-candidate] — Phase 81: LANGUAGE FREEZE

Freeze 6 area (sintaks, tipe, memori, modul, error,
konkurensi); NOL sintaks/semantik baru; versi compiler tetap
2.30.1 (final 3.0 di Phase 82+, beta di 84).

- Baru: `docs/language-spec-3.0-candidate.md` (lampiran
  freeze atas spec-2.0 normatif) + `docs/freeze-3.0.md`.
- Baru: `e2e_freeze.sh` (4 kasus: files/map/normative/spot).
- Diverifikasi: spec2 + compat + freeze hijau; website regen.

## [2.30.1] — 2026-09-12 (Phase 80: STABILITY RELEASE)

Freeze fitur bahasa (master Phase 80). Nol sintaks/semantik
baru; nol perubahan parser/checker/codegen. Penomoran: master
menarget "2.5.0" ganda (stale) — rilis ini patch monoton.

### Stabilitas
- Cache registry hermetik per-suite (`HOME=$TMPDIR/home` di
  eco2/registry/sec — flake paralel tamper-verify).
- `e2e_compat.sh` (baru): check 81 file (46 ok + 35 lib
  tanpa-main terkunci) + build subset.
- Fuzz seed-2: TEMUAN — generator non-terminasi (diperbaiki
  2 lapis: frozen + oracle rc=124); 6 seed hijau.

### Keamanan / performa / kompatibilitas / docs
- Audit runtime: tanpa strcpy/strcat/sprintf/gets; extern
  return-string tetap ditolak; hardening/sec hijau.
- Baseline: bench 2.06 s / 6776 B / 0.17 s; biner 1522248 B;
  parse.ns build 3.7 s.
- `standard-library.md` v2.30.1; catatan freeze; check-links;
  website regen.

### Diverifikasi
- Full `ctest -j2` 79/79 hijau; `.ns` + IR OK; `.ns.ll` bersih.

## [2.30.0] — 2026-09-12 (Phase 79: DISTRIBUTED — fondasi)

Primitif library-level, mayoritas non-compiler (master Phase 79;
syarat concurrency-stabil terpenuhi). Murni-Nusa; nol perubahan
bahasa/IR/ABI.

### Ditambahkan
- `std/rpc.ns`: JSON-RPC-min atas TCP newline (call/serve_once
  + helper amplop; params mentah utuh).
- `std/discovery.ns`: endpoint statis + round-robin + split.
- `std/timeout.ns`: task+poll deadline (abandon terdokumentasi).
- `std/retry.ns`: policy + backoff (hasil terakhir).
- `docs/dist-2.30.md`; `e2e_dist.sh` (4 kasus); 2 file
  `tests/*_test.ns`.

### Diverifikasi
- RPC Nusa↔Nusa lokal + unknown-method + port-tutup;
  `nusa test` +2; full `ctest -j2` hijau; `.ns` + IR OK;
  `bench.sh` identik 2.29.0.

## [2.29.0] — 2026-09-12 (Phase 78: SERVER — fondasi)

Primitif aplikasi server, framework-agnostic (master Phase 78).
Murni-Nusa + paket referensi; nol perubahan bahasa/IR/ABI.

### Ditambahkan
- `std/config.ns`: env > TOML > fallback (str/int/bool;
  false/0 eksplisit menang via has_key).
- `std/metrics.ns`: struct eksplisit + render Prometheus.
- `packages/tls/`: shim OpenSSL (verifikasi peer+hostname
  selalu aktif) + wrapper + manifest + README.
- `toml_has_key`; `docs/server-2.29.md`; `e2e_server.sh`
  (8 kasus); 2 file `tests/*_test.ns`.

### Diverifikasi
- TLS terverifikasi-penuh lokal (CA salah/port tutup → -1);
  config/metrics golden; async/serve smoke; full `ctest -j2`
  hijau; `.ns` + IR OK; `bench.sh` identik 2.28.0.

## [2.28.0] — 2026-09-11 (Phase 77: DATA TOOLING — serialisasi + sqlite)

Tanpa engine di stdlib (mandat). Murni-Nusa + paket referensi;
nol perubahan bahasa/IR/ABI.

### Ditambahkan
- `std/csv.ns`: baris/field index-getter (quote, `""`, CRLF).
- `std/json.ns`: getter skalar objek-datar + valid dangkal.
- `std/toml.ns`: getter (section, key) skalar-baris.
- `packages/sqlite/`: shim C + wrapper + manifest `[link]`
  + README (pola shim didokumentasikan di `ffi.md`).
- `docs/data-2.28.md`; `e2e_data.sh` (7 kasus); 3 file
  `tests/*_test.ns`.

### Diverifikasi
- Serialisasi golden + `nusa test` hijau; CRUD `:memory:`
  via jalur paket (init/add/update/build) + negatif;
  full `ctest -j2` hijau; `.ns` + IR OK; `bench.sh` identik 2.27.0.

## [2.27.0] — 2026-09-11 (Phase 76: CRYPTOGRAPHY — fondasi)

Primitif kripto jika kualitas dijamin (master Phase 76 lulus
bersyarat). Tanpa algoritma baru; tanpa klaim "teraudit".

### Ditambahkan
- Builtin `sha256_hex(s)` (FIPS 180-4 verbatim di
  `runtime/rt_crypto.c`; vektor NIST + cross-check independen).
- Builtin `crypto_rand_hex(n)` (`/dev/urandom` langsung;
  gagal-eksplisit `""`; tanpa fallback diam).
- `std/crypto.ns`: crypto_key_hex (fail-closed), crypto_compare
  (tanpa early-exit), sha256_file; `hex_valid_any` (encoding).
- RFC-0011 (Proposed) + `docs/crypto-2.27.md`;
  `e2e_crypto.sh` (11 kasus); unit `test_builtins` +2.

### Diverifikasi
- Vektor NIST (6, termasuk batas 55/56/64) cocok hashlib;
  keygen/unik/gagal-tertutup; misuse bertipe ditolak;
  full `ctest -j2` hijau; `.ns` + IR OK; `bench.sh` identik 2.26.0.

## [2.26.0] — 2026-09-11 (Phase 75: STDLIB 3.0 — matang)

Konsisten, terdokumentasi, teruji, portabel (master Phase 75).
Murni-Nusa; nol perubahan bahasa/IR/ABI.

### Ditambahkan
- `std/core.ns`: sign/abs_diff/ceil_div/mod_euclid/div_euclid/
  powmod/ilog2/is_pow2/sum_to.
- `std/testing.ns`: asersi berlabel diam-saat-sukses
  (test_true/false/eq_i64/eq_str/eq_bool/fail).
- `std/logging.ns`: log_debug/info/warn/error (prefix level).
- `std/encoding.ns`: hex_encode/decode_i64 + hex_valid/is_hex_digit
  (konvensi parse_int/parse_int_valid).
- string: starts/ends_with, trim trio, ascii lower/upper;
  io: read_line; fs: write_file, read_file_or.
- `tests/` baru (core/encoding/string2/testing_logging) +
  `e2e_stdlib3.sh` (9 kasus); `docs/stdlib-2.26.md` +
  `standard-library.md` v2.26.0.

### Ditunda jujur
- crypto.ns (butuh builtin `ord`/bitwise), koleksi generik
  (butuh param array), async.ns (= tasks.ns yang ada).

### Diverifikasi
- `nusa test tests/` 7/7 + `e2e_stdlib3` 9/9 + full `ctest -j2`
  hijau; `.ns` + IR OK; `bench.sh` identik 2.25.0.

## [2.25.0] — 2026-09-11 (Phase 74: SELF-HOSTING FULL — bootstrap 4-stage)

Rantai bootstrap lengkap + validasi Stage N == Stage N+1
(master Phase 74, "sebisa mungkin"). Nol perubahan bahasa/IR/ABI.

### Ditambahkan
- `nusa bootstrap ... [--rebuild DIR]`: Stage 3 (fresh-rebuild
  Stage 1 di dir lain + 1 run + banding D2 + diagnosis
  beda-pertama); tanpa flag = perilaku 2.24.0 persis.
- `docs/bootstrap-full-2.25.md`: peta 4 stage, tabel Nusa-vs-C++
  per tahap pipeline, reproduksi manual, diagnosis.
- `e2e_bootstrap.sh` 10 kasus (rebuild lex/parse/standalone).

### Diverifikasi
- REBUILD PASS lex (35.511 B) + parse (49.954 B); full `ctest -j2`
  hijau; `.ns` + IR OK; `bench.sh` identik 2.24.0.

## [2.24.0] — 2026-09-11 (Phase 73: BOOTSTRAP 2.0 — rantai multi-komponen)

Frontend Nusa masuk rantai bootstrap (master Phase 73).
Nol perubahan bahasa/IR/ABI (C++ tetap bootstrap layer + oracle).

### Ditambahkan
- `nusa bootstrap [file.ns] [--as lex|ast] [-o DIR]`: kind
  referensi Stage 0 (token vs AST dump), artefak
  `<base>_stage1`; default lama byte-perilaku-sama.
- Rantai parser: `parse_stage1` self-application == Stage 0.
- `docs/bootstrap-2.24.md` + `docs/bootstrap.md` rantai ganda;
  `e2e_bootstrap.sh` 7 kasus (parse-pass/standalone/det + `--as`).

### Diverifikasi
- `e2e_bootstrap` 7/7 + full `ctest -j2` hijau; `.ns` + IR OK;
  `bench.sh` identik 2.23.0.

## [2.23.0] — 2026-09-11 (Phase 72: SELF-HOSTING, milestone 2 — parser+AST)

Parser Nusa memparse Nusa (langkah bootstrap 2).
Nol perubahan bahasa/IR/ABI (murni aditif).

### Ditambahkan
- `selfhost/parse.ns` (fused lexer+parser; struct-threading +
  rekursi; guard anti-hang): dump AST byte-identik `nusa ast`.
- CLI debug `nusa ast <file.ns>` (cermin `nusa lex`): parse
  file tunggal + cetak `program.dump()`; rc!=0 bila gagal.
- `docs/selfhost-2.23.md` (arsitektur + batas jujur) +
  `docs/selfhosting.md` langkah 2; `compiler/tests/e2e_parse.sh`.

### Diverifikasi
- Diferensial 43 file identik (std+examples+bench+selfhost,
  termasuk parse.ns↔dirinya); `e2e_selfhost`/`e2e_bootstrap`
  hijau (lex.ns tak tersentuh); full `ctest -j2` hijau;
  `.ns` + IR OK; `bench.sh` identik 2.22.0.

## [2.22.0] — 2026-09-11 (Phase 71: VERIFICATION, milestone 1 — fondasi)

Invarian pipeline tertulis + dikunci mesin (RFC-0010 Proposed).
Nol perubahan bahasa/IR/ABI (murni aditif).

### Ditambahkan
- `docs/verification-2.22.md`: kontrak I-LEX/I-PARSE/I-FMT/
  I-CHECK/I-MIR/I-IR/I-RUN (totalitas, recovery, idempotence,
  determinisme, preservasi opt, llvm-verify, diferensial O0≡O2).
- Unit `test_verify` (5 properti deterministik, LCG seed-tetap):
  totalitas lexer (256 byte + soup), idempotence fmt, determinisme
  parse/check, preservasi MIR-opt (verify + instr tak-naik).
- `compiler/tests/fuzz_gen.py` (stdlib, `--seed/--count`):
  program bounded-terminating + manifest ok|bad.
- `compiler/tests/e2e_verify.sh`: diferensial korpus O0≡O2,
  run≡build+exec, determinisme check, no-crash malformed,
  fuzz loop 40 program (oracle tanpa-crash + deterministik).

### Diverifikasi
- `test_verify` + `e2e_verify` 52 kasus hijau; full `ctest -j2`
  72/72 hijau; `.ns` + IR OK; `bench.sh` identik 2.21.0.
- Sampingan: ekspektasi `e2e_diagnostics.sh` (kasus stray)
  dimutakhirkan ke pesan top-level ber-`const` (leftover Phase 70);
  `run` di `e2e_verify.sh` retry-1× khusus rc=124 (flaky beban
  paralel, program fuzz bounded-terminating).

## [2.21.0] — 2026-09-11 (Phase 70: COMPILE-TIME, milestone 1 — const)

Item `const` top-level (RFC-0009 Proposed + prototipe slice-1).
Spec 2.0 aditif (item + keyword + semantik const-eval).

### Ditambahkan
- Sintaks `const NAME [: T] = <expr>;` (literal i64/bool/string,
  unary/arith/cmp, ref-const urutan-bebas): eval fixpoint +
  substitusi scope-aware di checker (desugar; codegen nol baris).
  Div-nol/overflow/siklus/non-const/mismatch/assign/duplikat =
  error compile.
- RFC-0009 (Proposed; Security bounds) + `docs/comptime-2.21.md`.
- Unit per tahap (lexer/parser/semantic/typecheck) +
  `compiler/tests/e2e_const.sh` (15 kasus) + seksi `doc`.

### Diverifikasi
- `e2e_const` 15/15 + unit + selfhost/bootstrap + spec/rfc +
  fmt/doc/hardening/tooling/final/concurrency/lsp/ecosystem +
  suite lain hijau; `.ns` + IR OK; `bench.sh` identik 2.20.0.

## [2.20.0] — 2026-09-11 (Phase 69: MACRO SYSTEM, milestone 1 — evaluasi)

Evaluasi kebutuhan + reservasi keyword (mandat kondisional:
belum dibutuhkan — fungsi/generik/trait/builtin cukup).
Spec +1 reserved word (nol pemakaian korpus).

### Ditambahkan
- Keyword `macro` reserved (`TokenKind::Macro`, lexer C++ +
  selfhost, spell; parser menolak alami). Selfhost/bootstrap hijau.
- Unit `testMacroKeyword` + `docs/macro-2.20.md` (evaluasi evidence,
  ambang implementasi, sketsa higiene/tooling).

### Diverifikasi
- Unit + selfhost/bootstrap + frontend/hardening/lsp/tooling +
  suite lain hijau; `.ns` + IR OK; `bench.sh` identik 2.19.0.

## [2.19.0] — 2026-09-11 (Phase 68: MOBILE/EMBEDDED, milestone 1 — minimal)

Biner kecil via link minimal (perilaku program tetap).
Spec tak berubah.

### Ditambahkan
- Runtime dilewati TOTAL bila IR tak merujuk simbol fungsi
  runtime (`@nusa_thread_|mutex_|chan_|atomic_|sleep_|time_|
  random_|process_|tcp_`; presisi `@`, arah aman).
- `-Wl,--gc-sections` selalu + `nusa_rt.o`
  `-ffunction-sections -fdata-sections`.
- `compiler/tests/e2e_min.sh` (3 kasus: batas byte + fungsional)
  + `docs/min-2.19.md`.

### Ukuran terukur (AArch64)
- hello: 22288 → 5816 B (−74%); worker(thread): ~22KB+ → 12448;
  closureapply: 23072 → 6776. Run identik.

### Diverifikasi
- `e2e_min` 3/3 + unit + 29 suite terdampak (link berubah
  untuk semua build) hijau; `.ns` + IR OK; bench waktu identik.

## [2.18.0] — 2026-09-10 (Phase 67: WASM TARGET, milestone 1)

Alias `wasm32` compile-only (tanpa sysroot/WASI/runtime port).
Spec tak berubah.

### Ditambahkan
- Target `wasm32` → `wasm32-unknown-unknown` (alias + triple
  persis; pesan error diperbarui). Link/run mengikuti guard
  existing (sysroot/host).
- Unit `testResolveTarget` + 4 kasus `e2e_cross.sh` (IR triple,
  objek WebAssembly via `file`, tolak link/run) +
  `docs/wasm-2.18.md` + matriks `docs/targets.md`.

### Batasan jujur
- ILP32: asumsi LP64 tak berlaku (handle/closure/thread/net
  tak didukung); terjamin komputasi murni. WASI/sysroot/entry/
  runtime port = follow-up.

### Diverifikasi
- `e2e_cross` 16/16 + unit + suite lain hijau; `.ns` + IR OK.
- `bench.sh` identik baseline 2.17.0.

## [2.17.0] — 2026-09-10 (Phase 66: C INTEROP 2.0, milestone 1 — bindgen)

Generator binding header C subset (tanpa perubahan checker/
codegen; C++ tetap ditolak). Spec tak berubah.

### Ditambahkan
- `nusa bindgen <header.h> [-o out.ns]` (`compiler/bindgen/`,
  `Driver::runBindgen`): integer/`_Bool`/`char*`/`void`/
  typedef-umum/`extern "C"`/`(void)`; skip beralasan + warning
  (float/pointer/string-return/variadik/struct/typedef-kustom/
  inline); rc 1 bila nol; output fmt-clean.
- Unit `test_bindgen` (4 grup) + `e2e_bindgen.sh` (7 kasus:
  golden, warning, fmt, ujung-ke-ujung header→run 42/42,
  3 penolakan) + `docs/ffi-bindgen-2.17.md` + § Bindgen di `ffi.md`.

### Diperbaiki (temuan selama milestone)
- Normalisasi `const`/`volatile` sebelum klasifikasi tipe
  (`const char*` sempat "pointer type"); polos `unsigned` = int.
- Deklarasi ganda + `ec` ganda di header/driver (compile error).

### Diverifikasi
- Unit + `e2e_bindgen` + ffi/final/tooling + suite lain hijau.
- `.ns` + IR OK; `bench.sh` identik baseline 2.16.0.

## [2.16.0] — 2026-09-10 (Phase 65: PROFILER & OBSERVABILITY, milestone 1)

`bench` + build `--stats` (angka resmi tetap `bench/` +
`docs/perf.md`). Spec tak berubah.

### Ditambahkan
- `nusa bench <file.ns> [-n N] [-- args]`: build `--fresh` sekali
  + N run via fork+wait4 (waktu dinding + maxRSS anak; output
  diteruskan; min/median/max + maxrss ke stderr; rc = rc program).
- `nusa build --stats` (+ `pkg build --stats`): rincian ms
  frontend/codegen/clang/total + byte IR/exe (melewati cache agar
  bermakna; hanya build penuh).
- `compiler/tests/e2e_prof.sh` (7 kasus) + `docs/perf-2.16.md`.

### Diperbaiki (selama milestone)
- Duplikat baris help `--link` (warisan Phase 62).
- Include `<unistd.h>` (fork/exec).

### Diverifikasi
- `e2e_prof` 7/7 + unit + final/tooling/buildsys/incr/pkg/
  hello/concurrency/ecosystem/stability + suite lain hijau.
- `.ns` + IR OK; `bench.sh` identik baseline 2.15.0.

## [2.15.0] — 2026-09-10 (Phase 64: DEBUGGER 2.0, milestone 1 — DWARF)

Composite DWARF + formal params (tanpa engine interaktif/DAP:
butuh kontrol-proses, proyek tersendiri). Spec tak berubah.

### Ditambahkan
- `DW_TAG_structure_type` + member + offset (layout LP64,
  nested, cache, guard siklus) + `DW_TAG_formal_parameter`
  (argNo) + `DW_TAG_array_type` + bound dari aloka.
- Unit `testDebugCompositeIR` + 2 kasus `e2e_debug.sh`
  (composite-ir/composite) + `docs/debugging-2.15.md`.

### Diperbaiki (temuan selama milestone)
- `let` struct/array tak pernah mencapai `diDeclare` (dt
  disintesis skalar-saja) — kini named/array dari Binding.

### Diverifikasi
- `e2e_debug` 6/6 + unit + struct/enum/closure/incr/tooling +
  suite lain hijau; `.ns` + IR OK; `bench.sh` identik 2.14.0.

## [2.14.0] — 2026-09-10 (Phase 63: NUSA LSP 2.0, milestone 1)

`textDocument/references` di atas mesin scope-walk yang sama
dengan rename (himpunan identik). Spec tak berubah.

### Ditambahkan
- Server: capability `referencesProvider` + handler
  (`context.includeDeclaration` dihormati, default true;
  posisi kosong → `[]`; dokumen tak dikenal → `[]`).
- Harness `e2e_lsp.py` (4 asersi: 2 lokasi, garis, tanpa
  deklarasi, kosong) + baris `docs/lsp.md`.

### Diverifikasi
- `e2e_lsp` hijau penuh (24 asersi lama + 4 baru) + suite lain;
  `.ns` + IR OK; `bench.sh` identik baseline 2.13.0.

## [2.13.0] — 2026-09-10 (Phase 62: BUILD SYSTEM 2.0, milestone 1)

`clean` + `--profile` (build + pkg build). Spec tak berubah.

### Ditambahkan
- `nusa clean [DIR]`: hapus `*.ns.ll` + `.nusa-cache/` (rekursif),
  `target/` + `dist/` (top-level); idempoten; sumber tak tersentuh.
- `--profile dev|debug|release` (dev=O0, debug=O0+g, release=O2);
  flag eksplisit menang urutan-bebas; asing ditolak. Custom
  `[profile.*]` = follow-up.
- `compiler/tests/e2e_buildsys.sh` (10 kasus) + `docs/build-2.13.md`.

### Diperbaiki (temuan selama milestone)
- Redeclare `ec` di runClean (compile error, langsung).
- Ekspektasi stub init (`Hello, X` tanpa `!`; beda dengan template
  `new` — didokumentasikan apa adanya, bukan diseragamkan).

### Diverifikasi
- `e2e_buildsys` 10/10 + unit + final/tooling/pkg/cross/repro/
  new/registry/sec/eco2 + suite lain hijau; `.ns` + IR OK.
- `bench.sh` identik baseline 2.12.0.

## [2.12.0] — 2026-09-10 (Phase 61: REPRODUCIBLE BUILDS, milestone 1)

Tarball byte-deterministik + pin compiler di lock + jaminan
terdokumentasi. Spec tak berubah.

### Ditambahkan
- Publish deterministik: `tar --sort=name --mtime='@0'
  --owner=0 --group=0 --numeric-owner` + `gzip -n` (dua tahap agar
  error tak tertelan pipe). Republish identik → byte identik.
- `[meta] compiler = "<eksak>"` di `nusa.lock` (ditulis update);
  `lockFresh` menolak pin absen/beda (`pkg update` menyembuhkan).
  Lock pra-2.12 menjadi basi sekali (nol fixture di repo).
- Unit `testLockCompilerPin` + `compiler/tests/e2e_repro.sh`
  (5 kasus: tarball identik, meta, tolak pin, sembuh, build
  `--fresh` ganda identik) + `docs/repro-2.12.md` (jaminan).

### Diverifikasi
- `e2e_repro` 5/5 + unit + pkg/registry/new/sec/eco2/tooling/
  stability/ffi + suite lain hijau; `.ns` + IR OK.
- `bench.sh` identik baseline 2.11.0.

## [2.11.0] — 2026-09-10 (Phase 60: PACKAGE ECOSYSTEM 2.0)

Empat perintah inspeksi read-only (top-level + `pkg`).
Signing ditunda (butuh infra kunci); mirrors/offline/repro =
mekanisme ada + didokumentasikan. Spec tak berubah.

### Ditambahkan
- `tree`: graf dependensi (rekursi path aman-siklus; registry
  `req -> locked`).
- `verify`: manifest + pin lock (path: checksum + isi aman;
  registry: tarball vs index + konten vs pin; SKIP tak-tercache).
- `outdated`: `locked -> latest` per registry-dep (+ beyond-req).
- `audit`: capability + checksum closure + catatan signing.
- Unit tidak baru (perintah diuji e2e); `compiler/tests/e2e_eco2.sh`
  (13 kasus: fixture 2-versi, tree, verify OK/FAIL/sembuh,
  outdated, audit) + `docs/ecosystem-2.11.md`.

### Diverifikasi
- `e2e_eco2` 13/13 + unit + pkg/registry/new/sec + suite lain
  hijau; `.ns` + IR OK; `bench.sh` identik.

## [2.10.0] — 2026-09-10 (Phase 59: SECURITY MODEL, milestone 1)

Audit keamanan + deklarasi capability + 2 enforcement.
`nusa audit/tree/outdated/verify` tetap milik Phase 60.

### Audit (temuan)
- Tanpa eksekusi saat install (tanpa build script/hook — hanya
  tar+clang); deps = sumber. FFI tanpa deklarasi. Builtin
  fs/proses/net tanpa izin (ad-hoc = kepercayaan eksplisit).

### Ditambahkan
- `[capabilities]` manifest (filesystem/network/process/
  environment/native; default false; key asing ditolak;
  round-trip; ringkasan `capabilities: …` saat `pkg build`).
- Enforcement: `pkg build` menolak `extern fn`/`[link]` tanpa
  `native = true` (pindai AST gabungan; ad-hoc `nusa build`
  tak tersentuh). Template/`init` mendeklarasikan eksplisit
  (cli-app: process, server: network).
- Traversal guard install: pre-list tolak absolut/`..` +
  walk tolak subdir/symlink/nama-tak-waras (portabel
  filesystem; SEBELUM checksum).
- Unit (`testCapabilities`, `testPackageDirSafe`) +
  `compiler/tests/e2e_sec.sh` (5 kasus) + `docs/security-2.10.md`.

### Diperbaiki (dampak gate pada test lama)
- `e2e_ffi.sh` manifest ffiapp + `native = true` (gate bekerja).

### Diverifikasi
- `e2e_sec` 5/5 (×2) + unit + pkg/registry/new/ffi/cross +
  suite lain hijau; `.ns` + IR OK; `bench.sh` identik.

## [2.9.0] — 2026-09-10 (Phase 58: NETWORKING, milestone 1 — HTTP)

HTTP/1.x primitives murni Nusa di atas TCP (tanpa framework di
compiler; tanpa builtin/compiler baru). Spec tak berubah (aditif).

### Ditambahkan
- `std/http.ns` (modul std ke-18): `http_status/http_body`
  (parse; -1/`""` malformed), `http_get` (GET/1.0 hingga close),
  `http_serve_once` (200 OK + Content-Length, loopback) +
  helper privat `_dial_retry/_read_all/_read_request`.
- `compiler/tests/e2e_http.sh` (6 kasus): parse unit, 2 round-trip
  hermetik (server Nusa + client Nusa, port 18080/18081), dial
  gagal, fmt+doc.
- `docs/net-2.9.md`: audit net + desain + tunda (UDP, DNS API,
  WebSocket, router) + temuan `import std.string`.

### Diverifikasi
- `e2e_http` 6/6 + unit + stdlib2/async/stress/concurrency/
  memsafe/stability/tooling/ecosystem hijau; `.ns` + IR OK.
- `bench.sh` identik baseline 2.8.0.

## [2.8.0] — 2026-09-10 (Phase 57: ASYNC RUNTIME, milestone 1 — fondasi)

Future/Task/cancellation kooperatif sebagai LIBRARY (tanpa sintaks
`async`/`await`: tetap reserved, RFC-0004 Proposed; sintaks
dilarang sebelum Accepted). Spec tak berubah.

### Ditambahkan
- `std/tasks.ns` (modul std ke-17):
  `struct Future`, `task_spawn/poll/await`, `Cancel` +
  `cancel_new/cancel/is_cancelled/cancel_free` (murni Nusa di atas
  thread/channel/atomic; `///`-documented).
- `compiler/tests/e2e_async.sh` (8 kasus, stabil 3x): await+poll,
  cancel deterministik, shutdown 2-pekerja, konvensi error,
  stress 8 task (280), double-await trap, fmt+doc.
- `docs/async-2.8.md`: posisi desain + aturan milik/cancel/error/
  shutdown + tunda (sintaks, executor, select, timeout).

### Diperbaiki (temuan selama milestone, di luar rencana)
- Formatter mendegradasi `///` → `//` (merusak `nusa doc`;
  lolos karena repo tanpa `///`): pertahankan flag doc lexer +
  unit `testDocCommentsKept`. Nol `///` sebelumnya → nol drift.
- Nama modul `tasks` (bukan `async`): `async` reserved sehingga
  `import std.async` tak terparse (segmen path butuh identifier).

### Diverifikasi
- `e2e_async` 8/8 (×3) + unit (fmt baru) + stdlib2/tooling/
  concurrency/stress/memsafe/stability/ecosystem/new/hello hijau.
- Program `.ns` + IR `@main` OK. `bench.sh`: ukuran konsisten
  warisan registry/chan_len (hello ~22KB); waktu ~4x pada run ini
  = swap-thrash perangkat (492MB tersedia), bukan regresi.

## [2.7.0] — 2026-09-10 (Phase 56: CONCURRENCY 2.0, milestone 1)

Observability channel + stress suite. Aditif, non-breaking
(satu builtin baru; tanpa sintaks; `async` tetap reserved).

### Ditambahkan
- `chan_len(c: i64) -> i64` (panjang antre, non-blocking):
  runtime `nusa_chan_len` + field `size` O(1) di `rt_chan.c`
  (simbol 29 → 30); `frontend::isBuiltin`; checker grup
  arity-1 i64→i64; codegen grup i64-return. Race-tolerant
  by-design (cermin `mutex_try_lock`).
- `compiler/tests/e2e_stress.sh` (7 kasus, stabil 3x): `chan_len`
  dasar, polling produsen, fan-out 4×25, atomic hammer 8×2000,
  mutex contention 4×500, misuse `chan_len`-after-free, audit
  `async`-reserved.
- `docs/concurrency-2.7.md`: audit primitif + desain + tunda
  (select/close, executor, koleksi std) + audit async/await.

### Diperbaiki (dokumen, ditemukan saat milestone)
- `docs/concurrency.md`: `join(0) → 0` usang → fail-fast v2.5.0;
  + `chan_len`. `docs/{compiler,standard-library,getting-started}.md`:
  54 → 55 builtin.

### Diverifikasi
- `e2e_stress` 7/7 (×3) + unit + concurrency/memsafe/runtime/
  stdlib2/tooling/stability hijau; `.ns` + IR `@main` OK.
- `bench.sh` identik baseline 2.6.0 (pass belum rewire).

## [2.6.0] — 2026-09-10 (Phase 55: RUNTIME 2.0, milestone 1 — modular)

Split `runtime/nusa_rt.c` tanpa perubahan perilaku. Spec tak berubah.

### Diubah (pindah verbatim, nol logika)
- `runtime/`: `rt_handle.c` (registry), `rt_thread.c`, `rt_sync.c`
  (mutex+atomic), `rt_chan.c`, `rt_time.c`, `rt_net.c` +
  `rt_platform.h` (deteksi + kontrak: kode platform-spesifik hanya
  di balik header ini). `nusa_rt.c` = agregator satu TU (fallback
  driver + .deb tak tersentuh; driver tanpa perubahan).
- `CMakeLists.txt`: `DEPENDS` runtime diperluas (glob rt_*),
  versi 2.6.0, test `e2e_runtime` (+ `RUN_SERIAL`: uji fallback
  memindahkan `nusa_rt.o` sementara — balapan dengan test paralel,
  ditemukan saat verifikasi).
- `packaging/deb.sh`: kirim direktori `runtime/` (agregator +
  bagian); rilis `website/downloads/nusa_2.6.0-1_aarch64.deb` +
  regenerasi halaman via `website/gen.py`.

### Ditambahkan
- `docs/runtime-2.6.md`: audit batasan + peta subsistem + kontrak.
- `compiler/tests/e2e_runtime.sh` (4 kasus: simbol 29/29 identik,
  higiene platform, fallback tanpa `.o`, regresi concurrency).

### Diverifikasi
- Simbol publik identik 29/29; fallback build+run OK.
- `e2e_runtime`/`e2e_memsafe`/`e2e_deb`/`e2e_ecosystem` +
  unit + suite terdampak hijau; `.ns` + IR `@main` OK.
- `bench.sh` identik (closureapply 22856 B warisan Phase 54).

## [2.5.0] — 2026-09-10 (Phase 54: ADVANCED MEMORY SAFETY, milestone 1)

Audit model memori + interno runtime. Tanpa sintaks baru
(tanpa `unsafe{}` — language-level, butuh RFC bila kelak ada).

### Diperbaiki (terbukti ASan sebelum fix)
- UAF/double-free handle concurrency di `runtime/nusa_rt.c`:
  double-`join`, `send`-after-`free` (ASan: `nusa_thread_join:57`,
  `nusa_chan_check:142`), pola sama mutex/atomic.
- Mitigasi: registry handle hidup (mutex-global + list) —
  `new/spawn` mendaftar, `join/free` mengeluarkan; pakai/`free`
  ganda/basi/nol (sekuensial) → `runtime error: invalid handle
  in <op>` + exit 1 (konsisten OOB/div-0). ASan bersih pasca-fix.
- Perubahan perilaku (sengaja, fail-fast): `thread_join(0)` dan
  `mutex/atomic(0)` yang dulu diam kini error keras (`chan(0)`
  sudah error). Tak ada test yang bergantung perilaku diam.

### Ditambahkan
- `docs/mem-2.5.md`: hasil audit + batas unsafe eksplisit
  (builtin/FFI/murni) + residual jujur (ABA reuse,
  free-while-borrowed konkuren → milestone berikut).
- `compiler/tests/e2e_memsafe.sh` (6 kasus: 5 misuse + 1 valid).
- Adendum §12 di `docs/memory-model.md` (aturan lifecycle handle).

### Biaya terukur
- `closureapply` exe 20800 → 22856 B (+~10%: registry + pesan);
  `bench.sh` lain identik. Correctness > perf (kuantifikasi
  penuh di Phase 86).

### Diverifikasi
- `e2e_memsafe` 6/6 + unit + arch/mir/stability/concurrency/
  stdlib2/tooling/new hijau; `.ns` + IR `@main` OK.

## [2.4.0] — 2026-09-10 (Phase 53: OPTIMIZATION FRAMEWORK, milestone 1)

Pass internal pertama di atas MIR (LLVM tetap optimizer utama).
Pipeline `.ns` belum di-rewire (nol perilaku). Semantik tak berubah
(spec 2.0 normatif; tanpa RFC).

### Ditambahkan
- `compiler/mir/Opt.{h,cpp}` (`nusa::mir`): `foldConstants`
  (arith/cmp beroperand-konstanta; Div aman: pembagi nol dan
  `INT64_MIN/-1` tidak difold), `eliminateDeadCode` (murni +
  tak-dipakai; call/alloc/load/store/print/terminator dipertahankan),
  `optimizeFunction`/`optimizeModule` (fixpoint berbatas). Murni C++.
- Unit: 5 grup baru di `test_mir.cpp` (fold arith/cmp/div-safety/
  DCE/pipeline) + `bench/mir_opt_main.cpp` (workload sintetik) +
  `compiler/tests/e2e_opt.sh` (isolasi lapisan + bench 3x + regresi).
- `docs/opt-2.4.md`: desain framework + aturan keamanan + hasil.

### Diperbaiki (temuan selama milestone)
- DCE `erase` satu-per-satu = O(n²) (~2s/6002 instruksi) → filter
  partisi (~20ms, ~100x). Satu ekspektasi unit salah hitung (4→3).

### Benchmark (Termux AArch64, 3 run)
- MIR opt (n=2000): 6002 → 2 instruksi (folded=4000,
  eliminated=6000), verify pra/pasca ok, 19.9/21.9/23.4ms.
- `bench/bench.sh` `.ns` identik baseline 2.3.0 (pass belum rewire).

### Diverifikasi
- Unit 11/11 (ASan+UBSan bersih) + `e2e_opt` 4/4 + `e2e_mir` +
  `e2e_arch` + `e2e_stability` + suite terdampak 8/8 hijau.
- Program `.ns` + IR `@main` OK.

## [2.3.0] — 2026-09-10 (Phase 52: NUSA IR, milestone 1 — MIR fondasi)

Pipeline belum di-rewire (codegen tetap AST → LLVM; nol perilaku).
Semantik bahasa tidak berubah (spec 2.0 normatif; tanpa RFC).

### Ditambahkan
- `compiler/mir/Mir.{h,cpp}` (`nusa::mir`): typed values
  (I64/Bool/Str/Void/Ptr), instruksi (arith/cmp/call/alloc/load/
  store/branch/condbr/return/unreachable/print), basic blocks,
  `MirBuilder`, `verify()` (terminator, def-use, target, arity/tipe
  call, return), `dump()` deterministik. Murni C++ standar —
  tanpa include compiler/* lain maupun LLVM.
- Unit `compiler/tests/test_mir.cpp` (5 grup: build, golden dump,
  control-flow, 6 penolakan, `testFreshBuilder`) +
  `compiler/tests/e2e_mir.sh` (isolasi lapisan + regresi `.ns`).
- `docs/ir-2.3.md`: desain IR + keputusan implementasi.

### Diperbaiki (temuan selama milestone)
- OOB heap di counter id builder (vector paralel out-of-sync saat
  builder baru di atas modul tak-kosong — ditemukan via crash
  deterministik + ASan): counter pindah ke `MirFunction.nextValueId`
  + lazy-init; referensi stabil via `deque`. ASan+UBSan bersih.

### Diverifikasi
- Unit 11/11 + `e2e_mir` 2/2 + `e2e_arch` + `e2e_stability` +
  suite terdampak 8/8 hijau.
- Program `.ns` + IR `@main` + `bench/bench.sh` identik baseline 2.2.0.

## [2.2.0] — 2026-09-10 (Phase 51: COMPILER ARCHITECTURE 2.0, milestone 1)

Tanpa rewrite total. Satu milestone bertahap: putus kopling
Backend → Semantic Layer. Semantik bahasa tidak berubah
(spec 2.0 tetap normatif; tanpa RFC — bukan language-level).

### Diubah (refactoring murni, nol perilaku)
- Baru `compiler/frontend/Builtin.{h,cpp}`: `nusa::isBuiltin()`
  dipindah verbatim dari `Semantic.cpp` (lapisan Frontend sebagai
  rumah pengetahuan builtin bersama).
- `semantic/Semantic.h`: hapus deklarasi `isBuiltin`;
  `Semantic.cpp`/`TypeChecker.cpp` include frontend baru.
- `codegen/Codegen.cpp`: `semantic/Semantic.h` →
  `frontend/Builtin.h`. Backend kini nol-include
  `semantic/Semantic.h`/`TypeChecker.h` (dikunci mesin).
- `CMakeLists.txt`: tambah `compiler/frontend/Builtin.cpp`;
  versi 2.2.0; registrasi test `e2e_arch`.

### Ditambahkan
- `docs/architecture-2.2.md`: proposal + peta lapisan terukur
  (~26000 baris), 4 temuan kopling (1 diperbaiki, 3 ditunda
  beralasan: Pkg→Driver butuh interface; `sha256Hex` menunggu modul
  support; LSP→semantic diterima untuk v1; `Scopes.h` kandidat
  pindah ke support/).
- `compiler/tests/e2e_arch.sh` (4 kasus): backend-bersih,
  definisi-tunggal di frontend/, Semantic.h-bersih, perilaku
  builtin (`len`/`assert`/`str_len` run OK).

### Diverifikasi
- Unit 10/10 + `e2e_arch` 4/4 + `e2e_stability` 22/22 + suite
  terdampak (stdlib/closures/concurrency/hardening/tooling/match/
  error/stdlib2) 8/8 hijau.
- Program `.ns` (hello/concurrency) run OK; IR `@main`
  terverifikasi; `bench/bench.sh` identik baseline 2.1.0.

## [2.1.0] — 2026-09-10 (Phase 50: NUSA 2.1 STABILITY)

Fokus: stability, correctness, compatibility. Tanpa perubahan
semantik bahasa (spec 2.0 tetap normatif). Baseline 51/51 ctest
2.0.0 terverifikasi hijau sebelum perubahan; benchmark
`bench/bench.sh` identik (hello ~1.4 dtk/20032 B, perf2000 ~7.3 dtk).

### Diperbaiki
- Formatter lambda sesudah `(`/`[` menempel tanpa spasi
  (`apply(|x| …)`, `thread_spawn(|| …)`; dulu `apply( |x| …)` —
  bug `isOpener` di `compiler/fmt/Formatter.cpp`). Kanonisasi ulang:
  `examples/closures.ns`, `examples/concurrency.ns`,
  `tutorials/07-closures-concurrency/main.ns`, `std/random.ns`,
  `std/process.ns`, `std/thread.ns`, `bench/programs/closureapply.ns`,
  `bench/programs/matchdispatch.ns` (kini `fmt --check` bersih 51/51).
- Build env: symlink `/usr/bin/ar|ranlib` → Termux `llvm-ar`
  (cache CMake lama merujuk `/usr/bin/ar` yang hilang; bukan bug Nusa).

### Ditambahkan
- Unit `testLambdaAfterParen` (`compiler/tests/test_fmt.cpp`):
  lambda sesudah `(` + `||` sesudah `(`.
- `compiler/tests/e2e_stability.sh` (22 kasus, terdaftar `e2e_stability`
  di CMake): 9 area audit Phase 50 — crash/malformed, parser,
  typechecker, codegen+IR, runtime OOB/string, memory copy/loop,
  pkg manifest-invalid, LSP smoke, formatter idempoten+regresi.

### Diverifikasi
- Unit 10/10 + `e2e_stability` 22/22 + subset regresi
  (closures/concurrency/tooling/stdlib2/ecosystem/new) hijau.
- Program `.ns` nyata (hello/closures/concurrency/tutorial-07) run OK;
  LLVM IR `@main` terverifikasi; `bench/bench.sh` stabil.
- Fuzz manual 16 pola malformed: semua exit != 0, rc ≤ 128 (tanpa sinyal).

### Batasan 2.1.0 (kontrak, bukan utang baru)
- Spec bahasa = 2.0 (tidak diubah); tanpa GC/FFI-callback/async
  (tetap RFC-0007/0008/0004); channel tanpa select/close; registry lokal.
- Env membutuhkan `ar`/`ranlib` di PATH (diatasi via symlink di CI ini).

## [Paket deb] — 2026-09-09 (install global Termux + halaman unduhan)

`packaging/deb.sh` membangun `nusa_<ver>-1_aarch64.deb` (layout
`$PREFIX`: `bin/nusa` + `nusa_rt.o`, `runtime/`, `std/`,
`share/doc`; `Depends: clang`) dan halaman `website/downloads.html`
memilih versi stabil via dropdown tanpa-JS (daftar dari berkas
yang ada + sha256, riwayat menaut catatan rilis).

### Ditambahkan
- `packaging/deb.sh` (`--version/--build-dir/--output/--prefix`);
  berkas rilis `website/downloads/nusa_2.0.0-1_aarch64.deb`.
- `e2e_deb.sh` (5 kasus: build/meta/layout/run-terisolasi/halaman);
  CSS `details.pick`; tautan Unduh di navigasi.

### Batasan
- aarch64 saja (paket menolak asumsi lain via peringatan arsitektur);
  instalasi butuh `apt` (resolusi `clang`); hosting rilis = file
  di repo (tanpa registry biner eksternal).

## [Website] — 2026-09-09 (halaman per-dokumen + redesain visual)

Setiap dokumen kini halaman HTML sendiri (53 halaman via
`website/gen.py`, tetap offline-first tanpa dependensi) +
redesain (terminal fosfor, ledger, responsif). Beranda
`index.html` mirror `README.md` root (judul, penulis, quote 2.0,
status, jalur belajar, prasyarat/build/pakai, struktur, batasan);
`CHANGELOG.md` ikut generate (`docs/changelog.html`). Tautan
`.md` mentah dihapus dari situs; uji tautan e2e diperkuat
(resolusi per-file, mencakup subdirektori).

## [Docs sync] — 2026-09-09 (sinkronisasi docs/ ke 2.0.0, tanpa perubahan compiler)

Audit 33 dokumen `docs/` (referensi silang + implementasi): klaim usang
diperbarui, kontradiksi diperbaiki. Versi tetap 2.0.0.

### Diperbaiki
- `types.md`, `functions.md`, `structs.md`, `syntax.md`: "tanpa
  generics/first-class-fn/struct-param/package-manager" → status 2.0
  (monomorphisasi, closure values, struct by-value, `nusa.toml`);
  catatan bare-`return;` 2.0.
- `spec.md`: banner riwayat + penunjuk normatif spec-2.0; grammar
  dilengkapi `EnumDecl`/`MatchStmt`/`ExternDecl`; Option/Result struct
  → enum 2.0; batas v1.0.0 dibingkai historis.
- `standard-library.md`, `compiler.md`: penanda split Phase 40 →
  normatif spec-2.0; stdlib: GC-roadmap → RFC-0007; compiler: keyword/
  operator/CLI/builtins aktual (54 builtin terhitung dari sumber),
  `TypeKind::Param`, hitungan test lama ditandai foto era.
- `enums.md`: Option/Result struct → enum natif 2.0 (B1).
- `pkg.md`: deps versi-registry + caret (kontradiksi Phase 30
  diperbaiki); `update`/`publish` sesuai implementasi.
- `getting-started.md`: v2.0.0 (50 test), stdlib 16 modul, contoh
  diagnostics bahasa Inggris sesuai output aktual.
- `language-spec-2.0.md` + `language-spec-1.0.md`: `ptr` dikeluarkan
  dari grammar `Type` + prosa §5 (implementasi tak punya tipe `ptr`
  yang dapat ditulis — handle terekspos sebagai `i64`/`string`).
- `ecosystem.md`: intro tanpa klaim BELUM (12/12 ✅).

## [Ecosystem] — 2026-09-09 (Phase 43 lanjutan: website + tutorials + examples 2.0)

Melengkapi tiga komponen §28 master yang tertinggal: website statis,
tutorial dedikasi, dan contoh runnable era 2.0. Tanpa perubahan
compiler (versi tetap 2.0.0).

### Ditambahkan
- `website/`: `index.html` (beranda + quickstart + highlight 2.0),
  `ecosystem.html` (cermin `docs/ecosystem.md`), `tutorials.html`
  (index jalur 01–08), `examples.html` (index contoh runnable),
  `style.css` (satu stylesheet, system fonts), `README.md` (preview
  `python3 -m http.server` + opsi publish statis). Offline-first:
  tanpa JS eksternal/CDN/tracking/build step; tautan relatif
  `../docs|tutorials|examples|rfcs/`.
- `tutorials/`: jalur 01–08 (`README.md` + `main.ns` runnable per
  modul): hello, variables, functions/struct, generics+traits,
  match+enum, error handling (Option/Result enum + `?`),
  closures+concurrency, package project (`pkg`/`new`/`test`).
- `examples/`: 7 contoh 2.0 runnable (`generics`, `traits`, `match`,
  `enums`, `closures`, `error_option_result`, `concurrency` —
  thread + channel i64); basis v1.0 utuh.
- `e2e_ecosystem.sh` (8 kasus: website offline+link, tutorials
  run+fmt, examples run+fmt, template list, registry docs,
  RFC index, playground docs, ecosystem table) + `docs/ecosystem.md`
  (§Website/§Tutorials/§Examples 2.0; tabel status kini 12/12 ✅);
  README struktur + `.gitignore` (`.nusa-cache/`).

### Batasan
- Website = statis murni (tanpa compiler-service online; eksekusi
  tetap lokal via `nusa run` / `nusa playground` loopback).
- Tutorial 08 mempraktikkan `pkg`/`new` di `/tmp` agar repo bersih.

## [2.0.0] — 2026-09-08 (Phase 45: Nusa 2.0)

Rilis mayor: sum-type natif (B1, BREAKING), channel bertag +
TCP server (ekspansif-terkirim), satu bug diperbaiki, spec 2.0 +
panduan migrasi. GC/FFI-callback/async SENGAJA di luar 2.0
(RFC-0007/0008/0004 Proposed dengan desain + acceptance criteria).

### Breaking (migrasi: docs/migration-2.0.md)
- `Option`/`Result`: encoding struct + dummy DIHAPUS →
  `enum Option<T> { Some(T), None }` / `enum Result<T, E> { Ok(T),
  Err(E) }` di std. `none(d)`/`ok(v,d)`/`err(d,e)` → ctor
  qualified (`Option<i64>.None()`); `opt_map`/`res_map`/
  `res_map_err` tanpa dummy; field `.value`/`.has_value`/`.is_ok`/
  `.error` ditolak (gunakan match/kombinator); struct
  `Option`/`Result` pengguna + pola variannya ditolak dengan hint.
- Keyword `async`/`await` di-reserved (nol pemakaian di korpus).

### Ditambahkan
- `?` atas enum `Result` (uji tag + unwrap slot; struct-mirip
  tetap didukung); exhaustiveness varian untuk Option/Result natif.
- Channel bertag (RFC-0006): `chan_new_bool/send_bool/recv_bool`,
  `chan_new_str/send_str/recv_str` (tag runtime, mismatch = exit 1).
- TCP server loopback: `tcp_listen`/`tcp_accept` + `std.net`
  `listen_on`/`accept_one`; template `server` kini serve-once nyata.
- Bare `return;` di tengah `main` legal (bug IR diperbaiki).
- Substitusi tparam ke posisi-tipe ekspresi body generik
  (`Option<U>.Some(…)`; dulu `unknown type`) + recheck
  StructLit rewrite mengembalikan tipe applied (dulu
  `named(mangled)`; latent sejak Phase 26).
- `docs/language-spec-2.0.md` (normatif; 1.0 disimpan),
  `docs/migration-2.0.md`; RFC-0005 Accepted; RFC-0006 Implemented;
  RFC-0007/0008 Proposed.
- Uji: unit typecheck (migrasi + `testMatchOptionStructRejects`),
  builtins (tagged channels, listen/accept), lexer (async);
  `e2e_spec2.sh` (6: B1 dua arah); e2e_error/match/closures/
  concurrency/stdlib2/new dimigrasi/ditambah.
- Audit pasca-rilis: manifest scaffold invalid (`compiler =`
  kehilangan kutip — off-by-one replace, kini diuji via
  `pkg build` di e2e_new) + 5 file template tak-lolos fmt
  (kini nol-baris-kosong + dikunci e2e_new).

### Batasan 2.0 (kontrak, bukan utang)
- Tanpa GC (heap program-lifetime dipertahankan); tanpa FFI
  struct/callback; tanpa async; channel tanpa select/close;
  registry lokal; playground sekuensial; DWARF skalar; LSP satu-file.

## [2.0 design] — 2026-09-08 (Phase 44: NUSA 2.0 design)

Review desain pra-2.0 TANPA code (mandat master): evaluasi 15 area
di `docs/nusa-2.0-design.md` + RFC payung RFC-0005 (Proposed).
Temuan utama: satu bug terkonfirmasi (bare-return di main, dengan
repro), dua kandidat breaking (sum-type natif, keyword async),
satu tambahan stdlib (`tcp_listen`/`accept`), selebihnya DINYATAKAN
kontrak. Tanpa perubahan versi (tetap 1.20.0).

## [Templates] — 2026-09-08 (Phase 43: ecosystem)

`nusa new` + 4 template tertanam (hello-world/cli-app/library/
server) yang semuanya build+run/test hijau, dan `docs/ecosystem.md`
(inventaris komponen §28 master + status website). Tanpa perubahan
versi (tetap 1.20.0).

### Ditambahkan
- `compiler/pkg/Templates.cpp` (`pkg::runNew`, perintah top-level
  `nusa new <template> [name] [--dir]` + `--list`): tolak template
  asing/nama invalid/dir tak-kosong; `compiler =` = versi pen-scaffold.
- `e2e_new.sh` (6 kasus: list/4 template/negatif); `docs/ecosystem.md`.

### Batasan Phase 43
- Template server = serve-once nyata (sejak 2.0); website
  dievaluasi-ditunda (alasan tercatat, bukan penolakan permanen).

## [Playground] — 2026-09-08 (Phase 42: nusa playground)

Menjalankan program tak-terpercaya secara aman: `nusa sandbox`
(verdict mesin) + `nusa playground` (HTTP loopback → sandbox)
sesuai arsitektur master (Browser → Playground → Compiler Service
→ Sandbox → Program).

### Ditambahkan
- `compiler/driver/Sandbox.{h,cpp}`: supervisor/worker/runner
  (fork, grup proses, tmpdir, env scrub, /dev/null stdin, output
  cap, `RLIMIT_CPU`, timeout total run+build, verdict
  ok/timeout/mem-limit/output-limit/signal/build-fail).
- `compiler/driver/Playground.cpp` (`Driver::runSandbox/runPlayground`):
  server 127.0.0.1-only (`GET /`, `POST /api/run` JSON, `--port 0`
  ephemeral, cap header/body, halaman embedded).
- `e2e_playground.sh` (9 kasus: 5 verdict + page/run/timeout/
  bad-request+404); `docs/playground.md`.

### Batasan Phase 42 (keamanan jujur)
- Memori via polling VmRSS (RLIMIT_AS merusak startup Bionic —
  diverifikasi); tanpa namespace (NPROC sengaja tak dipakai);
  jaringan tak diblokir; server sekuensial loopback-only tanpa auth;
  `import` lokal tak tersedia di playground. Tanpa perubahan versi
  (tetap 1.20.0).

## [RFC system] — 2026-09-08 (Phase 41: RFC process)

Perubahan bahasa besar kini wajib melalui RFC
(`rfcs/README.md`): Proposed → Design → Prototype →
Implementation → Tests → Specification. Template + uji
konsistensi mesin (`e2e_rfc.sh`).

### Ditambahkan
- `rfcs/`: README proses (alur/status/aturan penomoran/indeks),
  TEMPLATE (8 bagian wajib), RFC-0001 generics / 0002 traits /
  0003 ownership (Implemented, retroaktif) / 0004 async (Proposed,
  dengan pertanyaan terbuka; implementasi dilarang sebelum Accepted).

### Batasan Phase 41
- Tanpa perubahan compiler (versi tetap 1.20.0); RFC historis
  mencatat rationale, bukan mengubah keputusan lama.

## [Spec 1.0] — 2026-09-08 (Phase 40: formal language specification)

Spesifikasi bahasa resmi pertama (`docs/language-spec-1.0.md`,
normatif atas compiler 1.20.0): lexical grammar, syntax EBNF, AST
semantics, type system, memory model, evaluation rules, modules,
generics, traits, pattern matching, concurrency, FFI, stdlib —
dengan pemisahan Language Spec vs Compiler Implementation
(`docs/compiler.md`) vs Stdlib Spec (`docs/standard-library.md`)
dan Appendix undefined behavior.

### Ditambahkan
- `docs/language-spec-1.0.md` (15 bagian + appendix; tiap topik
  menunjuk file uji pengunci via `Tests:`).
- `e2e_spec.sh` (5 kasus: penanda/13-topik/split/traceability/
  probe eksekusi); penanda split satu-baris di `compiler.md` dan
  `standard-library.md`.

### Batasan Phase 40
- Tanpa perubahan compiler (versi tetap 1.20.0); spec mendeskripsikan
  perilaku teruji — di luar itu = undefined (Appendix A).

## [1.20.0] — 2026-09-08 (Phase 39: bootstrap compiler)

Rantai bootstrap terdokumentasi + terverifikasi mesin: Stage 0 (C++)
mengkompilasi lexer Nusa menjadi biner native (Stage 1) yang
output-nya byte-identik dengan lexer C++ (Stage 2 = Stage 0) dan
deterministik — via perintah baru `nusa bootstrap`.

### Ditambahkan
- `Driver::runLex` (dump `nusa lex` pindah dari main → dipakai ulang
  sebagai referensi Stage 0) + `Driver::runBootstrap` (build --fresh,
  self-lexing, perbandingan byte, cek determinisme 2x, diagnosis byte
  beda pertama).
- `nusa bootstrap [lex.ns] [-o DIR]` (default input:
  `<exe>/../selfhost/lex.ns`, lalu `selfhost/lex.ns`).
- `e2e_bootstrap.sh` (4 kasus: rantai/standalone/determinisme/
  negatif); `docs/bootstrap.md` (kontrak komponen + reproduksi manual).

### Batasan Phase 39
- Rantai mencakup lexer saja (parser/AST/semantik masih C++);
  komponen berikutnya mengikuti kontrak yang sama.

## [1.19.0] — 2026-09-07 (Phase 38: self-hosting)

Bootstrap langkah 1: lexer Nusa (`selfhost/lex.ns`) dibuktikan
ekuivalen dengan lexer C++ via differential test 33 file + perintah
debug `nusa lex` dan builtin `int_to_str` pendukungnya.

### Ditambahkan
- `nusa lex FILE`: dump `kind|lexeme|line|col` (+ escape terdokumen).
- `selfhost/lex.ns`: lexer streaming murni-Nusa (keyword/angka/
  string+escape/operator-2-char/komentar; pola loop end/lim).
- `str_index_of/str_contains` (std/string.ns, murni Nusa);
  builtin `int_to_str(i64)` (snprintf+malloc).
- `e2e_selfhost.sh` (5 kasus: check/diferensial-33/CLI/format/
  hazard); `docs/selfhosting.md` (kontrak escape + batas).

### Batasan Phase 38
- Divergensi hanya di luar korpus valid (\0, overflow, kanal error);
  performa ~O(n²) (tercatat); parser/AST/semantik masih C++.

## [1.18.0] — 2026-09-07 (Phase 37: debugger support)

DWARF via LLVM DIBuilder di balik `-g`: compile-unit, subprogram,
line table per statement, dan variabel lokal skalar — terverifikasi
`llvm-dwarfdump` (lldb/gdb tak tersedia di CI ini).

### Ditambahkan
- `Codegen::setDebug` + `diBeginFunction/diSetLoc/diDeclare/diType/
  diFinalize` (CU malas-per-file, SP, lokasi, declare i64/bool/
  string; agregat dilewati); flag versi modul eksplisit.
- `nusa build -g/--debug` (+ `pkg build -g`); clang `-g`; `-g`
  masuk fingerprint incremental.
- Unit codegen 1 (metadata IR + default bersih); `e2e_debug.sh`
  (4 kasus: IR/dwarfdump/tanpa-g/-gO2); `docs/debugging.md`.

### Batasan Phase 37
- Skalar saja; satu scope per fungsi; tanpa pretty-printer;
  tanpa debugger interaktif di CI (dwarfdump sebagai bukti).

## [1.17.0] — 2026-09-07 (Phase 36: language server)

Server LSP via stdio (`nusa lsp`): diagnostics, completion, hover,
goto-definition, symbols, formatting, rename — di atas pipeline
yang sudah ada (lexer/parser-recovery/semantic/fmt).

### Ditambahkan
- `compiler/lsp/`: JSON minimal (tanpa exceptions; `\u` + surrogate),
  transport Content-Length, doc store, scope-walk use→def
  (let/param/bind-pola/lambda/match-arm + global; method/field
  dilewati), 7 fitur + initialize/shutdown/exit.
- `nusa lsp` + baris help; `docs/lsp.md`.
- `e2e_lsp.py` (24 asersi via harness python: semua fitur +
  bad-method + close + exit-code).

### Batasan Phase 36
- Satu file (tanpa resolve impor); tanpa typechecker (hover local
  tanpa tipe); tanpa goto-tipe/references/codeAction; byte-based.

## [1.16.0] — 2026-09-07 (Phase 35: incremental compilation)

Build hangat 7x lebih cepat: fingerprint sumber+flag dan cache
artefak berbasis hash IR — clang dilewati bila IR identik.

### Ditambahkan
- `compiler/incr/`: dependency graph, fingerprint
  (`<outdir>/.nusa-cache/fingerprint`: compiler/target/opt/emit/
  sysroot/links/modul/IR), perbandingan flag-vs-sumber terpisah.
- `nusa build --fresh` (+ `pkg build --fresh`); pesan
  `Fresh <out> (cached)` vs `rebuilding: ~a.ns, +b.ns` /
  `rebuilding: flags changed`; output hilang → rebuild.
- `checkStages` mengeluarkan modul (opsional, non-breaking).
- Unit `test_incr` (4); `e2e_incr.sh` (6 kasus: hangat/komentar/
  logika/flag/fresh/relink/dep); `docs/incremental.md`.

### Batasan Phase 35
- Frontend selalu jalan (disengaja — murah + sound); tanpa cache
  AST/semantik/objek-per-modul (beralasan di dokumen); tanpa
  daemon/watch-mode.

## [1.15.0] — 2026-09-07 (Phase 34: compiler optimization)

Optimasi berbasis benchmark: runtime precompiled (−36% waktu build),
flag `-O`, harness bench + `docs/perf.md`. Tanpa micro-opt frontend
(bukti: clang >85% build).

### Ditambahkan
- Target CMake `nusa_runtime` (`build/nusa_rt.o`); driver memakai
  `<exe>/nusa_rt.o` bila ada (fallback `.c`); `-Wno-override-module`.
- `nusa build -O0/1/2/3/s/z` (+ terus ke `pkg build`); default O0
  tak berubah (O2: −9% ukuran, ~0–10% runtime — opt-in).
- `bench/programs/` (fib/loopsum/strcat/matchdispatch/closureapply)
  + runtime di `bench.sh`; `docs/perf.md` (anatomi + keputusan).
- e2e_tooling: kasus `-O2` fungsional + `-O9` ditolak.

### Batasan Phase 34
- Tanpa incremental (Phase 35); tanpa PGO/LTO; angka bench acuan
  perangkat-spesifik (ulangi ≥3x selang-seling).

## [1.14.0] — 2026-09-07 (Phase 33: cross compilation)

Target Linux AArch64/x86-64 via clang `--target` (compile-only tanpa
sysroot; link butuh `--sysroot`), mode emisi IR/asm/objek, dan
matriks target terdokumentasi. Host Android tak tersentuh.

### Ditambahkan
- `Codegen::setTargetTriple` (default kosong = perilaku lama);
  `Driver::hostTriple/resolveTarget` (alias + triple persis +
  daftardukung, host = triple clang ini).
- `nusa build --target/--sysroot/--emit-ir/--emit-asm/-c`;
  pre-emptive error link-cross-tanpa-sysroot; `run --target`
  (host OK, cross ditolak); manifest `target` + flag pkg build.
- Unit codegen 2 (triple di IR, resolver); `e2e_cross.sh`
  (12 kasus incl. ELF x86-64/aarch64 via `file`); `docs/targets.md`.

### Batasan Phase 33
- Link cross butuh sysroot pengguna; tanpa Windows/macOS
  (dievaluasi, alasan tercatat); asumsi LP64 di semua target.

## [1.13.0] — 2026-09-07 (Phase 32: C interoperability)

FFI terbatas: deklarasi `extern fn`, ABI C untuk primitif
(bool = int), dan integrasi linker (`--link`, manifest `[link]`).

### Ditambahkan
- Lexer `extern`; AST `FunctionDecl.isExtern`; parser
  `extern fn ...;` (tanpa body; tanpa cek main ganda).
- Semantic: namespace fungsi bersama, tolak generik/main;
  checker `checkExternSig` (param i64/string/bool; return
  i64/bool/void + alasan per-jenis); tolak extern-as-value.
- Codegen: deklarasi saja (tanpa body), `ffiType` (bool→i32),
  zext argumen + icmp return, `externFns_` set.
- Linker: `LinkSpec` (libs/dirs/files) → clang `-l/-L`;
  `nusa build --link/--link-dir` (repeatable); manifest `[link]`.
- `nusa doc` § Extern Functions; `docs/ffi.md` (+ tabel mapping,
  model keamanan, batasan); sentuh syntax/spec.
- Unit: lexer 1, parser 2, semantic 2, typecheck 4, doc 1;
  `e2e_ffi.sh` (10 kasus: libc/bool-mapping/.o/-L-l/[link]/
  5 penolakan/simbol-hilang).

### Batasan Phase 32
- Tanpa pointer umum/struct-by-value/callback/variadik/
  string-return/dlsym; pustaka dipercaya penuh (pin sumbernya).

## [1.12.0] — 2026-09-07 (Phase 31: package registry)

Registry file-backed (tanpa server): index + tarball per direktori,
`nusa search/install/publish`, resolver versi (persis/caret) dengan
cache terverifikasi, keamanan dua-lapis checksum.

### Ditambahkan
- Layout registry (`index/<name>.toml` + `packages/*.tar.gz`);
  sumber `--registry`/`$NUSA_REGISTRY` (fallback berurutan).
- `pkg publish --registry` (tolak path-deps; tulis-sekali per versi;
  idempoten bila identik; segarkan hash transport), `pkg search`,
  `pkg install name[@req]` (add caret + update), top-level
  `nusa search/install/publish`.
- Resolver versi: seleksi maksimum-memenuhi, pin lock
  (nama/versi/registry/checksum), cache `$HOME/.cache` + verifikasi
  transport & konten, sembuh-otomatis, build hermetik bila hangat.
- `satisfiesReq` (caret 0.x benar); lock `registry` opsional;
  manifest `description` (searchable, round-trip).
- Unit `testSatisfiesReq`; `e2e_registry.sh` (19 kasus: publish/
  search/install/caret-max/tamper-tarball/tamper-cache-heal/
  konflik/republish); `docs/pkg.md` § Registry.

### Batasan Phase 31
- Registry lokal/shared-dir saja (tanpa HTTP); tanpa yank;
  tanpa offline-flag eksplisit (implisit bila cache hangat);
  tanpa namespace/owners.

## [1.11.0] — 2026-09-07 (Phase 30: package manager)

Manajer paket sumber-lokal: `nusa pkg
init/add/remove/update/build/publish`, manifest `nusa.toml` (subset
TOML), resolver path-deps transitif, lock + SHA-256, build
reproduksibel.

### Ditambahkan
- `compiler/pkg/`: parser TOML-subset, manifest read/write/validasi,
  semver (parse/banding/minimum), SHA-256 (LLVM), checksum direktori,
  resolver (konflik/siklus/registry-deferred), 6 perintah.
- `nusa pkg build`: cek minimum compiler, auto-update bila lock absen,
  tolak bila basi, `NUSA_PATH` otomatis (warisan dipertahankan),
  output `target/<name>`; `publish`: tarball terurut + sha.
- Versi sumber-tunggal (`NUSA_VERSION` dari CMake) untuk
  Driver/main/pkg.
- Unit `test_pkg` (6: manifest, semver, sha vektor-uji, lock,
  checksum); `e2e_pkg.sh` (25 kasus); `docs/pkg.md`.

### Batasan Phase 30
- Tanpa registry (versi-deps ditolak dengan arahan Phase 31); tanpa
  range/caret; tanpa profile; publish = kemas lokal + dry-run.

## [1.10.0] — 2026-09-07 (Phase 29: standard library 2.0)

Modul std baru (kanonis `import std.x`), builtin string/waktu/
acak/proses/net, dan argumen CLI (`process_argc/argv` + penerusan
`nusa run`).

### Ditambahkan
- Builtin: `str_char_at/slice` (clamp, inline), `time_now_ms/s`,
  `random_u64` (/dev/urandom), `process_exit/env/cwd/argc/argv`,
  `tcp_connect/send/recv/close` (runtime C: getaddrinfo, loop-send,
  recv berbatas 1 MiB).
- `main(i32, ptr)` + global argv (diisi di entry; dibuat di
  generate agar fungsi std tak kehabisan); `nusa run prog.ns args...`
  meneruskan argumen.
- Import bertitik: `import std.time;` ≡ `import time;` (parser rantai
  Ident + strip prefix di resolver).
- Modul: `time` (elapsed), `random` (LCG MMIX + range/bool),
  `process` (prog_name/args/getenv_or), `path` (murni Nusa:
  basename/dirname/ext/join/normalize; tanpa break/else-if/||),
  `thread` (`par_map2` deterministik), `sync` (`with_lock`,
  atomic_inc/dec), `net` (`recv_line` murni Nusa).
- Runtime C: waktu/entropi/TCP (+ `<stdio.h>`, tanpa sys/random.h).
- Unit builtins 2 + parser 1; `e2e_stdlib2.sh` (9 kasus incl. TCP
  localhost); docs standard-library.

### Batasan Phase 29
- Channel tetap `i64`; net TCP saja (tanpa TLS/UDP); PRNG bukan
  kriptografis; `recv` tanpa close-semantics; argumen CLI via
  `nusa run`/`build` (binary langsung menerima argv OS).

## [1.9.0] — 2026-09-07 (Phase 28: concurrency)

Threads (pthread) + channel i64 + mutex + atomik + sleep sebagai
builtin, didukung runtime C yang di-link otomatis driver
(`clang prog.ll runtime/nusa_rt.c -o prog -pthread`).

### Ditambahkan
- `runtime/nusa_rt.c`: thread spawn/join (entry = trampolin closure,
  ABI LP64), channel FIFO tak-terbatas (mutex+condvar), mutex
  (+try_lock), atomik seq-cst, sleep_ms.
- 16 builtin reserved: `thread_spawn` (`fn() -> i64` ketat),
  `thread_join`, `mutex_new/lock/unlock/try_lock/free`,
  `chan_new/send/recv/free`, `atomic_new/get/set/add/free`,
  `sleep_ms` — cek `checkBuiltin`, emisi `genBuiltinCall`.
- Driver: penemuan runtime (`<exe>/../runtime`, `./runtime`,
  `$NUSA_PATH`) + `-pthread` selalu (tak merusak program lama).
- `docs/concurrency.md`; sentuh memory-model (§2, §3, §7, §10),
  standard-library, spec.
- Unit builtins 2 (IR declares + 10 penolakan); `e2e_concurrency.sh`
  (11 kasus deterministik: join/closure-antar-thread/atomic-4T/
  pipeline-15/mutex+channel, 5 penolakan, sleep smoke).

### Batasan Phase 28
- Channel `i64` saja; tanpa select/close/scoped-threads/pool;
  tanpa `async`/`await`/TLS; tanpa deteksi deadlock statis.

## [1.8.0] — 2026-09-07 (Phase 27: closures & functional features)

Lambda `|params| body` dengan capture by-value, lifting ke fungsi
hidden + struct env, dan panggilan tak langsung seragam via trampolin
`(ptr env, ...)` — kompatibel model memori (heap program-lifetime).

### Ditambahkan
- Lexer `|`; AST `Expr::Lambda`/`Closure` (+ dump/clone); parser
  lambda (param opsional-anotasi, body ekspresi dinormalisasi return
  atau blok).
- Semantic: scope lambda + dup-param; checker: propagasi tipe konteks
  (call/let/return/generik-dua-putaran), inferensi return kolektif,
  free-var walk scope-aware (lewati nested lambda, bind pola match),
  lift `$lambda_N`/`$env_N` + prepended lets, rewrite Closure.
- Codegen: pair heap `{trampoline, env}`, trampolin memoized
  (plain + lifted), indirect-call via trampolin, decay fn-ref ke pair,
  field fn di struct env + signature ikut di load.
- Guard match menolak lambda; fmt bar `|x|` menempel idempoten.
- `docs/closures.md` + sentuh types/syntax.
- Unit: lexer 1, parser 4, semantic 3, typecheck 9, fmt 1;
  `e2e_closures.sh` (10 kasus: 4 runtime, 5 penolakan, fmt).

### Batasan Phase 27
- Selalu by-value (tanpa move/borrow tiers); tanpa capture array;
  tanpa lambda di guard; tanpa rekursi closure.

## [1.7.0] — 2026-09-07 (Phase 26: enums & algebraic data types)

Enum dengan payload posisional, konstruktor qualified `E.V(...)`,
pola varian + exhaustiveness, dan generic enums — semuanya via desugar
hidden struct (`tag` + slot payload), tanpa codegen baru kecuali
toleransi field-hilang dan flag layout.

### Ditambahkan
- Lexer `enum`; AST `EnumVariant`/`EnumDecl` (+ `Program.enums`,
  `Pat::Variant.tag`, `MethodCall.targs`, flag `StructDecl.hiddenEnum`
  + `StructLit.enumCtor`).
- Parser `enum E<T> { V, W(T, ...) }` (koma, trailing comma, `V()` =
  unit) + ctor eksplisit `E<T>.V(args)` (lookahead seimbang).
- Semantic: namespace tipe bersama struct/alias, dup varian, receiver
  enum di MethodCall/FieldAccess, blokir `E` sebagai value / `E {...}`,
  hint ctor tak-qualified.
- Checker: `processEnumDecl` → hidden struct pre-pass-1a;
  `checkEnumCtor` (inferensi via unify + eksplisit + mismatch per
  posisi); pola varian enum (reinterpretasi bind-Kapital, tag test +
  destruktur slot); exhaustiveness seluruh varian (pesan missing);
  blokir tempa struct/pattern; print enum mengarah ke match.
- `impl Trait for Enum` + metode bekerja gratis (dispatch statis).
- `nusa doc` seksi `## Enums`; `docs/enums.md` + sentuh
  syntax/types/pattern-matching/error-handling/spec.
- Unit: lexer 1, parser 3, semantic 5, typecheck 10; `e2e_enums.sh`
  (15 kasus: 5 runtime, 8 penolakan, fmt, doc).

### Batasan Phase 26
- Konstruktor sintaktis (bukan first-class fn); tanpa payload bernama/
  or-pattern/range/repr; exhaustiveness top-level saja; std
  Option/Result tetap struct (sengaja).

## [1.6.0] — 2026-09-07 (Phase 25: pattern matching)

Statement `match` dengan desugar ke `let` + rantai `if`/`else` biasa
(tanpa codegen baru): literal int/bool/string, `_`, bind, struct,
guard `if`, dan varian std `Some(v)`/`None`/`Ok(v)`/`Err(e)` untuk
Option/Result. Exhaustiveness wajib (bool + pasangan varian tertutup
tanpa wildcard; literal ganda + unreachable ditolak). `nusa fmt`
memformat arm match (idempoten).

### Ditambahkan
- Lexer `match` + `=>`; AST `Pat`/`MatchArm`/`MatchStmt` (+ guard,
  dump, clone).
- Parser pola: literal (negatif didukung), `_`, bind, struct
  (+ targs), varian `Name(...)`, `None` telanjang, guard
  `<pola> if <kondisi>`.
- Checker: validasi type-directed, exhaustiveness, desugar flag
  (`$matchK`/`$mK`), inline guard (binding → klon aksesor murni),
  transparansi `if true` untuk all-paths, arm codegen defensif.
- Semantic: scope per-arm + guard boleh memakai binding.
- `docs/pattern-matching.md`; sentuh `syntax`/`control-flow`/
  `error-handling`/`spec`.
- Unit typecheck 14, `e2e_match.sh` (13 kasus: 5 runtime, 7 penolakan,
  fmt).

### Batasan Phase 25
- Statement-only; tanpa pola or/range; tanpa exhaustiveness int/string
  umum (butuh `_`); varian enum pengguna = Phase 26.

## [1.5.0] — 2026-09-07 (Phase 21–24: type system, traits, memori, error)

Rilis kumulatif pasca-1.1.0: mencakup target Phase 21 (1.2.0),
22 (1.3.0), 23 (1.4.0), dan 24 (1.5.0) dalam satu rilis.

### Phase 24: error handling

#### Ditambahkan
- `Option<T>` / `Result<T, E>` / `Error` di `std/` (encoding struct
  jujur + dummy terdokumentasi; sum types asli di fase enum).
- Kombinator bebas generik (`is_some`, `opt_unwrap_or`, `opt_map`,
  `is_ok`, `res_unwrap_or`, `res_map`, `res_map_err`, …).
- Operator propagasi `expr?` (operan dan return fungsi harus
  `Result` sama; `Option`/void/E-beda ditolak jelas).
- Builtin `parse_int_valid` (konsisten `strtoll`) +
  `parse_int_opt`/`file_read_opt` murni-Nusa; API lama utuh.
- `nusa fmt` menempelkan `?`; `docs/error-handling.md`.

#### Tes (Phase 24)
- Unit: lexer 1, parser 1, typecheck 5, builtins 2, fmt 1.
- `e2e_error.sh` (9 kasus: aliran, kombinator, validasi, 5 penolakan,
  fmt).

#### Batasan Phase 24
- Konstruktor butuh dummy; tanpa konversi error otomatis; tanpa `?`
  untuk `Option`; tanpa exception/panic; matching via `if` (menunggu
  fase pattern matching).

### Phase 23: memory model

#### Ditambahkan
- `docs/memory-model.md` (dokumen desain resmi): evaluasi opsi,
  model terpilih (owned values + program-lifetime heap), dan 8 area
  (Allocation, Deallocation, Ownership, References, Mutability,
  Lifetime, Stack, Heap).
- Komentar "bocor sengaja" diganti status resmi by-design.
- `e2e_memory.sh` (4 kasus: copy, concat-loop, input-EOF, struct).

#### Batasan Phase 23
- Loop alokasi tumbuh per iterasi (terdokumentasi); tanpa TCO;
  revisi saat referensi/heap user ada.

### Phase 22: traits/interfaces

#### Ditambahkan
- Deklarasi `trait T { fn m(self, …) …; }` (metode tanpa body;
  `self` telanjang pertama; keyword `trait`/`impl`/`for`/`self`).
- `impl Trait for Type { … }` (target: struct biasa/generik-konkret,
  primitif; array/fn/void ditolak; duplikat impl ditolak).
- Panggilan `recv.method(args)`: resolve statis (tipe, nama) →
  rewrite ke fungsi mangled (`Trait_method_Type`); codegen nol baris.
- Bounds `T: Trait` (+ ganda `+`) di tparam fungsi dan struct;
  dicek saat instansiasi konkret (`does not implement trait`).
- Diagnostik: unknown/ambiguous method (+ daftar), mismatch/arity
  (`'T.m'`), missing/not-member/dup-impl/non-self, method-vs-field
  hint, privacy `_`, tabrakan nama mangled.
- `nusa doc`: seksi `## Traits`; `nusa fmt` untuk trait/impl/call.

### Tes (Phase 22)
- Unit: lexer 1, parser 6, semantic 8, typecheck 17, fmt 1, doc 1.
- `e2e_traits.sh` (18 kasus: master, bounds, primitif, generik,
  modul, privacy, 12 penolakan, fmt, doc).

### Batasan Phase 22
- Tanpa metode/impl generik, supertrait, body default, tipe `Self`,
  associated function, dynamic dispatch. Koherensi: unik program-wide.

### Phase 21: type system

Slice pertama Phase 21 (Advanced Type System). Tanpa perubahan bahasa
selain penambahan deklarasi `type`; tanpa perubahan IR/ABI.

### Ditambahkan
- Deklarasi `type Name = T;` (top-level, keyword `type` reserved):
  `i64`/`bool`/`string`/`void`/`[T]`/bernama sebagai target.
- Resolusi alias transparan di typechecker (forward reference OK,
  dikanonikalisasi in-place agar codegen tak berubah).
- Validasi eager: target unknown, `circular type alias` (dengan rantai
  `A -> B -> A`), `type alias chain too deep` (> 100) — tepat satu
  error per masalah (memo situs gagal).
- Namespace tipe bersama struct (`duplicate type`), tolak nama
  reserved, tolak konstruksi struct via alias.
- `nusa doc`: seksi `## Type Aliases`; `nusa fmt` memformat deklarasi
  alias (`type Nums = [i64];`).

### Tes
- Unit: lexer keyword; parser (parse/dump/3 penolakan); semantic
  (6: ok/duplikat/2 arah tabrakan/reserved/structlit); typecheck
  (15: resolve/forward/rantai/unknown 1x/siklus 2/depth/dup-direct/
  tabrakan-direct/array-param/field/void); fmt; doc.
- `e2e_alias.sh` (11 kasus: nilai runtime 42/3, 7 penolakan, fmt, doc).

### Batasan slice 21a (ke slice 21 berikutnya)
- Tanpa nullable/optional (ditunda ke Phase 24 bersama `Option`/`Result`).
- Tanpa type constraints/bounds (Phase 22, butuh traits).

### Batasan slice 21c (ke slice 21d)
- Tanpa struct generik, tanpa type constraints/bounds, tanpa argumen
  tipe eksplisit (inferensi saja).

### Slice 21d — generic structs + struct by-value

- Sintaks `struct P<T, U> { … }`; aplikasi `Point<i64>` (nested OK);
  literal eksplisit `Point<i64> { … }` (lookahead seimbang; `if a<b {`
  tetap perbandingan).
- Monomorphisasi cermin fn: inferensi field/eksplisit, instance layout
  mangled (`Point_i64`), template dibuang; rigid dipertahankan agar
  diagnostik cantik; siklus = error ukuran-infinit; cap 64 bersama fn.
- Struct by-value di param/return (menutup TODO v0.8.0 "records"):
  load/store di call/return; berlaku umum (bukan cuma generik).
  Test penolakan lama dikonversi menjadi penerimaan + runtime.
- Tabrakan nama mangled vs deklarasi user ditolak (fn + struct).
- `fmt`: scan bracket seimbang untuk aplikasi tipe; `doc`: header
  `struct P<T>`. Spaceless `T>=` tetap butuh spasi (ambigu perbandingan).

### Tes slice 21d
- Unit: parser 4, semantic 1, typecheck 11, codegen 1 (IR record),
  fmt 1, doc 1.
- `e2e_structs_gen.sh` (14 kasus: runtime nested/eksplisit/alias/
  by-value, IR instance-only, 9 penolakan, fmt, doc).

### Slice 21b — function types

- Sintaks tipe `fn(T, …) -> T` di posisi anotasi (`-> T` absen = `void`).
- Function reference: nama fn top-level sebagai value (`let op = add;`);
  `main`/builtin/`print` sebagai value ditolak; privacy `_` dicerminkan.
- Panggilan tak langsung via variabel/locals/param/return (`f(x)`,
  `apply(add, …)`, `mk()` mengembalikan fn); arity/mismatch dicek sama
  seperti panggilan langsung (helper bersama `checkCallArgs`).
- Codegen: `ValueKind::Func` (pointer), decay bitcast, signature per
  nama (`funcTypes_`), indirect `call` via loaded ptr (void tanpa nama
  hasil — regression teruji di IR).
- Ditolak jelas: `print(f)`, `f == g`, `[f]`, field `f: fn…`, panggil
  non-fungsi.
- Batasan slice 21b: tanpa closure/lambda/currying (tanpa captures).

### Slice 21c — generic functions (monomorphisasi)

- Sintaks `fn f<T, U>(...)` (tparams; `main` generik dan dup tparam
  ditolak); inferensi tipe di call-site dari argumen (tanpa turbofish).
- Monomorphisasi di checker: unifikasi struktural (tembus `[T]`/`fn`),
  konflik/arity/uninferrable ditolak dengan pesan panggilan yang sama;
  tiap kombinasi argumen → instance konkret bernama-mangled
  (`identity_i64`) yang dicek penuh (aturan void/array/struct natural
  berlaku); template dibuang sebelum codegen (codegen nol baris).
- Body template dicek per-instance (model C++); cache instance
  (rekursi tipe-sama berhenti); cap kedalaman 64 anti-divergensi;
  state checker (env/ret/func) save-restore anti-korupsi bersarang.
- Fungsi generik sebagai value ditolak eksplisit (inferensi butuh
  argumen konkret).
- `fmt` merekatkan bracket generik (`f<T, U>(`); perbandingan `a < b`
  tak tersentuh. `doc` merender `f<T>(…)`.

### Tes slice 21c
- Unit: parser 3, semantic 1, typecheck 9, fmt 1, doc 1 (clone via
  semua jalur instance).
- `e2e_generics.sh` (12 kasus: runtime 5 nilai + nested/rekursi,
  IR instance-only tanpa template, 7 penolakan, fmt, doc).

### Tes slice 21b
- Unit: parser 3 (parse/dump/2 gagal), semantic 5, typecheck 11,
  codegen 2 (bentuk IR + regression void), fmt 1.
- `e2e_fn.sh` (14 kasus: runtime 42/3/7/30/100/100/6, 10 penolakan,
  alias-to-fn, lintas modul, fmt).

## [1.1.0] — Diagnostics & Recovery (2026-09-06)

Fase 20 (pasca-1.0.0): stabilitas compiler, diagnostics, error
recovery, locations, messages, formatter, test infra, dan pengukuran
performa compiler. Tanpa perubahan bahasa (spec v1.0.0 tetap berlaku).

### Ditambahkan
- Error recovery top-level: parser melanjutkan ke item berikutnya
  (`import`/`struct`/`fn`) setelah item gagal — semua error parse
  dilaporkan sekaligus (dulu: berhenti di item pertama).
- Error cap 25 per tahap (lexer + parser): file sampah yang dulu
  mencetak 200+ error kini berhenti di 25 + satu ringkasan
  "too many errors, stopping" (rc tetap 1, tanpa crash/hang).
- Token `Unknown` (kerusakan yang sudah dilaporkan lexer) dibuang di
  konstruktor parser — tidak ada error parser duplikat.
- `bench/bench.sh`: mengukur waktu `nusa build` + ukuran executable
  (hello/nusa10/perf2000) + ukuran biner compiler.
- `e2e_diagnostics.sh`: recovery multi-error, error cap, hint Inggris,
  guard sumber (tidak ada literal `contoh:` di sumber compiler).

### Diubah
- Diagnostics user-facing kini seluruhnya bahasa Inggris (dulu campur
  Indonesia: `contoh:` → `example:`, ~40 hint diterjemahkan setia di
  parser/typechecker/semantic/codegen/driver). Format
  `file:line:col: error:` + `help:` + snippet/caret tidak berubah.
- Formatter: spasi sebelum `[` literal array diperbaiki
  (`let numbers: [i64] = [1, 2, 3, 4];` — dulu `:[i64] =[1, ...]`);
  `[` indexing tetap menempel (`a[0]`). `examples/arrays.ns` dan
  `tests/strings_test.ns` diformat ulang (hanya spasi).

### Diverifikasi
- Lokasi sumber: error di modul import menunjuk file:line:col yang
  tepat; span multi-karakter (`^^^^`) tepat — tanpa perubahan kode.
- 21/21 ctest lulus (20 warisan + `e2e_diagnostics` baru), suite 120.71 dtk.
- ASan+UBSan bersih: 8 unit test + `e2e_diagnostics`/`e2e_tooling`/`e2e_final`.
- `nusa fmt --check` bersih di seluruh `.ns` repo.

### Baseline performa v1.1.0 (`bench/bench.sh`, AArch64 Termux)
- `hello`: build ~1.2–1.7 dtk, exe 5704 bytes.
- `nusa10`: build ~1.3 dtk, exe 6104 bytes.
- `perf2000` (2000 statement): build ~7.3 dtk, exe 45712 bytes.
- `nusa` compiler: 449640 bytes (v1.0.0: 447648; +1992 untuk kode baru).
- Suite ctest penuh: 120.71 dtk (v1.0.0: 118.29 dtk; setara + 1 test baru).

## [1.0.0] — Stable Nusa (2026-09-05)

Rilis stabil sesuai MASTER PROMPT. Bukan versi fitur: verifikasi
menyeluruh bahwa seluruh pipeline, bahasa, tooling, stdlib,
diagnostics, dan dokumentasi memenuhi Definition of Done 12/12.

### Ditambahkan
- Diagnostics span: `length` di `SemanticError`/`ParseError`/
  `ModuleError`, dirender caret beruntun + layout blank-line persis
  contoh master (`^^^^^^` untuk identifier). Diisi di situs
  identifier-presisi (variabel/fungsi/struct/field/call).
- Dokumentasi wajib 1.0: `types.md`, `functions.md`, `control-flow.md`,
  `arrays.md`, `structs.md`, `modules.md`; `stdlib.md` →
  `standard-library.md`; spesifikasi dasar `spec.md` (leksikal, grammar,
  tipe, semantik runtime, modul, diagnostics, batasan normatif).
- Program uji 1.0 verbatim (`examples/nusa10.ns`) + `e2e_final`
  (build+run+output eksak + IR + format diagnostics + 8 CLI).

### Diverifikasi (Definition of Done 12/12)
- [x] clean build berhasil (0 warning, 0 error)
- [x] semua unit tests lulus
- [x] semua integration tests lulus (20/20 ctest)
- [x] contoh program berhasil dikompilasi (program 1.0 + semua examples)
- [x] executable AArch64 dijalankan di Termux (`file` + run)
- [x] LLVM IR terverifikasi (`verifyModule` + inspeksi `@main`/`@add`/`%Person`)
- [x] compiler tidak crash pada malformed input (`e2e_hardening`)
- [x] diagnostics bekerja (format = contoh master, e2e_final)
- [x] dokumentasi tersedia (10 file wajib + spec + README)
- [x] CLI bekerja (8/8 perintah, e2e_final)
- [x] standard library dasar bekerja (`nusa test tests/` hijau)
- [x] regression tests tersedia (unit 9 + e2e 11 file)

### Batasan final (sesuai master — roadmap 1.x/2.x)
- Tanpa generics, traits, advanced pattern matching,
  ownership/borrowing, garbage collector, async/await, concurrency,
  macros, FFI kompleks, package manager, self-hosting,
  cross-platform toolchain.

## [0.9.5] — Hardening (2026-09-05)

Bukan versi fitur: stabilitas compiler, error handling, regression
testing, verifikasi LLVM, memory safety, diagnostics. Semua behavior
penting kini terkunci test; compiler tak boleh crash pada malformed input.

### Ditambahkan
- Guard rekursi anti-stack-overflow (batas 500): RAII `DepthGuard` di
  `parseExpr`/`parseUnary`/`parseBlock`/`parseType` + guard di 3 walker
  (`Semantic`/`TypeChecker`/`Codegen::checkExpr`/`genExpr`). Rantai
  biner/unary/index 20k yang lolos parser iteratif tertangkap walker.
  Nesting wajar (100) tetap jalan.
- Division-by-zero: literal → compile error; dinamis → block `div.zero`
  (pesan + exit 1). Sebelumnya: garbage diam-diam (LLVM fold ke undef).
- `emitOomCheck` (malloc null → pesan + exit 1) di `str_concat` dan
  `file_read`; `ftell` negatif → jalur kosong + `fclose` (tanpa fd leak).
- Cap rantai import 100 (`import chain too deep`, tanpa stack overflow).
- `e2e_hardening` baru (30+ kasus): matriks 9 negative wajib MASTER
  PROMPT (syntax, undefined/duplikat variable, mismatch, arity, return,
  field, index, module) + robustness (nesting 20k expr/block, rantai
  biner/unary/index, korpus 12 malformed incl. NUL/0xFF/terpotong/kosong,
  self-import, rantai 150 vs 5, div-0 statis/dinamis, CLI edges) +
  regression (`if c {`, ident 5000 char) + sanity performa (4000 stmt).
- Unit: depth guard parser (3), div-0 statis + dinamis-OK (2),
  IR `div.zero` (1).
- `verifyModule` wajib sebelum emit — ditegaskan sebagai backstop
  (audit: terpanggil di semua jalur generate).

### Diperbaiki (temuan selama milestone)
- **SIGSEGV nyata**: nesting paren 50k → crash (rc=139). Akar: guard
  hanya di call-depth parser; rantai iteratif + walker rekursif lolos.
  Diperbaiki 4 guard walker + `parseUnary`; teruji ke 20k multi-bentuk.
- **Div-by-zero diam**: `print(1/0)` mencetak garbage (`549262527400`)
  exit 0. Kini error statis + runtime.
- Kasus `let x = voidFn()` terverifikasi ditolak bersih saat audit
  (`cannot declare variable … with 'void' value`).
- Replace global ceroboh `0.9.0→0.9.5` di docs (riwayat versi dirusak)
  — diperbaiki manual per bagian; pelajaran: bump versi surgical.

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **19/19 passed**.
- Korpus malformed: semua exit != 0, rc ≤ 128 (tanpa sinyal), berpola pesan.
- `let x = voidFn()` → error; direktori sebagai `file_read` → `""`/0.
- Performa: 4000-statement build+run benar (± tercatat di e2e).

### Batasan yang diketahui
- Batas nesting 500 & rantai import 100 (error bersih, bukan crash).
- Aritmetika wrapping (kecuali div-0); `INT64_MIN / -1` wrap diam.
- `while true {}` tanpa break tetap hang (oleh desain; `nusa test`
  memagari dengan timeout 10 dtk).
- Tanpa package manager, generics (roadmap 1.x/2.x).

## [0.9.0] — Standard Library & Tooling (2026-09-05)

Milestone stdlib & tooling sesuai MASTER PROMPT: mulai `std/`
(io/math/string/collections/fs/system), 11 builtin runtime, dan CLI
`check`/`test`/`fmt`/`doc` + formatter sederhana + dokumentasi API.
Minimal tetapi nyata dan teruji.

### Ditambahkan
- Builtins global (reserved, ter-typecheck, emisi libc langsung):
  `input` (getline + strip newline, EOF→`""`), `assert` (pesan lokasi +
  exit), `str_len`, `str_concat` (malloc), `parse_int` (strtoll),
  `file_read` (fopen→ftell→malloc→fread, hilang→`""`), `file_write`,
  `file_exists`, `array_sum/min/max` (loop inline, init 0/MAX/MIN).
  Heap string bocor sengaja (GC = roadmap 1.x/2.x).
- `std/`: io (`read_int`), math murni (abs/min/max/clamp/pow/fact/gcd/
  is_even/isqrt Newton), string (repeat/pad via builtins), collections
  (panduan builtin — array tak bisa lewat batas fungsi), fs
  (copy_file/file_size), system (`die`). Semua `///`-documented.
- Resolusi modul: lokal → `$NUSA_PATH` → `<exe>/../std`
  (`/proc/self/exe`); `import string;` valid (keyword tipe diterima
  sebagai nama modul).
- CLI: `check` (tahapan bersama `checkStages`, tanpa codegen),
  `test` (`*_test.ns`, fork+alarm 10 dtk, stdio anak dialihkan,
  ringkasan), `fmt` (+`--check`), `doc` (stdout/`-o`).
- Formatter token-based: aturan terdokumentasi, unary kontekstual,
  string re-escape, komentar utuh, deterministik + idempoten (diuji).
- Docgen: signature AST + `///` menempel + intro modul → Markdown.
- Lexer menangkap komentar (`line/text/doc`); parser tak tersentuh.
- Contoh dogfood `tests/{math,strings,files}_test.ns` (jalan via
  `nusa test tests/`).
- Tests: `test_builtins` (typing + IR libc + reserved), `test_fmt`
  (golden + idempotensi + escape), `test_doc` (fn/struct/intro/error),
  `e2e_stdlib` (runtime + pipe + roundtrip + std-resolve + assert-gagal),
  `e2e_tooling` (check/test/fmt/doc incl. timeout/build-error).
- Docs: `docs/stdlib.md` (referensi API), README/getting-started/
  compiler diperbarui.

### Diperbaiki (temuan selama milestone)
- `ConstantInt::get` bertipe `Constant*` di LLVM ini (bukan
  `ConstantInt*`) — satu baris array_sum.
- `nusa test` ganda-print: buffer stdio terwarisi anak fork —
  `fflush(nullptr)` sebelum `fork` (pelajaran klasik).
- Ekspektasi e2e (copy_file true; kebutuhan `import fs`) — data, bukan kode.

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **18/18 passed**.
- `echo 5 | nusa run` (import io+math) → `120`; `nusa test tests/` →
  `3 passed, 0 failed`; `fmt` idempoten; `doc` golden.
- AArch64 executable; `std` ter-resolve dari direktori mana pun.

### Batasan yang diketahui
- String buram (tanpa `==`/substring/indexing); file tanpa error rinci;
  array fixed-size lokal (tanpa param/return/generik).
- `fmt` sederhana (aturan token); `test` sekuensial, timeout 10 dtk statis.
- Tanpa package manager (sesuai master v0.8.0).
- `nusa check` memvalidasi program (file lib tanpa `main` tetap error).

## [0.8.0] — Struct & Module (2026-09-05)

Milestone struct & module sesuai MASTER PROMPT: deklarasi struct,
konstruksi literal, akses field (termasuk nested), named LLVM types,
plus module loader — `import` multi-file dengan namespace merge,
visibilitas dasar, dan deteksi circular import. Tanpa package manager.

### Ditambahkan
- Lexer: keywords reserved `struct`, `import`; token `.` (Dot) untuk
  field access (tanpa ambiguitas float — Nusa hanya punya integer).
- Tipe bernama: `TypeKind::Struct` + `sname` + factory `Type::named()`;
  `name()` mengembalikan nama struct; `==` struktural per nama.
  `parseType` menerima `Ident` sebagai tipe (keberadaan dicek checker →
  `unknown type 'Foo'`).
- AST: `StructDecl`/`StructField`/`Import` top-level
  (`Program{imports, structs, functions}`); `StructLit` (`(Person …)`)
  dan `FieldAccess` (`(field base f)`); dump `(struct P …)`,
  `(import m)`.
- Parser: loop top-level `import`/`struct`/`fn` bebas campur;
  `struct P { f: T; … }` (`;` opsional); literal `P { f: v, … }`;
  postfix `.f` berantai dan bercampur index (`a.b[0]`, `a[0].b`).
  Ambiguitas `if c {` (block) vs literal dipecahkan lookahead 2-token
  (`{` diikuti `}`/`field :` → literal), tanpa backtracking.
- Semantic: namespace struct terpisah (ala Rust — koeksis dengan fungsi);
  tabel definisi; literal dicek (unknown field + daftar field sebagai
  hint, duplikat, missing); privacy `_` antar-file untuk call dan
  konstruksi (`is private to module`); reserved `len`/`exit`/`print`.
  Field access diteruskan ke checker (butuh tipe base).
- TypeChecker: tabel struct (dup field, resolve tipe incl. unknown,
  tolak field `void`/array — array butuh ukuran statis); literal vs
  deklarasi per field (`mismatched field type`); akses → tipe field
  (unknown + daftar field); nested struct didukung penuh; tolak
  print/`==`/param/return struct dengan pesan jelas. Bonus: `let x`
  dari nilai `void` kini ditolak (bug laten sejak v0.5.0).
- Codegen: named LLVM struct types two-phase (`%Person = type {…}`,
  forward-ref antar-struct OK); nilai struct = pointer (tanpa load);
  konstruksi = alloca + store per field urutan deklarasi (nested via
  copy agregat); akses = StructGEP + load (skalar) / pointer (nested);
  `let`/`assign` via `storeGenValue` terpadu (skalar langsung, agregat
  terverifikasi); guard param/return struct; `verifyModule` wajib.
- Module loader (`compiler/driver/Modules`): `import m;` →
  `<dir>/m.ns`; DFS + visited (dedupe) + stack rantai
  (`circular import detected: a -> b -> a`); identitas absolut,
  tampil relatif; `std::filesystem` via `error_code` (build
  `-fno-exceptions`); diagnostic per file (engine per modul);
  merge deps-dulu; `imports` dikonsumsi. Driver: lex/parse per modul
  diganti loader; semantic→codegen jalan di program gabungan.
- Contoh: `examples/person.ns` → `Nusa/1`; `examples/math.ns` +
  `examples/main.ns` (import + struct lintas modul) → `6/Ada`.
- Tests: lexer 16 (+struct/import/dot), parser 49 (+struct/import/field,
  regresi ambiguitas `testBareIdentCondition`), semantic 31 (+struct/
  field/privacy), typecheck 54 (+field/nested/unknown/param), codegen 19
  (+named type/nested IR), `e2e_struct_module` baru (10 kasus: person,
  modul, nested, lintas-modul, privacy, circular, import hilang,
  dup struct, tipe field, param struct).

### Diperbaiki (temuan selama milestone)
- Bug nyata: `if c {` / `while done {` dispekulasi sebagai struct literal
  (`expected field name`) — diperbaiki dengan lookahead, terbukti oleh
  kembalinya 5 test yang sempat gagal (branch-return, e2e_control, …).
- Ekspektasi test disesuaikan desain: `testBadTypeFails` →
  `testNamedTypeParses` (tipe bernama valid di grammar); unknown-field
  kini 2 diagnostic valid (unknown + missing).

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **13/13 passed**.
- Contoh master verbatim: struct → `Nusa` + `1`; modul → `21` + `Nusa`;
  AArch64 executable; IR `%Person = type { ptr, i64 }` + GEP field.
- Circular `a ↔ b` → rantai jelas, exit 1; privacy `_secret` lintas
  modul → ditolak; import hilang → pesan + hint.

### Batasan yang diketahui
- Struct: tanpa field array, param/return struct, `==`/print struct;
  field selalu publik; tanpa methods/traits.
- Modul: flat tetangga (tanpa subdir/`::`/package manager — sesuai master);
  merge dengan konflik = error duplikat.
- Tanpa generics (sesuai master).
- `nusa check/test/fmt/doc` belum ada (roadmap v0.9.0).

## [0.7.0] — String & Array (2026-09-05)

Milestone string & array sesuai MASTER PROMPT: literal, indexing,
length, bounds checking — minimal, tanpa generic collections.

### Ditambahkan
- Tipe `[T]` (`TypeKind::Array` + elem; ukuran BUKAN bagian tipe).
- AST: `ArrayLit` `(array …)`, `Index` `(index base idx)`; parser:
  literal `[a, b]`, postfix `a[0][1]` berantai, tipe `[T]`/`[[T]]`.
- Semantic: builtin `len()` (bukan fungsi user); `fn len`/`fn exit`
  reserved (tabrakan simbol runtime); subekspresi index dicek.
- TypeChecker: elemen homogen non-void non-array; `[]` tanpa anotasi
  ditolak; index non-array/non-`i64` ditolak; `len()` 1 arg array → i64;
  print/`==`/param/return array ditolak jelas; `[void]` ditolak.
- Codegen: alloca `[N x T]` entry; nilai array = pointer (tanpa load);
  indexing via GEP + runtime bounds check (`oob.fail` → pesan +
  `exit(1)` + `unreachable`); copy agregat (`let b = a`, `b = a`);
  `len()` = konstanta kompilasi; `void @exit(i32)` dideklarasikan.
- String: `let`/`print` variabel string (ada sejak v0.3.0) + regression.
- Contoh `examples/arrays.ns` → `Hello/1/4/30`.
- Tests: parser +5, semantic +3, typecheck +14, codegen +2,
  `e2e_arrays` baru (11 kasus: nilai, dinamis, copy, OOB ±, 6 penolakan).

### Diperbaiki (temuan selama milestone)
- Contoh `arrays.ns` salah hitung (10, bukan 30) — diperbaiki dengan
  `doubled[0] + doubled[1] + total`.

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **12/12 passed**.
- Contoh master verbatim: `numbers[0]` → `1`, `message` → `Hello`;
  AArch64 executable; IR `alloca [4 x i64]` + GEP + `exit` terverifikasi.
- OOB (±) → `runtime error: index out of bounds`, exit 1.

### Batasan yang diketahui
- Array fixed-size dari literal (tanpa `push`/resize); tanpa indexed
  assignment (`a[0] = x`); tanpa nested arrays; tanpa param/return array
  (butuh slices — mendatang); tanpa `==`/print array; `[]` butuh isi;
  tanpa generic collections (sesuai master).

## [0.6.0] — Control Flow (2026-09-05)

Milestone control flow sesuai MASTER PROMPT: `if`/`else`, `while`,
nested if, nested loops — dengan LLVM basic blocks yang benar
(entry/then/else/merge, loop.cond/body/after).

### Ditambahkan
- Parser: `if Expr Block [else Block | else if …]`, `while Expr Block`
  (tanpa parens di kondisi, sesuai contoh master); dangling-else terikat
  ke `if` terdekat via rekursi; `else` nyasar & `{` hilang ditolak jelas.
- AST: `IfStmt`, `WhileStmt`, `AssignStmt` (`(set x …)`); `Block` dipegang
  via `unique_ptr` (memutus siklus definisi); dump `(if c (t) (e))`,
  `(while c (body))`.
- `Scopes<T>` (`compiler/semantic/Scopes.h`, header-only): tumpukan scope
  leksikal dipakai identik oleh semantic, checker, dan codegen.
  Shadowing antar-scope diizinkan; duplikat se-scope tetap ditolak
  (aturan v0.9.5 `duplicate variable` terpenuhi).
- Semantic: tiap Block scope baru; var luar terlihat di dalam, tak
  sebaliknya; assignment memeriksa eksistensi (`undefined variable`).
- TypeChecker: kondisi `if`/`while` wajib `bool`; unreachable per-block;
  generalisasi all-paths (v0.5.0 return-last → tiap cabang `if`/`else`
  harus return; `while` tak dihitung karena bisa nol iterasi —
  konservatif & terdokumentasi); assignment dicek tipenya.
- Codegen: `genBlock` (scope+terminator), `genIf` (CondBr then/else/merge),
  `genWhile` (br cond → CondBr body/after → br cond); return di cabang
  → `ret` benar; merge tak-terjangkau di non-void → `unreachable`
  (IR tetap valid); semua alloca di entry.
- Assignment `x = expr;` (v0.6.0): mutasi agar loop bisa progres —
  klausul "jika diperlukan" v0.3.0 kini terpicu; tipe harus sama.
- Contoh `examples/control.ns` → `10/3/2/1`.
- Tests: parser +7 (master/else-if/nested/missing-brace/stray-else/assign)
  & `testIfRejected` pensiun; semantic +4 (shadow/visibility/dup-block);
  typecheck +9 (cond-bool, both-return, no-else, while-return, unreachable
  cabang, void-branch, assign ±); codegen +3 (then/else/merge,
  loop.cond/body/after, branch-return); `e2e_control` baru (9 kasus +
  timeout anti-gantung + cek IR).

### Diperbaiki (temuan selama milestone)
- Void-call butuh statement → `ExprStmt` (v0.5.0, sudah ada) kini terpakai
  penuh; tiga `std::get` final diganti cabang eksplisit (anti crash).
- `.gitignore`: `*.ns.ll` (artefak IR regenerable tak mengotori tree).

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **11/11 passed**.
- Contoh master verbatim → AArch64 → `10` lalu countdown `3/2/1`;
  nested loops → `11..33`; sum 1..10 → `55`; IR memuat label
  `then/else/merge/loop.cond/loop.body/loop.after` + `br i1`.

### Batasan yang diketahui
- `while true { return …; }` tak dihitung sebagai return (konservatif);
  tanpa `break`/`continue` (belum di roadmap — diusulkan 1.x bila perlu);
  tanpa assignment gabungan (`+=`); kondisi non-bool ditolak.

## [0.5.0] — Functions (2026-09-05)

Milestone functions sesuai MASTER PROMPT: deklarasi berparameter,
return type, return statement, calls, local scope, validasi argumen —
dengan LLVM function dan `call` yang benar.

### Ditambahkan
- Lexer: token `->` (greedy; `Minus`+`Gt` berdampingan tak pernah valid).
- AST multi-fungsi: `Program.functions`, `Param`, `retType` (default void),
  `ReturnStmt`, `Call`, `ExprStmt` (call-statement fungsi void);
  dump `(program (fn add(a: i64, b: i64) -> i64 …))`, `(call f …)`,
  `(return …)`, `(do …)`.
- Parser: `fn name(params) [-> Type] block`, param `name: Type` (anotasi
  wajib), `return [expr]`, call di primary (lookahead `Ident (`),
  call-statement; `main` tanpa parameter & return void dipertahankan.
- Semantic: namespace fungsi vs nilai; `main` wajib & tak callable;
  reserved `print`/`puts`/`printf`; local scope per fungsi (param + `let`);
  `undefined/duplikat function`, `duplicate parameter`; two-pass
  (forward reference OK).
- TypeChecker: signatures two-pass (rekursi OK), arity + tipe argumen,
  return cocok (`mismatched return type`), missing/bare return,
  unreachable sesudah `return` (aturan straight-line v0.5.0), param
  `void` ditolak.
- Codegen: two-pass declare→define; `define i64 @add(i64 %a, i64 %b)`,
  `call`, param alloca+store, `ret`/`ret void`/`unreachable` defensif;
  `main` tetap `i32 @main` return 0; `verifyModule` wajib.
- Contoh `examples/functions.ns` → `30`.
- Tests: lexer +1 (Arrow), parser +8 & dump direvisi, semantic +8,
  typecheck +11, codegen +3 (`define/call/ret` + forward + bool/str/void),
  `e2e_functions` baru (9 kasus: milestone, multi, forward, build-only
  rekursi, arity, tipe argumen, undefined fn, missing return).

### Diperbaiki (temuan selama milestone)
- `ExprStmt` ditambahkan: void-call sebagai statement (`greet("Nusa");`)
  adalah kebutuhan nyata yang terlewat dari desain awal; tanpanya fungsi
  void tak terpanggil. Semua `std::get<ReturnStmt>` final di
  semantic/checker/codegen diganti cabang eksplisit (anti
  `bad_variant_access`).

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **10/10 passed**.
- Program milestone verbatim → AArch64 executable → `30`; IR:
  `define i64 @add(i64 %a, i64 %b)`, `call i64 @add(i64 10, i64 20)`,
  `ret i64` — sesuai tuntutan master.

### Batasan yang diketahui
- Fungsi top-level saja; `return` harus terakhir; tanpa closures/generics;
  control flow = v0.6.0.

## [0.4.0] — Static Type System (2026-09-05)

Milestone type system sesuai MASTER PROMPT: anotasi, tipe dasar,
type checking, kompatibilitas, inferensi sederhana, function type
checking. Tanpa generics / advanced types.

### Ditambahkan
- Modul `compiler/types/`: `Type` (value, copyable) — `i64`, `bool`,
  `string`, `void`, `Func(params, ret)`, `Error` anti-kaskade;
  `name()` (`fn() -> void`) + `operator==` struktural.
- Lexer: keywords reserved `i64 bool string void` (+ token tests).
- Parser: `let name[: Type] = expr` (`parseType()` + error `expected
  type`); `LetStmt::declaredType`; dump `(let age: i64 15)`.
- `TypeChecker` (`compiler/semantic/`): anotasi vs inisialisasi
  (`mismatched types ... declared as 'i64' but initializer has type
  'string'`), inferensi tanpa anotasi, operand aritmetika/perbandingan
  `i64`, equality sejenis non-string/void, unary `i64`, `void` ditolak
  untuk variabel & `print`, `main : fn() -> void`. Error dikumpulkan,
  diagnostic konsisten dengan semantic.
- Driver pipeline: lex → parse → semantic → **typecheck** → codegen.
- Contoh `examples/types.ns` → `15/true/Nusa/30`.
- Tests: `test_typecheck` baru (11 cek), lexer +2, parser +4, codegen +1
  (`alloca i64/i1` beranotasi), `e2e_types` baru (nilai + mismatch
  verbatim + inferensi + `1+true` + `void`).

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **9/9 passed**.
- Contoh master verbatim berjalan (`15/true/Nusa`); AArch64 executable;
  IR (`alloca i64/i1/ptr`) terverifikasi.
- Error master verbatim: `let x: i64 = "hello"` → `mismatched types`,
  exit 1, tanpa crash.

### Batasan yang diketahui
- Single `fn main()` flat scope; tanpa assignment terpisah; tanpa
  generics/traits/polymorphism (roadmap 1.x/2.x); parameter & return
  fungsi = v0.5.0.

## [0.3.0] — Variables & Expressions (2026-09-05)

Milestone eksekusi sesuai MASTER PROMPT: `let`, variabel, integer,
boolean, binary/unary expressions, dan symbol table — semuanya menjadi
native executable. Type system penuh = v0.4.0.

### Ditambahkan
- Semantic analysis (`compiler/semantic/`): `SymbolTable` + pemeriksa
  sekuensial single-scope. `print(x)` tanpa deklarasi →
  `error: undefined variable 'x'` + snippet + `help:` (bukan crash,
  sesuai tuntutan MASTER PROMPT). Deklarasi ganda → `duplicate
  variable 'x'` + lokasi deklarasi sebelumnya. `let x = x;` dan
  use-before-def ditolak.
- Codegen variabel/ekspresi: integer→i64 (`add/sub/mul/sdiv` bertanda),
  boolean→i1 (`icmp` bertanda `slt/sgt/sle/sge`, `eq/ne` + equality bool),
  string→ptr (variabel string + `print` variabel string), unary minus
  (`CreateNeg`), `alloca` entry-block + `load`/`store`, `verifyModule`
  tetap wajib.
- `print` segala tipe: string→`puts`, integer→`printf("%lld\n")`,
  boolean→`select "true"/"false"` + `puts`. Mencetak ekspresi langsung
  (`print(a + b)`) juga bisa.
- Validasi bentuk operand minimal di codegen (mismatch → error bersih
  ber-hint v0.4.0; string `==` ditolak eksplisit) — bukan type system,
  hanya penjaga IR valid.
- Contoh `examples/arithmetic.ns` → mencetak `30`, `70`, `true`.
- Tests: `test_semantic` baru (8 cek), `test_codegen` ditulis ulang
  (8 cek bentuk IR: `add/mul/sdiv/sub/icmp/select/printf/puts` +
  penolakan mismatch), `e2e_variables` baru (nilai eksekusi: milestone
  `30`, mixed `70/-5/4/2/false/true/Nusa`, undefined + duplicate).
  `e2e_frontend` diperbarui (expr.ns kini build+run sukses).
- Driver pipeline: lex → parse → **semantic** → codegen → clang.

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **7/7 passed**.
- Program milestone verbatim → executable AArch64 mencetak `30`;
  IR memuat `alloca i64`/`load`/`add i64`/`printf` + `ret i32 0`.
- Error wajib verbatim: `print(x)` → `error: undefined variable 'x'`,
  exit 1, tanpa crash.

### Batasan yang diketahui
- Single flat scope; tanpa assignment terpisah; tanpa anotasi & type
  checking (v0.4.0); `return`/`if`/`while` tetap ditolak ber-hint versi.
- `sdiv` dengan pembagi nol menjebak (trap) saat runtime — ditangani
  diagnostics/runtime di versi hardening.

## [0.2.0] — Lexer & Parser (2026-09-05)

Milestone front-end sesuai MASTER PROMPT. Scope: lexer penuh + parser
dengan operator precedence yang benar. Codegen sengaja tetap subset
v0.1.0 (eksekusi `let`/ekspresi = v0.3.0).

### Ditambahkan
- Lexer penuh: keywords `fn let return if else while true false`,
  identifier, integer literal desimal (i64, cek overflow,
  `123abc` ditolak), string literal + escapes, operator
  `+ - * / = == != < > <= >=`, simbol `( ) { } [ ] : , ;`,
  source location (file/line/column) di setiap token,
  error rapi (`unexpected '!' — did you mean '!='?`, dsb.).
- AST ekspresi (`compiler/ast/`): `LiteralInt/Bool/String`, `VarRef`,
  `Unary`, `Binary`, `LetStmt`, `PrintStmt(<expr>)` + s-expr `dump()`
  untuk test & debugging, mis. `(+ 10 (* 20 3))`.
- Parser precedence-climbing:
  `equality → comparison → term → factor → unary → primary`,
  sehingga `10 + 20 * 3` = `(+ 10 (* 20 3))` (bukan `(10+20)*3`).
  `return`/`if`/`while` ditolak dengan hint versi (v0.5.0/v0.6.0).
  `;` tetap opsional (kompatibel v0.1.0).
- Codegen: subset v0.1.0 tidak berubah; `let` / `print(non-string)`
  ditolak dengan error lokasi + `help: ... coming in v0.3.0`.
  Error codegen kini diformat lengkap (`file:line:col: error:`).
- Contoh `examples/expr.ns` (front-end demo: ter-parse, build ditunda).
- Tests: `test_lexer` (12 cek: keywords, int, operator, simbol, lokasi,
  overflow, `!`, …), `test_parser` (18 cek: precedence, asosiatif,
  parens, unary, bool, error `==`/`return`/`if`/`)`), `test_codegen`
  (subset + 2 penolakan v0.3.0), integration baru `e2e_frontend`
  (parse-OK-lalu-deferral + 3 negative tests).
- Docs: `docs/syntax.md` (spesifikasi syntax saat ini);
  `README`/`getting-started`/`compiler` diperbarui ke v0.2.0.

### Diperbaiki
- `Parser::parse()` kini `return std::move(result_)`: me-return member
  tidak mendapat implicit-move, dan AST move-only (variant + unique_ptr)
  membuat fallback copy gagal compile. Ditemukan via bisect empiris.

### Diverifikasi
- Clean build: 0 warning, 0 error. `ctest`: **5/5 passed**.
- `hello.ns` regression: executable AArch64 mencetak `Hello, Nusa!`,
  IR memuat `define i32 @main()` + `declare i32 @puts(ptr)`.
- `expr.ns`: gagal di codegen dengan pesan v0.3.0 (tanpa kata "expected",
  tanpa crash) — bukti front-end menerima, backend menunda.
- Malformed (`let x = 10 + ;`, `let x == 3;`, `return`) gagal di parser
  dengan diagnostic + snippet, tanpa crash.

### Batasan yang diketahui
- Hanya `fn main()` + `let`/`print`; codegen executable hanya untuk
  `print("string literal")`.
- Tanpa fitur v0.3.0+: symbol table, evaluasi variabel/ekspresi, dsb.

## [0.1.0] — Hello Nusa (2026-09-05)

Milestone pertama sesuai MASTER PROMPT. Scope: compiler minimal yang
benar-benar berjalan di Android AArch64 / Termux + LLVM 21.1.8.

### Ditambahkan
- Project CMake (`cmake -S . -B build`, target `nusa`).
- Lexer: `fn`, identifier, string literal + escapes (`\" \\ \n \t \r \0`),
  simbol `( ) { } ; ,`, source location (file/line/column),
  skip whitespace + komentar `//`, error rapi untuk
  unterminated string / unexpected character.
- Parser recursive-descent + AST (`Program/FunctionDecl/Block/PrintStmt`),
  semicolon opsional, error dengan hint
  (mis. "v0.1.0 only supports 'fn main'").
- Diagnostics: format `file:line:col: error: msg` + snippet + caret + `help:`.
- LLVM backend (LLVM 21.1.8, opaque `ptr`):
  `declare i32 @puts(ptr)`, `define i32 @main`, `verifyModule` wajib
  sebelum emit, output `.ll` di samping source.
- Driver + CLI: `nusa --version`, `nusa --help`,
  `nusa build file.ns [-o out]`, `nusa run file.ns`
  (link via `clang` → native AArch64/Bionic executable).
- Contoh `examples/hello.ns` → mencetak `Hello, Nusa!`.
- Tests: `test_lexer`, `test_parser`, `test_codegen` (ctest) +
  `e2e_hello` (build → run → cek output + IR + negative test malformed).
- Docs: `README.md`, `docs/getting-started.md`, `docs/compiler.md`.

### Diverifikasi
- `uname -m` = aarch64, `llvm-config --version` = 21.1.8,
  `clang --version` = 21.1.8 (target aarch64-unknown-linux-android24).
- `ctest` lulus, `./hello` mencetak `Hello, Nusa!`,
  LLVM IR memuat `define i32 @main()` + `declare i32 @puts(ptr)`.
- Malformed source (`print( )`) gagal dengan diagnostic, tanpa crash.

### Batasan yang diketahui
- (Telah digantikan v0.2.0 untuk lexer/parser; codegen tetap subset ini.)
