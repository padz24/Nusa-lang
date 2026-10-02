# Compiler Nusa — Design & Implementation (aktual v2.0.0)

> Phase 40: dokumen ini = **Compiler Implementation** (pilihan
> implementasi, bukan definisi bahasa). Definisi bahasa normatif:
> `docs/language-spec-2.0.md` (`docs/language-spec-1.0.md`
> diabadikan sebagai riwayat 1.20.0). Catatan berversi v0.x di bawah
> = arkeologi keputusan (perilaku saat ini mengikuti spec 2.0).

Pipeline:

```
main.ns (+math.ns) → Load → Lex → Parser → AST → Semantic → TypeCheck → Codegen (LLVM 21.1.8) → .ll → clang → executable
```

Phase 51 (2.2.0): registry builtin (`isBuiltin`) tinggal di
lapisan Frontend baru (`compiler/frontend/Builtin.{h,cpp}`),
dipakai bersama semantic + codegen. Backend tidak include
`semantic/Semantic.h`/`TypeChecker.h` (dikunci `e2e_arch.sh`).
Peta + utang layer tercatat di `docs/architecture-2.2.md`
(Pkg→Driver, `sha256Hex`, LSP→semantic, `Scopes.h` = follow-up).

Phase 52 (2.3.0): fondasi IR internal di `compiler/mir/Mir.{h,cpp}`
(`nusa::mir`: typed values, instruksi, basic blocks, builder,
verifier, dump teks) — murni C++ standar, pipeline belum di-rewire.
Desain: `docs/ir-2.3.md`; conformance: `e2e_mir.sh`.

Phase 53 (2.4.0): pass optimisasi di `compiler/mir/Opt.{h,cpp}`
(ConstFold + DCE + fixpoint manager; aturan aman di `docs/opt-2.4.md`,
benchmark `bench/mir_opt_main.cpp` via `e2e_opt.sh`). LLVM tetap
optimizer utama; pipeline `.ns` belum di-rewire.

Phase 55 (2.6.0): runtime modular di `runtime/rt_*.c` + `rt_platform.h`
(`nusa_rt.c` = agregator satu TU; driver tak berubah). Desain:
`docs/runtime-2.6.md`; conformance: `e2e_runtime.sh`.

Phase 56 (2.7.0): `chan_len` (observability antre) + stress suite
`e2e_stress.sh`; desain: `docs/concurrency-2.7.md`.

Phase 57 (2.8.0): fondasi async sebagai library (`std/tasks.ns`:
Future + cancellation kooperatif; tanpa sintaks baru) +
`e2e_async.sh`; desain: `docs/async-2.8.md`. Sampingan: formatter
kini mempertahankan komentar dokumentasi `///` (regresi dikunci
`testDocCommentsKept`).

Phase 58 (2.9.0): HTTP/1.x primitives murni Nusa (`std/http.ns`
di atas TCP loopback; tanpa builtin/compiler baru) +
`e2e_http.sh`; desain: `docs/net-2.9.md`.

Phase 59 (2.10.0): model keamanan paket — `[capabilities]`
manifest + enforcement `native` di `pkg build` (pindai AST) +
traversal guard install; desain: `docs/security-2.10.md`;
conformance: `e2e_sec.sh`.

Phase 60 (2.11.0): inspeksi ekosistem read-only — `tree`,
`verify`, `outdated`, `audit` (top-level + `pkg`); desain:
`docs/ecosystem-2.11.md`; conformance: `e2e_eco2.sh`.

Phase 61 (2.12.0): reproducible builds — tarball publish
byte-deterministik + pin compiler `[meta]` di lock; jaminan:
`docs/repro-2.12.md`; conformance: `e2e_repro.sh`.

Phase 62 (2.13.0): build system — `nusa clean` + `--profile`
dev/debug/release (build + pkg build; eksplisit menang);
desain: `docs/build-2.13.md`; conformance: `e2e_buildsys.sh`.

Phase 63 (2.14.0): LSP `textDocument/references` (himpunan ≡
rename; `includeDeclaration` dihormati); harness `e2e_lsp.py`.

Phase 64 (2.15.0): DWARF composite (struct/array) + formal
params; desain: `docs/debugging-2.15.md`; conformance: kasus
baru `e2e_debug.sh`.

Phase 65 (2.16.0): observability — `nusa bench` (N run +
maxRSS) + `build --stats` (rincian compile); desain:
`docs/perf-2.16.md`; conformance: `e2e_prof.sh`.

Phase 66 (2.17.0): `nusa bindgen` (header C subset → `extern
fn`; tanpa perubahan checker/codegen); desain:
`docs/ffi-bindgen-2.17.md`; conformance: `e2e_bindgen.sh`.

Phase 67 (2.18.0): target `wasm32` bare-metal compile-only
(subset portabel; guard link/run existing); desain:
`docs/wasm-2.18.md`; conformance: kasus baru `e2e_cross.sh`.

Phase 68 (2.19.0): link minimal — runtime dilewati bila tak
dirujuk + gc-sections; desain: `docs/min-2.19.md`;
conformance: `e2e_min.sh`.

Phase 69 (2.20.0): macro system — evaluasi (belum dibutuhkan)
+ reservasi keyword `macro`; desain: `docs/macro-2.20.md`.

Phase 70 (2.21.0): compile-time `const` (eval + substitusi
desugar di checker; RFC-0009 Proposed); desain:
`docs/comptime-2.21.md`; conformance: `e2e_const.sh`.

Phase 71 (2.22.0): fondasi verifikasi — invarian per tahap
(RFC-0010 Proposed); desain: `docs/verification-2.22.md`;
unit `test_verify` (5 properti) + `fuzz_gen.py` berseed +
conformance: `e2e_verify.sh`.

Phase 72 (2.23.0): self-host parser+AST — `selfhost/parse.ns`
fused lexer+parser, oracle `nusa ast`, diferensial 43 file;
desain: `docs/selfhost-2.23.md`; conformance: `e2e_parse.sh`.

Phase 73 (2.24.0): bootstrap 2.0 — rantai multi-komponen
(lex + parse via `--as lex|ast`, artefak `<base>_stage1`);
desain: `docs/bootstrap-2.24.md`; conformance: `e2e_bootstrap.sh`.

Phase 74 (2.25.0): bootstrap full 4-stage — Stage 3 rebuild
(`--rebuild`, D3 == D2) + dokumentasi proses lengkap;
desain: `docs/bootstrap-full-2.25.md`;
conformance: `e2e_bootstrap.sh` (10 kasus).

Phase 75 (2.26.0): stdlib 3.0 — modul baru core/testing/
logging/encoding + string/io/fs; konvensi konsistensi;
desain: `docs/stdlib-2.26.md`;
conformance: `e2e_stdlib3.sh` + `nusa test`.

Phase 76 (2.27.0): cryptography foundation — builtin
`sha256_hex`/`crypto_rand_hex` (RFC-0011 Proposed) +
`std/crypto.ns`; desain: `docs/crypto-2.27.md`;
conformance: `e2e_crypto.sh`.

Phase 77 (2.28.0): data tooling — `std/csv+json+toml` +
paket referensi `packages/sqlite/` (pola shim FFI);
desain: `docs/data-2.28.md`;
conformance: `e2e_data.sh` + `nusa test`.

Phase 78 (2.29.0): server foundation — `std/config+metrics` +
paket referensi `packages/tls/` (shim OpenSSL terverifikasi);
desain: `docs/server-2.29.md`;
conformance: `e2e_server.sh` + `nusa test`.

Phase 79 (2.30.0): distributed foundation — `std/rpc+discovery+
timeout+retry` (concurrency stabil 13/13); desain:
`docs/dist-2.30.md`; conformance: `e2e_dist.sh` + `nusa test`.

Phase 80 (2.30.1): STABILITY RELEASE — freeze fitur bahasa;
harness hermetik, e2e_compat, fuzz seed-2 (temuan generator),
baseline performa, audit keamanan; desain:
`docs/stability-2.30.md`; conformance: suite penuh hijau.

Phase 81 (3.0-candidate): LANGUAGE FREEZE — 6 area dikunci,
NOL perubahan bahasa; kandidat: `docs/language-spec-3.0-
candidate.md`; desain: `docs/freeze-3.0.md`; conformance:
`e2e_freeze.sh` + spec2 + compat.

Phase 82 (3.0-final): SPECIFICATION — `docs/language-spec-3.0.md`
normatif (19 topik, independen implementasi); NOL perubahan
bahasa (ratifikasi); conformance: `e2e_spec3.sh`.

Phase 83 (3.0-audit): COMPATIBILITY AUDIT — korpus 76/3/0/0/2;
CLI `nusa migrate [PATH]` (Driver::runMigrate, read-only);
guide: `docs/migration-3.0.md`; conformance: `e2e_migrate.sh`.

Phase 84 (3.0.0-beta): BETA RELEASE — freeze 4 area (semantik,
arsitektur, API stdlib, format paket); NOL perubahan bahasa;
label `3.0.0-beta` (deb tetap `3.0.0-1`); record:
`docs/release-3.0-beta.md`; gate: `e2e_beta.sh`.

Phase 85 (3.0-realworld): REAL-WORLD VALIDATION — 6 proyek
`realworld/` + `e2e_realworld.sh`; bugfix F4 `file_exists`
satu-baris + regresi; temuan F1–F6 di `docs/realworld-3.0.md`.

Phase 86 (3.0-perf): PERFORMANCE VALIDATION — suite resmi
`bench/suite.sh` (8 dimensi, 35 metrik) + `docs/perf-3.0.md`;
gate: `e2e_perf.sh`; NOL klaim tanpa angka.

Phase 87 (3.0-security): SECURITY AUDIT — 9 area × 6 kelas:
0 critical/high; fix S1 (`--memory-mb 0` fail-open → tolak)
+ regresi; record: `docs/security-3.0.md`; `e2e_sec` 5→9.

Phase 88 (3.0.0-rc.1): RELEASE CANDIDATE — label `3.0.0-rc.1`
(deb tetap `3.0.0-1`); NOL perubahan bahasa/arsitektur/API/format
(beku sejak beta); record: `docs/release-3.0-rc.1.md`;
gate: `e2e_rc.sh` (`e2e_beta` dipertahankan sebagai regresi).

Phase 89 (3.0.0): FINAL RELEASE — hapus suffix (`3.0.0`,
help `Stable`); NOL perubahan bahasa/arsitektur/API/format;
record: `docs/release-3.0.md`; gate: `e2e_release.sh`
(`e2e_beta`/`e2e_rc` dipertahankan sebagai regresi).

v0.9.0 adalah milestone **standard library & tooling**: 11 builtin
runtime (emisi libc langsung), modul `std/` murni-Nusa, dan CLI
`check`/`test`/`fmt`/`doc`.

v0.9.5 adalah **hardening** tanpa fitur besar: guard rekursi (parser +
3 walker, batas 500), div-by-zero statis + runtime, malloc/ftell guards,
import-chain cap, matriks 9 negative + korpus malformed. Detail di
§Hardening di bawah.

v0.7.0 menambahkan **array**: literal homogen, tipe `[T]`, indexing
dengan runtime bounds check, dan builtin `len()` — dengan alloca
statis `[N x T]` dan copy agregat. Tanpa generic collections.

## Lexer (`compiler/lexer/`)

- Hand-written, sekali jalan, melacak `line/column` di setiap token.
- Keywords: `fn let return if else while true false` + keywords tipe
  `i64 bool string void` (reserved, v0.4.0; pemetaan di
  `lexIdentOrKeyword`) + `struct import` (v0.8.0) + `type` (v1.2.0) +
  `trait impl for self` (v1.3.0) + `match` (v1.6.0) + `enum` (v1.7.0) +
  `extern` (v1.13.0) + `async await` (reserved v2.0 untuk RFC-0004;
  tanpa sintaks) + `macro` (reserved v2.20.0, Phase 69; tanpa
  sintaks). `main`/`print` tetap identifier biasa yang
  dimaknai di parser — disengaja agar error-nya jelas.
- `IntLit`: digit desimal dengan cek overflow i64 manual
  (`value > (INT64_MAX - digit)/10` → error). `123abc` menempel
  langsung → `invalid integer literal`. Tanda minus TIDAK di-lex
  sebagai bagian angka (ditangani parser sebagai unary op).
- Operator dua karakter: `==`, `!=`, `<=`, `>=`, `->` (greedy),
  `=>` (match arm), `|` (lambda). `!` sendirian → `unexpected
  character '!' (did you mean '!='?)`; `?` postfix (propagasi
  `Result`, sejak v1.5.0).
- Simbol: `( ) { } [ ] : , ;` (`[` `]` `:` dicadangkan untuk
  array/type annotation v0.4.0/v0.7.0).
- String: `"..."` dengan escapes `\" \\ \n \t \r \0`; newline mentah
  atau EOF sebelum quote penutup → `unterminated string literal`.
- Melewati whitespace + `//` komentar (`/` vs `//` dibedakan via
  lookahead sebelum lexing `Slash`).

## Parser + AST (`compiler/parser/`, `compiler/ast/`)

Grammar v0.2.0 (foto historis — grammar berlaku: `docs/syntax.md`):

```
Program    := FuncDecl+
FuncDecl   := "fn" Ident "(" [Param ("," Param)*] ")" ["->" Type] Block
Param      := Ident ":" Type
Block      := "{" Stmt* "}"
Stmt       := LetStmt | PrintStmt | ReturnStmt | ExprStmt
LetStmt    := "let" Ident [":" Type] "=" Expr [";"]
PrintStmt  := "print" "(" Expr ")" [";"]
ReturnStmt := "return" [Expr] [";"]
ExprStmt   := Call [";"]
Type       := "i64" | "bool" | "string" | "void"
Expr      := equality
equality   := comparison (("==" | "!=") comparison)*
comparison := term (("<" | ">" | "<=" | ">=") term)*
term       := factor (("+" | "-") factor)*
factor     := unary (("*" | "/") unary)*
unary      := ("-" | "+") unary | primary
primary    := IntLit | StringLit | "true" | "false" | Call | Ident
            | "(" Expr ")"
Call       := Ident "(" [Expr ("," Expr)*] ")"
```

- Precedence climbing memastikan `10 + 20 * 3` → `(+ 10 (* 20 3))`.
- AST memakai `std::variant` + `unique_ptr` (move-only). Konsekuensi:
  `Parser::parse()` harus `return std::move(result_)` — me-return
  *member* tidak mendapat implicit-move (hanya local yang dapat),
  sehingga fallback copy gagal compile. Pelajaran v0.2.0, didokumentasikan
  di CHANGELOG.
- `AST::dump()` menghasilkan s-expr (`(let x (+ 10 (* 20 3)))`) untuk
  test & debugging.
- Keyword tahap lanjut ditolak dengan hint versi: `if`/`while` → v0.6.0.
  Error recovery `synchronize()` (lompat ke `;`/`}`).
- `;` tetap opsional (kompatibel v0.1.0).
- Lookahead satu token (`Ident (`) membedakan call vs variable.
- `->` greedy di lexer (`Minus`+`Gt` berdampingan tak pernah valid).

## Diagnostics (`compiler/diagnostics/`)

- Format = contoh master: `file:line:col: error: msg`, baris kosong,
  snippet (indent 4), caret **beruntun** selebar span, baris kosong,
  `help:` opsional. Span (`length`, default 1, dijepit ke sisa baris)
  diisi di situs identifier-presisi (variabel/fungsi/struct/field/call).
- Lexer errors sudah berformat penuh (verbatim); parser errors via engine
  (`sourceLines` untuk snippet). `SemanticError`/`ParseError`/
  `ModuleError` membawa `length` menembus driver multi-file.

## Semantic (`compiler/semantic/`)

- `SymbolTable`: `unordered_map` nama → `{declLoc}`; `declare()` menolak
  ganda beserta lokasi deklarasi sebelumnya untuk hint.
- Two namespaces: fungsi (top-level, two-pass agar forward reference bisa)
  dan nilai (local scope baru per fungsi: parameter + `let`).
- `main` wajib tepat satu & tak callable; reserved
  `print`/`len`/`puts`/`printf`/`exit`.
- `undefined variable 'x'` / `undefined function 'foo'` + help —
  bukan crash. Semua error dikumpulkan.
- v0.8.0: namespace struct + tabel definisi; literal dicek
  (unknown/duplikat/missing field + privacy `_` antar-file);
  field-access diteruskan ke checker (butuh tipe).

## Types (`compiler/types/`)

- `Type`: value type (copyable) — `I64 | Bool | String | Void | Func |
  Array | Struct | Param | Error`. Komponen (`params`/`ret`/`elem`) via
  `shared_ptr`; `Struct` via nama (`sname`); `Param` = parameter tipe
  generik `T` (rigid selama inferensi; tak pernah mencapai codegen —
  instance selalu konkret, template dibuang checker); `Error` untuk
  pemulihan tanpa error beruntun. Ukuran array BUKAN bagian tipe.
  Generik = monomorphisasi di checker (lihat `docs/types.md`).
- `name()`: `"i64"` / `"fn() -> void"` / …; `operator==` struktural.
- `LetStmt::declaredType` (`std::optional<Type>`); dump menampilkan
  `(let age: i64 15)` bila beranotasi.

## Type checking (`compiler/semantic/TypeChecker`)

- Tahap sendiri setelah semantic (pipeline master): anotasi vs
  inisialisasi harus sama (`let x: i64 = "hello"` → `mismatched types`);
  tanpa anotasi → inferensi dari inisialisasi untuk pemakaian berikut.
- Aturan ekspresi: aritmetika/perbandingan butuh `i64`; equality butuh
  sisi sama (non-string, non-void); unary butuh `i64`; `print(void)`
  dan variabel `void` ditolak. Operand `Error` menekan error beruntun.
- v0.5.0: signatures two-pass (rekursi OK); arity + tipe argumen;
  return cocok dengan return type; missing/bare return; unreachable
  sesudah `return`; param `void` ditolak; `main : fn() -> void` ditegaskan.
- v0.7.0: literal homogen (non-void non-array sejenis; `[]` ditolak);
  index butuh base array + idx `i64`; `len()` 1 arg array → i64;
  print/`==`/param/return array ditolak; `[void]` ditolak.
- v0.8.0: tabel struct (dup field, resolve tipe incl. unknown, tolak
  field void/array); literal vs deklarasi (tipe per field); akses field
  → tipe field (unknown → daftar field sebagai hint); tolak field
  bertipe void di `let x = voidFn()` (bug laten v0.5.0, ditutup di sini);
  tolak print/`==` struct (param/return struct by-value didukung
  sejak v1.2.0, Phase 21d).
- Error memakai `SemanticError` (snippet/caret konsisten); semua
  dikumpulkan, bukan berhenti di pertama.

## Codegen (`compiler/codegen/`, LLVM 21.1.8)

- API modern (`LLVMContext`, `Module`, `IRBuilder`, opaque `ptr`),
  **wajib** `llvm::verifyModule` sebelum emit.
- Representasi: integer→`i64` (literal via `ConstantInt`), boolean→`i1`,
  string→`ptr` (global private constant + GEP).
- Variabel: `alloca` di entry block + `store` saat `let`, `load` saat
  dipakai; peta nama → `{slot, kind}` di codegen (terpisah dari
  symbol table semantic — pemisahan front-end/back-end).
- Aritmetika bertanda: `add/sub/mul/sdiv`; perbandingan bertanda
  `icmp slt/sgt/sle/sge`; equality `eq/ne` (int–int, bool–bool);
  unary minus `CreateNeg`. LLVM constant-folding melipat ekspresi
  literal (wajar; test memakai variabel agar instruksi terlihat).
- `print`: string→`puts`, integer→`printf("%lld\n")` (deklarasi vararg
  `i32 (ptr, ...)`), boolean→`select` antara global `"true"`/`"false"`
  + `puts`. Format string dibuat lazily sekali per module.
- Validasi ringan di setiap lookup/call (arity, tipe argumen LLVM) —
  penjaga IR valid, BUKAN type checker (itu ranah TypeChecker).
  Pertahanan anti-crash: tahap sebelumnya seharusnya sudah menolak
  lebih dulu dengan diagnostic lebih baik.
- v0.5.0: two-pass declare→define; `main` → `i32 @main` return 0;
  parameter via alloca+store; `return` → `ret`/`ret void`;
  call → LLVM `call` dengan kind dari signature.
- v0.7.0: array = alloca `[N x T]` entry (nilai = pointer, tanpa load);
  indexing = GEP + bounds check (`oob.fail` → puts + `exit(1)`); copy
  agregat load/store; `len()` = konstanta; `void @exit(i32)` dideklarasikan.
- v0.8.0: struct = named LLVM type (`%Person = type { ptr, i64 }`,
  two-phase agar forward-ref); nilai struct = pointer (tanpa load);
  konstruksi = alloca + store per field (urutan deklarasi; nested via
  copy agregat); akses = StructGEP + load (skalar) / pointer (nested);
  `let`/`assign` struct via `storeGenValue` terpadu.
- v0.6.0: `genBlock` (scope + pelacakan terminator); `genIf` =
  CondBr ke then/else(/merge) + merge; `genWhile` = br cond →
  CondBr body/after → br cond; `return` di cabang → `ret` di block itu;
  merge non-void yang tak terjangkau → `unreachable` (bukan IR invalid);
  SEMUA alloca (termasuk block dalam) di entry — tidak ada pertumbuhan
  stack per iterasi + dominansi untuk semua use.
- Emit `.ll` tekstual, lalu Driver memanggil `clang` untuk object + link
  native (AArch64 Android/Bionic ABI).

## Driver (`compiler/driver/`)

- `Driver::runBuild`: entry `.ns` → **ModuleLoader** (load+lex+parse
  transitif) → semantic → typecheck → codegen → tulis `<input>.ns.ll`
  → `popen("clang ...")` → `<output>`.
- `Driver::runRun`: build ke `<input>.run.out` temporer → `system()` →
  hapus file temporer (dengan prefix `./` untuk path relatif +
  `WEXITSTATUS`, pelajaran v0.1.0).
- `main.cpp`: parsing argumen tanpa library eksternal
  (`--version/--help/build/run/check/test/fmt/doc`, `lsp`, `lex`,
  `bootstrap`, `sandbox`, `playground`, `new`, `pkg`/`search`/
  `install`/`publish`, `-o`, `--check`, dan flag backend `-O*`/
  `--target`/`--emit-ir`/`--emit-asm`/`-c`/`-g`/`--fresh`/link).
- `checkStages` bersama untuk `check`/`build`/`test`; `check` berhenti
  setelah typecheck (`Checked X: no errors`).

## Builtins runtime (`isBuiltin`; 55 nama, tumbuh per fase)

- Daftar tunggal di `compiler/frontend/Builtin.cpp` (sejak 2.2.0):
  `len` (v0.7.0) + 12 klasik
  v0.9.0 (`input`, `assert`, `str_len`, `str_concat`, `parse_int`,
  `file_read`, `file_write`, `file_exists`, `array_sum/min/max`) +
  `str_eq` (v1.6.0, match string) + `parse_int_valid` (v1.5.0) +
  concurrency v1.9.0 (`thread_spawn/join`, `mutex_new/lock/unlock/
  try_lock/free`, `chan_new/send/recv/free`, `atomic_new/get/set/
  add/free`, `sleep_ms`) + channel bertag 2.0 (`chan_new_bool/
  send_bool/recv_bool`, `chan_new_str/send_str/recv_str`) + v1.10.0
  (`str_char_at/slice`, `time_now_ms/s`, `random_u64`,
  `process_exit/env/cwd/argc/argv`, `tcp_connect/send/recv/close`) +
  `int_to_str` (v1.19.0) + `tcp_listen/accept` (2.0) +
  `chan_len` (v2.7.0, observability antre). Reserved +
  exempt di semantic; arity/tipe di `checkBuiltin`; emisi di
  `genBuiltinCall` (libc langsung:
  `getline`, `strlen`, `malloc`/`memcpy`, `strtoll`, `fopen`/`fread`/
  `fwrite`/`fseek`/`ftell`/`rewind`/`fclose`, loop inline,
  pthread/channel/atomic/TCP di `runtime/nusa_rt.c`).
- Loop `array_*` inline di call-site (panjang statis dikenal) — tanpa
  masalah ABI; `len()` tetap konstanta. String heap bocor sengaja
  (program-lifetime; GC = RFC-0007 Proposed).

## Tooling (`compiler/fmt`, `compiler/doc`, `Testing.cpp`, baru v0.9.0)

- `fmt`: normalisasi token-stream (4 spasi, aturan terdokumentasi di
  header); unary `-/+` via konteks token-sebelumnya; string di-escape
  ulang; komentar `//`/`///` dipertahankan (own-line/trailing);
  deterministik + idempoten (diuji); `--check` untuk CI.
  Phase 50 (2.1.0): lambda sesudah `(`/`[` menempel tanpa spasi
  (`apply(|x| …)`, bukan `apply( |x| …)`); regresi dikunci
  `testLambdaAfterParen` + `e2e_stability.sh`.
- `doc`: signature dari AST + blok `///` menempel (+ intro modul bila
  dipisah baris kosong); Markdown ke stdout/`-o`.
- `test`: kumpulkan `*_test.ns` (file/dir, default `.`); build + jalankan
  tiap file via fork+alarm 10 dtk (anti-gantung); stdout/stderr/stdin
  anak dialihkan (deterministik); PASS = exit 0; ringkasan + rc.
  Pelajaran: `fflush` sebelum `fork` (buffer ganda).
- Komentar lexer: `Lexer::comments()` (`line/text/doc`); parser tak
  tersentuh (komentar bukan token).

## Module loader (`compiler/driver/Modules`, baru v0.8.0)

- `import math;` → kandidat berprioritas (v0.9.0): direktori pengimpor →
  `$NUSA_PATH` (`:`-separated) → `<exe>/../std` (via `/proc/self/exe`).
  Lokal menang (bisa override std); `import string;` valid.
- DFS rekursif + visited (dedupe diam-diam) + stack rantai untuk
  `circular import detected`.
- Identitas absolut-ternormalisasi (`std::filesystem`, `error_code`
  karena build `-fno-exceptions`); tampil relatif-bila-bisa.
- Tiap file di-lex/parse dengan identitasnya → diagnostic tepat file
  (engine per file di driver); error modul-tengah tak menghentikan
  modul lain (semua error sekaligus).
- Merge deps-dulu (urutan tak memengaruhi hasil — semua tahap two-pass);
  `imports` dikonsumsi (program gabungan bersih).
- v0.9.5: rantai > 100 modul ditolak bersih (`import chain too deep`).

## Hardening (baru v0.9.5)

Aturan main: compiler tak boleh crash (sinyal) atau hang pada input
apapun; kegagalan selalu exit != 0 dengan diagnostic (`e2e_hardening`).

- Rekursi dibatasi 500 di 4 titik (naksir stack overflow): `parseExpr`,
  `parseUnary`, `parseBlock`, `parseType` (RAII `DepthGuard` parser) +
  `checkExpr` ×3 (semantic/checker/codegen, inc/dec manual — body
  straight-line terverifikasi). Rantai biner iteratif parser lolos guard
  parser tetapi tertangkap guard walker. AST ≤ 500 → hilir aman.
- Division-by-zero: literal → compile error; dinamis → block `div.zero`
  (pesan + exit 1). Sebelumnya diam-diam garbage (LLVM fold ke undef).
- `malloc` null → `oom.fail` (pesan + exit 1) di `str_concat`/`file_read`;
  `ftell` negatif → jalur kosong + `fclose` (tanpa fd leak).
- `verifyModule` wajib sebelum emit (backstop IR invalid).
- Korpus: 12 malformed (NUL/0xFF/terpotong/kosong/dalam) + CLI edges.
- Performa: 4000 statement build & run benar (waktu tercatat di e2e).
## Tests (`compiler/tests/`; hitungan di bawah = foto era v1.0.0–v1.1.0,
suite kini 50 test via `ctest` — lihat CHANGELOG per fase)

- `test_lexer` (16 cek): + `struct`/`import`/`.`.
- `test_parser` (49 cek): + struct decl/lit/field/import dumps,
  field-chain/index-mix, error field/brace/dot; regresi ambiguitas
  `if c {`; `testBadTypeFails` → `testNamedTypeParses` (tipe bernama
  valid di grammar).
- `test_typecheck` (54 cek): + field cocok/salah/unknown, akses di
  skalar, unknown type, print/`==`/param struct, nested struct.
- `test_codegen` (19 cek): + `%Person` type/alloca, nested struct IR.
- `test_semantic` (31 cek): + struct OK/undefined/duplikat,
  unknown/missing/duplikat field.
- `test_builtins` (baru, 3 grup): typing valid + 10 arity/tipe + reserved.
- `test_fmt` (baru, 7 cek): golden basic/control/struct, komentar,
  unary, escape, idempotensi, lex-error.
- `test_doc` (baru, 4 cek): fn/struct/intro/error.
- `e2e_hello.sh`: `nusa build` → `./hello` == `Hello, Nusa!`,
  `nusa run` sama, cek `.ll`, negative test malformed gagal rapi.
- `e2e_frontend.sh`: `expr.ns` build+run sukses; `let x = 10 + ;` →
  `expected expression`; `let x == 3` → `expected '='`;
  `return 1;` di main → error void-function (typecheck).
- `e2e_variables.sh` (baru): NILAI eksekusi — milestone `30`,
  `arithmetic.ns` (`30/70/true`), mixed
  (`70/-5/4/2/false/true/Nusa`), undefined + duplicate gagal rapi.
- `e2e_types.sh` (baru, v0.4.0): `types.ns` (`15/true/Nusa/30`),
  `let x: i64 = "hello"` → `mismatched types`, inferensi regression,
  `1 + true` dan `void` ditolak.
- `e2e_functions.sh` (baru, v0.5.0, 9 kasus): milestone `30`, multi
  (`10/false/true/Nusa`), forward `3`, build-only rekursi, arity,
  tipe argumen, undefined function, missing return.
- `e2e_control.sh` (baru, v0.6.0, 9 kasus + timeout): milestone
  (`10/3/2/1` + cek label IR), else, nested-if/else-if (`3/2/1`),
  sum 1..10 (`55`), nested loops (`11..33`), while-false, assignment
  mismatch/undefined, kondisi non-bool.
- `e2e_arrays.sh` (v0.7.0, 11 kasus): milestone, string regression,
  dinamis, copy, OOB ±, 6 penolakan statis.
- `e2e_struct_module.sh` (baru, v0.8.0, 10 kasus): person (`Nusa/1` +
  cek `%Person`), modul (`6/Ada`), nested (`7/1`), lintas-modul struct,
  privacy, circular, import hilang, dup struct, tipe field, param struct.
- `e2e_stdlib.sh` (baru, v0.9.0, 5 kasus): builtins runtime, input via
  pipe, file roundtrip, modul std via exe-resolve, assert-gagal.
- `e2e_tooling.sh` (baru, v0.9.0, 4 grup): check ±, test
  (pass/fail/timeout/build-error/kosong), fmt (format/idempoten/--check),
  doc (stdout/-o/error).
- `e2e_hardening.sh` (baru, v0.9.5, 30+ kasus): matriks 9 negative,
  nesting/korpus/rantai/div-0/CLI-edges, regression, performa.
- `e2e_final.sh` (baru, v1.0.0): program uji verbatim (build+run+output
  eksak + IR + AArch64), format diagnostics = contoh master, 8/8 CLI.
- `e2e_diagnostics.sh` (baru, v1.1.0, 5 kasus): recovery multi-error
  top-level, error cap file sampah, hint Inggris, guard sumber
  (tanpa literal `contoh:`).

## Catatan v1.1.0 (diagnostics & recovery, tanpa perubahan bahasa)

1. Recovery parser: `parse()` melanjutkan ke item berikutnya via
   `synchronizeTopLevel()` (batas `import`/`struct`/`fn`/Eof) dengan
   pengaman progres; `parseBlock` sudah recovery per-statement.
2. Error cap: `Lexer::kMaxErrors` = `Parser::kMaxErrors` = 25; error
   ke-26 adalah ringkasan "too many errors, stopping", sisanya dibuang.
3. Token `Unknown` dibuang di konstruktor parser (sudah dilaporkan
   lexer) — tanpa error duplikat.
4. Diagnostics user-facing Inggris penuh; format `file:line:col:`,
   `help:`, snippet/caret/span tak berubah.
5. Formatter: `[` literal array memakai spasi (`= [`, `: [`, `, [`,
   `return [`), `[` indexing menempel.
6. Baseline performa (`bench/bench.sh`): hello ~1.2–1.7 dtk / 5704 B,
   nusa10 ~1.3 dtk / 6104 B, perf2000 ~7.3 dtk / 45712 B, compiler
   449640 B, suite 21 test 120.71 dtk.

## Keputusan yang disengaja (dan kenapa)

1. Semantic sebagai tahap sendiri (bukan terlipat di codegen):
   arsitektur MASTER PROMPT (`semantic/`) + diagnostic lebih baik
   (snippet/caret) + codegen berasumsi input valid.
2. Assignment (`x = ...;`) ditambahkan di v0.6.0 — klausul "jika
   diperlukan" v0.3.0 terpicu oleh loop (tanpa mutasi, `while` tak bisa
   progres). Menimpa slot terdekat (shadowing-aware); tipe harus sama.
3. Shadowing antar-scope diizinkan (seperti Rust); duplikat se-scope
   tetap error. Seragam di semantic/checker/codegen via `Scopes<T>`.
4. Import = merge nama publik (bukan `::`): master hanya menunjukkan
   `import math;` tanpa sintaks kualifikasi; merge + deteksi konflik
   adalah interpretasi minimal yang koheren. `::` bila dibutuhkan (1.x).
5. Ambiguitas `if c {` vs literal dipecahkan lookahead 2-token
   (`}`/`field :`), bukan backtracking — tanpa mengubah grammar.
6. Variabel string didukung walau tipe `string` resmi v0.4.0: hampir gratis
   (ptr) dan membuat `let s = "Nusa"; print(s);` konsisten.
7. `sdiv` bertanda + trap pembagi-nol didokumentasikan sebagai batasan,
   bukan di-crash-kan secara artifisial.
8. Tanpa gtest: Termux tidak selalu punya gtest; CHECK-macro + ctest cukup
   (pelajaran v0.1.0: JANGAN pakai `assert()` untuk call berefek —
   no-op di Release).
