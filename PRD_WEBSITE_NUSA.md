# PRD — Website Dokumentasi Resmi Bahasa Pemrograman Nusa

| Field | Isi |
|---|---|
| Nama produk | Nusa Docs — Website Dokumentasi Resmi Bahasa Nusa |
| Versi PRD | 1.0.0 |
| Tanggal | 2026-09-10 |
| Status | Proposed (siap review) |
| Bahasa target | Nusa v2.18.0 (LLVM 21.1.8, AArch64 Android/Termux, file `.ns`) |
| Penulis | Product Manager / Software Architect / Technical Writer |
| Sumber konteks | `README.md`, `CHANGELOG.md` (v0.1.0 → v2.18.0), `docs/` (49 file), `tutorials/01–08`, `examples/` (18 file), `std/` (18 modul), `rfcs/` (RFC-0001–0008 + TEMPLATE), `compiler/` (16 subsistem), `nusa --help` (CLI v2.18.0) |
| Prinsip | Modern, developer-focused, offline-first, cepat, mudah dipelajari, siap dataset AI |

> Ruang lingkup dokumen ini: **PRD untuk membangun website dokumentasi Nusa**.
> Dokumen ini BUKAN isi dokumentasi bahasa Nusa itu sendiri.

---

## Daftar Isi

1. [Product Overview](#1-product-overview)
2. [Target User](#2-target-user)
3. [Product Goals](#3-product-goals)
4. [Information Architecture](#4-information-architecture)
5. [Core Features](#5-core-features)
6. [Search System](#6-search-system)
7. [Code Documentation System](#7-code-documentation-system)
8. [Interactive Playground](#8-interactive-playground)
9. [AI Documentation Support](#9-ai-documentation-support)
10. [UI/UX Design](#10-uiux-design)
11. [Technical Specification](#11-technical-specification)
12. [Version Roadmap](#12-version-roadmap)
13. [Database / File Structure](#13-database--file-structure)
14. [Security Requirement](#14-security-requirement)
15. [Performance Requirement](#15-performance-requirement)
16. [Definition of Done](#16-definition-of-done)
17. [Appendix A — Requirement Traceability Matrix](#appendix-a--requirement-traceability-matrix)
18. [Appendix B — User Stories & Acceptance Criteria](#appendix-b--user-stories--acceptance-criteria)
19. [Appendix C — Technical Notes](#appendix-c--technical-notes)

Konvensi requirement: setiap kebutuhan diberi ID `REQ-XXX`. Setiap fitur inti
wajib memiliki User Story + Acceptance Criteria + Technical Notes.

---

# 1. Product Overview

## 1.1 Nama produk

**Nusa Docs** — website dokumentasi resmi bahasa pemrograman Nusa
(`docs.nusa` / `nusa-lang.org/docs`, hosting statis + serve lokal via
`python3 -m http.server`).

## 1.2 Visi produk

Menjadi **satu-satunya sumber kebenaran (single source of truth)** untuk
bahasa Nusa: manusia bisa belajar dari nol sampai kontribusi compiler,
mesin/AI bisa mengonsumsi dokumentasi sebagai dataset terstruktur yang
akurat per versi.

## 1.3 Misi produk

1. Menyediakan jalur belajar nol → produktif untuk bahasa compiled
   `.ns` → native executable di atas LLVM 21.1.8.
2. Mendokumentasikan bahasa (syntax, types, stdlib 18 modul), tooling
   (22+ subperintah CLI `nusa`), dan internal compiler (pipeline
   Lexer → Parser → AST → Semantic → TypeChecker → LLVM Backend →
   Native Binary) secara akurat dan teruji.
3. Menyediakan pengalaman developer setara Rust/Go/Swift/LLVM Docs
   tetapi offline-first, ringan (< 1 dtk first load), dan tanpa
   dependensi eksternal — cocok untuk Termux/Android AArch64.
4. Menjadi lapisan data untuk AI assistant (Nusa Helper): retrieval,
   Chat QA, dan dataset export berversi.

## 1.4 Masalah yang ingin diselesaikan

| # | Masalah (fakta repo saat ini) | Dampak jika tidak diatasi |
|---|---|---|
| M-1 | Pengetahuan tersebar di 49 file `docs/`, 8 modul `tutorials/`, 18 `examples/`, 9 RFC, `CHANGELOG.md` raksasa (v0.1.0→v2.18.0), dan `nusa --help`. Tidak ada navigasi tunggal. | Pengguna baru tersesat; onboarding > 1 hari. |
| M-2 | Tidak ada `website/` yang hidup di repo saat PRD ini ditulis (README merujuk `website/` + `website/gen.py` + `website/downloads/` yang tidak ada di working tree). | Link mati; rilis `.deb` (`packaging/deb.sh`) tidak punya etalase unduhan. |
| M-3 | Spec normatif (`docs/language-spec-2.0.md`), spec historis (`spec.md`, `language-spec-1.0.md`), dan 49 doc tambahan belum dipisah per audiens (beginner vs compiler dev). | Advanced/compiler dev sulit menemukan arsitektur, memory model, FFI, target matrix. |
| M-4 | `nusa playground` (HTTP loopback 127.0.0.1) + `nusa sandbox` sudah ada sebagai binary lokal, tetapi belum ada playground browser yang aman untuk website publik. | Calon pengguna tidak bisa mencoba tanpa install Termux+LLVM. |
| M-5 | Tidak ada search (keyword/code/API/error-message), version filtering, maupun dataset export JSON untuk AI. | AI menghalusinasi API Nusa; error message tidak bisa dicari. |

## 1.5 Nilai utama website

1. **Belajar cepat:** path 01–08 (`tutorials/`) menjadi kurikulum web
   dengan contoh runnable + exercise.
2. **Referensi presisi:** setiap halaman bahasa mengikuti format
   Overview → Syntax → Example → Rules → Compiler behavior →
   Limitations → Related topics, ditaut ke file uji pengunci (`Tests:`).
3. **Transparansi compiler:** arsitektur, MIR/Opt (v2.3.0/v2.4.0),
   runtime modular (`rt_*.c`, v2.6.0), DWARF (v1.18.0/v2.15.0),
   incremental (v1.16.0), target matrix (`docs/targets.md` + WASM
   v2.18.0) terdokumentasi jujur termasuk batasan.
4. **Ekosistem hidup:** RFC index, changelog otomatis dari
   `CHANGELOG.md`, downloads `.deb` aarch64, registry file-backed.
5. **AI-ready:** tiap halaman punya JSON terstruktur
   (`title/category/version/content/code_example/keywords`) untuk
   Nusa Helper.

**Requirement ID bagian ini:**

| ID | Requirement |
|---|---|
| REQ-001 | Website WAJIB menjadi pusat informasi resmi tunggal untuk 5 audiens: pengguna baru, developer Nusa, developer compiler, kontributor, AI assistant. |
| REQ-002 | Website WAJIB mencerminkan status bahasa aktual (v2.18.0 saat PRD; spec normatif 2.0 + adendum versi) dan menandai konten historis (v1.x) secara eksplisit. |

---

# 2. Target User

## 2.1 Beginner Developer

**Persona:** Andi, 20 th, mahasiswa Banyumas. Bisa Bash/Python dasar,
punya HP Android + Termux. Belum pernah pakai bahasa compiled/LLVM.

| Aspek | Isi |
|---|---|
| Kebutuhan | Install ≤ 15 menit; program pertama `Hello, Nusa!`; paham `let/fn/if/match`; tahu cara `build/run/check/fmt/test`; contoh yang bisa di-copy-run. |
| Pain point | Prasyarat LLVM 21.1.8 + clang + cmake membingungkan; error `file:line:col + snippet + caret + help:` belum familiar; takut merusak environment Termux. |
| Solusi website | **Learn path** 01–08 sebagai kurikulum web; halaman Getting Started dengan verifikasi env (`uname -m`, `llvm-config --version`); tiap tutorial punya Penjelasan → Source `.ns` → Output → Exercise → Next step; tombol copy + tombol "Coba di Playground"; diagnostics diterjemahkan ke bahasa panduan (taut error-message search). |

## 2.2 Advanced Developer

**Persona:** Sinta, 28 th, backend engineer. Sudah pakai Go/Rust.
Ingin membangun CLI/server (`std/http.ns`, `std/net.ns`), memakai
generics/traits/Option-Result-`?`/closures/concurrency
(thread/channel/atomic), package manager + registry, C FFI + bindgen,
cross-compilation.

| Aspek | Isi |
|---|---|
| Kebutuhan | Language reference presisi; stdlib API per fungsi (param/return/error); contoh pola produksi (par_map2, serve-once, channel bertag); tabel mapping FFI; matriks target; `nusa.toml` + lock + capability. |
| Pain point | Breaking 2.0 (Option/Result struct → enum natif, ctor qualified, `?` hanya Result, `async` reserved) merusak kode lama; channel hanya `i64` + tanpa select/close; FFI terbatas (tanpa struct-by-value/callback/variadik/string-return); registry lokal saja. |
| Solusi website | Halaman referensi per topik dengan Rules + Compiler behavior + Limitations jujur; halaman Migrasi 1.x→2.0; stdlib API dengan signature + error behavior; halaman FFI + bindgen subset + penolakan eksplisit; halaman Targets (host Android AArch64, cross AArch64/x86-64 Linux, `wasm32` compile-only) + sysroot guidance. |

## 2.3 Compiler Developer

**Persona:** Bagas, 32 th, compiler enthusiast. Ingin paham/hack
`compiler/` (lexer/parser/ast/types/semantic/diagnostics/codegen/
driver/fmt/doc/lsp/mir/frontend/incr/pkg/bindgen) dan `runtime/`
(`nusa_rt.c` agregator + `rt_*.c`), MIR/Opt, DWARF, LSP, bootstrap.

| Aspek | Isi |
|---|---|
| Kebutuhan | Pipeline end-to-end; arsitektur lapisan (aturan kopling, mis. Backend nol-include `semantic/`); IR/MIR dump; memory model + audit memsafe; perf (`bench/` + `--stats`); debugging (`-g`, dwarfdump); bootstrap Stage 0→1→2. |
| Pain point | 26.000+ baris C++ multi-lapisan; pengetahuan implisit (mis. formatter `///` vs `//`, `int_to_str` untuk selfhost, fallback runtime); tanpa peta = butuh berminggu-minggu. |
| Solusi website | **Compiler section**: Architecture, Pipeline diagram, Frontend/MIR/Opt/Runtime/Memory-model/LLVM-integration/DWARF/LSP/Incremental/Bootstrap; tiap klaim menunjuk file sumber + e2e test (`e2e_arch/mir/opt/runtime/memsafe/debug/lsp/incr/bootstrap`); batasan jujur (mis. DWARF composite tanpa engine interaktif; LSP satu-file; MIR belum rewire penuh). |

## 2.4 Contributor

**Persona:** Dewi, 25 th, open-source contributor. Ingin kirim PR:
fix docs, tambah stdlib, ikut RFC, publish paket ke registry.

| Aspek | Isi |
|---|---|
| Kebutuhan | Contributing guide; RFC process (Proposed→Accepted→Implemented→Rejected + Superseded); TEMPLATE 8 bagian; cara jalan `cmake/ctest/e2e_*.sh`; konvensi `fmt --check`; registry publish flow; changelog flow. |
| Pain point | Takut melanggar proses (perubahan bahasa besar tanpa RFC ditolak); bingung status RFC-0004/0007/0008 (Proposed, implementasi dilarang sebelum Accepted); lock pra-2.12 basi; capability gate (`native=true`) menggagalkan build. |
| Solusi website | **Contribute hub**: alur PR, RFC index berstatus + filter, checklist Definition of Done, halaman Security/Capability, halaman Reproducible builds, taut langsung ke file sumber di repo. |

**Requirement ID bagian ini:**

| ID | Requirement |
|---|---|
| REQ-003 | Website WAJIB menyediakan 4 jalur persona terpisah (Beginner / Advanced / Compiler Dev / Contributor) dengan entry point dan next-step berbeda. |
| REQ-004 | Setiap halaman tutorial WAJIB memuat Penjelasan, Source `.ns`, Output, Exercise, Next step. |
| REQ-005 | Setiap halaman language reference WAJIB memuat Overview, Syntax, Example, Rules, Compiler behavior, Limitations, Related topics. |

---

# 3. Product Goals

## 3.1 Primary goals (P0 — wajib di v1 website)

| ID | Goal | Metrik penerimaan |
|---|---|---|
| REQ-006 | Onboarding ≤ 15 menit dari landing → `Hello, Nusa!` berjalan (lokal atau playground). | 90% tester baru berhasil tanpa bantuan; Quickstart copy-paste 100% jalan di Termux AArch64 + playground. |
| REQ-007 | Seluruh pengetahuan inti terpeta: Learn (01–08), Language ref (12 topik), Stdlib (18 modul), Compiler (pipeline+runtime), Ecosystem (pkg/registry/template), RFC, Changelog, Downloads. | Cakupan 100% file `docs/` + `tutorials/` + `std/` + `rfcs/` terindeks; nol halaman yatim. |
| REQ-008 | Offline-first + cepat: statis murni, tanpa CDN/JS eksternal/tracking/build-step berat. | Lighthouse > 95; first load < 1 dtk (4G throttled); `python3 -m http.server` langsung jalan. |
| REQ-009 | Akurasi per versi: tiap halaman berlabel versi + status (Stable/Historis/Proposed) + taut ke uji pengunci. | Nol klaim usang (contoh: "tanpa generics" dilarang muncul di halaman 2.x); e2e link-check hijau. |

## 3.2 Secondary goals (P1 — v1.x website)

| ID | Goal | Metrik |
|---|---|---|
| REQ-010 | Search akurat (keyword/code/API/error-message + version filter). Cth: `chan_len` → concurrency doc + stdlib + changelog 2.7.0. | Top-3 relevan untuk 50 query uji; error-message search mencakup 100% pesan `runtime error:` + diagnostics umum. |
| REQ-011 | Playground browser aman (compile/run di sandbox dengan limit). | 99% request valid kembali < 10 dtk; nol escape sandbox; halaman statis tetap berguna saat service mati. |
| REQ-012 | AI dataset export: tiap halaman punya JSON + dump massal berversi untuk Nusa Helper (retrieval + Chat QA). | Export mencakup 100% halaman; skema tervalidasi mesin; notebook Colab dapat rebuild index. |
| REQ-013 | SEO + aksesibilitas (semantic HTML, alt, kontras, keyboard nav, sitemap, meta). | Lighthouse A11y/SEO 100; sitemap + robots + canonical benar. |

## 3.3 Non-goals (eksplisit TIDAK dikerjakan)

| ID | Non-goal + alasan |
|---|---|
| NON-001 | **Bukan IDE online penuh** — tanpa multi-file project editor, debugger interaktif, DAP, atau eksekusi background. Alasan: `nusa playground` loopback sekuensial; tanpa auth; jaringan tak diblokir di sandbox (jujur di docs). |
| NON-002 | **Bukan registry server / package hosting web** — registry tetap file-backed lokal/shared-dir (tanpa HTTP/yank/namespace/owners). Website hanya mengindeks + memandu. |
| NON-003 | **Bukan pengganti `nusa lsp` / compiler** — website tidak mengkompilasi di klien (tanpa WASM build di v1); eksekusi hanya via Playground API → Compiler Service → Sandbox. |
| NON-004 | **Bukan forum / komentar / akun pengguna** — diskusi tetap di issue tracker + file RFC. |
| NON-005 | **Tidak mengubah semantik bahasa** — website mendeskripsikan spec 2.0 normatif + adendum; perubahan bahasa wajib RFC. |

---

# 4. Information Architecture

## 4.1 Struktur folder website (target)

```text
website/
├── index.html            # Homepage (hero, intro, highlights, install, quickstart)
├── learn/
│   ├── index.html        # Kurikulum 01–08 + learning path
│   ├── beginner/         # 01 hello, 02 variables, 03 functions
│   ├── intermediate/     # 04 generics-traits, 05 match-enum, 06 error-handling
│   └── advanced/         # 07 closures-concurrency, 08 package-project
├── language/
│   ├── index.html        # Indeks 12 topik + peta ke spec-2.0
│   ├── syntax.html       # Grammar, keyword (incl. reserved async/await), operator, ?
│   ├── variables.html
│   ├── functions.html
│   ├── types.html        # i64/bool/string/void/fn/array/struct + alias `type`
│   ├── structs.html
│   ├── enums.html        # Payload, ctor qualified, exhaustiveness
│   ├── generics.html     # Monomorphisasi
│   ├── traits.html       # Bounds, dispatch statis, koherensi
│   ├── pattern-matching.html
│   ├── error-handling.html  # Option/Result enum natif + `?`
│   ├── concurrency.html  # thread/channel/atomic/mutex + chan_len
│   └── modules.html      # import, NUSA_PATH, flat modules
├── reference/
│   ├── cli.html          # 22+ subperintah nusa --help
│   ├── fmt.html
│   ├── docgen.html
│   └── diagnostics.html  # Format error + katalog pesan
├── stdlib/
│   ├── index.html        # Indeks 18 modul
│   ├── io.html  math.html  string.html  collections.html
│   ├── fs.html  system.html  time.html  random.html
│   ├── process.html  path.html  thread.html  sync.html
│   ├── net.html  http.html  tasks.html
│   └── error.html  option.html  result.html
├── compiler/
│   ├── index.html        # Pipeline diagram + peta lapisan
│   ├── architecture.html # Lapisan 2.2, aturan kopling, ~26k LOC map
│   ├── frontend.html     # Lexer/parser/AST/semantic/typechecker
│   ├── mir.html          # MIR v2.3.0 (belum rewire penuh)
│   ├── optimizations.html# Opt v2.4.0 + -O + bench
│   ├── runtime.html      # rt_*.c modular v2.6.0, 30 simbol
│   ├── memory-model.html # Owned + program-lifetime heap + audit 2.5.0
│   ├── llvm.html         # Integrasi LLVM 21.1.8, triple, emit
│   ├── debugging.html    # -g DWARF skalar+composite, dwarfdump
│   ├── lsp.html          # 7 fitur + references, satu-file
│   ├── incremental.html  # Fingerprint + cache IR
│   ├── bootstrap.html    # selfhost/lex.ns Stage 0→1→2
│   ├── ffi.html          # extern fn + bindgen subset
│   └── targets.html      # Host/cross/wasm32 matrix
├── ecosystem/
│   ├── index.html
│   ├── packaging.html    # nusa.toml, pkg build, lock, capabilities
│   ├── registry.html     # File-backed index+tarball, search/install/publish
│   ├── templates.html    # new hello-world/cli-app/library/server
│   └── security.html     # Capability + traversal guard + audit
├── tutorials/
│   └── index.html        # Mirror kurikulum (taut ke learn/)
├── examples/
│   ├── index.html        # Indeks 18 examples runnable
│   └── {hello,arithmetic,...,concurrency}.html
├── rfcs/
│   ├── index.html        # Status filter: Proposed/Accepted/Implemented/Rejected/Superseded
│   └── RFC-*.html        # 0001–0008 + template
├── changelog/
│   ├── index.html        # Timeline + filter versi
│   └── vX.Y.Z.html       # Per versi, digenerate dari CHANGELOG.md
├── downloads/
│   ├── index.html        # Picker versi stabil + sha256 (tanpa-JS dropdown)
│   └── nusa_<ver>-1_aarch64.deb
├── playground/
│   └── index.html        # UI playground (progressive enhancement)
├── search/
│   └── index.html        # UI search offline (index JSON statis)
├── ai/
│   ├── index.html        # Cara pakai docs untuk AI
│   └── dataset.json      # Export massal berversi (generated)
├── assets/
│   ├── style.css         # Satu stylesheet (Nusa Terminal Theme)
│   ├── app.js            # Minimal JS (search, copy, playground client)
│   └── font/             # System fonts only (tanpa webfont eksternal)
└── sitemap.xml + robots.txt + manifest.webmanifest + sw.js
```

Navigasi global (header): `Learn | Language | Stdlib | Compiler | Ecosystem | RFCs | Changelog | Downloads | Playground | Search`.

## 4.2 Fungsi setiap bagian

| Bagian | Fungsi | Sumber kebenaran |
|---|---|---|
| `learn/` | Kurikulum 01–08 + 3 path (beginner/intermediate/advanced); tiap modul = README + main.ns runnable. | `tutorials/*`, `docs/getting-started.md` |
| `language/` | Referensi 12 topik dengan format 7-seksi; menunjuk spec normatif. | `docs/syntax,types,functions,structs,enums,traits,pattern-matching,error-handling,concurrency,modules,arrays,control-flow` + `language-spec-2.0.md` |
| `reference/` | CLI lengkap, fmt, docgen, katalog diagnostics/error-message. | `nusa --help`, `docs/compiler.md`, `compiler/diagnostics/` |
| `stdlib/` | API per modul/fungsi: signature, param, return, example, error behavior. | `std/*.ns` + `nusa doc` + `docs/standard-library.md` |
| `compiler/` | Internal: pipeline, MIR/Opt, runtime, memory, LLVM, DWARF, LSP, incr, bootstrap, FFI, target. | `docs/architecture-2.2,ir-2.3,opt-2.4,runtime-2.6,mem-2.5,debugging*,lsp,incremental,bootstrap,ffi*,targets,wasm-2.18,perf*` |
| `ecosystem/` | pkg/registry/template/security/repro. | `docs/pkg,ecosystem*,security-2.10,repro-2.12` + `packaging/deb.sh` |
| `tutorials/`, `examples/` | Indeks runnable + tombol copy + run-in-playground. | `tutorials/`, `examples/` |
| `rfcs/` | Sistem RFC berstatus + template 8 bagian. | `rfcs/` |
| `changelog/` | Otomatis dari `CHANGELOG.md`: version page + release notes + timeline. | `CHANGELOG.md` |
| `downloads/` | Picker `.deb` aarch64 + sha256 + syarat (`apt`, clang). | `packaging/deb.sh`, `website/downloads/` |
| `playground/` | UI coba-kode (enhancement di atas API lokal). | `docs/playground.md`, `Driver::runPlayground/runSandbox` |
| `search/` | UI + indeks offline. | Indeks generate dari konten |
| `ai/` | Skema dataset + panduan retrieval/QA + dump JSON. | `dataset/` (existing) |

**Requirement ID bagian ini:**

| ID | Requirement |
|---|---|
| REQ-014 | Struktur folder + navigasi WAJIB mengikuti §4.1; tiap bagian punya `index.html` + fungsi sesuai §4.2. |
| REQ-015 | Homepage WAJIB memuat hero, intro Nusa, feature highlights, installation button, quickstart (detail §5.1). |
| REQ-016 | Halaman changelog WAJIB digenerate otomatis dari `CHANGELOG.md` (version page + release notes + timeline). |

---

# 5. Core Features

## 5.1 Homepage

| ID | Requirement |
|---|---|
| REQ-017 | Hero section: nama Nusa, tagline ("Compiled `.ns` → native via LLVM 21.1.8"), badge versi stabil (dibaca dari `CHANGELOG.md` head), CTA [Install] + [Quick Start] + [Coba Playground]. |
| REQ-018 | Introduction: 3 kalimat (apa Nusa, untuk siapa, kenapa beda) + penulis (Padil, Banyumas) + target (Android AArch64/Termux). |
| REQ-019 | Feature highlights (6 kartu): Static typing, Generics+Traits, Enum+Match+`?`, Concurrency, Pkg+Registry+FFI, LSP+DWARF+Bootstrap. Tiap kartu → halaman referensi. |
| REQ-020 | Installation button → `downloads/` (deteksi aarch64 + peringatan arsitektur lain) + perintah `apt install ./nusa_X-1_aarch64.deb`. |
| REQ-021 | Quick start (copy-paste 5 baris): `pkg install clang cmake llvm` → build → `nusa run tutorials/01-hello/main.ns` → `Hello, Nusa!`. |

User story: *Sebagai pemula, saya ingin dalam 2 menit paham apa itu Nusa
dan menjalankannya, sehingga saya yakin lanjut belajar.*

Acceptance: CTA install mengarah ke `.deb` yang ada + sha256 cocok;
quickstart lolos uji fresh-Termux; Lighthouse homepage > 95.

## 5.2 Learning System

Tiga path, 8 modul (pemetaan 1:1 ke `tutorials/`):

| Path | Modul | Keluar (bisa apa) |
|---|---|---|
| Beginner | 01-hello, 02-variables, 03-functions | build/run/check; `let`, tipe, array, fn, struct dasar |
| Intermediate | 04-generics-traits, 05-match-enum, 06-error-handling | generic, trait/impl, match exhaustiveness, Option/Result + `?` |
| Advanced | 07-closures-concurrency, 08-package-project | lambda capture-by-value, thread/channel/atomic, `nusa pkg/new/test`, registry |

| ID | Requirement |
|---|---|
| REQ-022 | Setiap modul tutorial WAJIB: Penjelasan (konsep + kenapa), Source `.ns` (verbatim dari `tutorials/*/main.ns`, fmt-clean), Output program (verbatim run), Exercise (2–4 soal + solusi tersembunyi `<details>`), Next step (taut modul berikut + topik referensi). |
| REQ-023 | Setiap modul WAJIB lolos `nusa run` + `nusa fmt --check` di CI website (sample code tidak boleh basi). |
| REQ-024 | Path WAJIB menampilkan prasyarat + estimasi waktu + checklist progres (localStorage saja, tanpa akun). |

## 5.3 Language Reference (12 topik)

Topik: Syntax, Variables, Functions, Types, Struct, Enum, Generics,
Traits, Pattern Matching, Error Handling, Concurrency, Modules.

| ID | Requirement |
|---|---|
| REQ-025 | Format 7-seksi per halaman: Overview, Syntax (code fence `nusa` + EBNF ringkas), Example (runnable + output), Rules (do/don't list), Compiler behavior (tahap pipeline yang menolak/menulis ulang, cth. enum→hidden struct tag+slot, match→desugar if-else, trait→rewrite mangled), Limitations (kontrak jujur per versi), Related topics. |
| REQ-026 | Setiap klaim perilaku WAJIB menaut ke `language-spec-2.0.md` § + file uji pengunci (`Tests:`) bila ada. |
| REQ-027 | Halaman WAJIB menandai breaking 2.0 (Option/Result enum, ctor qualified, field lama ditolak, `async` reserved) + taut migrasi. |

Contoh per-topik (ringkas, bukan isi penuh):

- **Concurrency:** `thread_spawn(fn()->i64)`, `chan_new/send/recv` (+ varian `_bool/_str` bertag, mismatch=exit 1), `mutex_*`, `atomic_*`, `sleep_ms`, `chan_len` (O(1), race-tolerant); batas: channel i64-centric, tanpa select/close, tanpa async.
- **Error Handling:** `enum Option<T>{Some,None}`, `enum Result<T,E>{Ok,Err}`, kombinator, `?` khusus Result; batas: tanpa exception, tanpa konversi otomatis.
- **FFI (di compiler/ffi):** `extern fn` primitif, bool=i32 ABI, `--link/--link-dir`, `[link]` manifest; batas eksplisit.

## 5.4 Standard Library Documentation (18 modul)

Modul: io, math, string, collections, fs, system, time, random,
process, path, thread, sync, net, http, tasks, error, option, result.

| ID | Requirement |
|---|---|
| REQ-028 | Dokumentasi API WAJIB digenerate dari `nusa doc std/*.ns` (komentar `///`) + kurasi manual; format per fungsi: Module, Signature, Parameter (tipe+makna), Return value, Example (runnable), Error behavior (nilai gagal: `""`/`false`/`-1`, `runtime error: invalid handle`, dsb). |
| REQ-029 | Setiap modul WAJIB mencantumkan versi sejak-kapan + perubahan (cth. `chan_len` → 2.7.0, `std/http` → 2.9.0, `std/tasks` → 2.8.0). |
| REQ-030 | Contoh stdlib WAJIB diuji run di CI (`e2e_stdlib2`-setara untuk snippet web). |

## 5.5 Compiler Documentation

Diagram pipeline normatif (ASCII + SVG):

```text
Source (.ns)
  ↓ Lexer (token + `//`/`///`, keyword incl. extern/enum/match/trait/impl/self/type)
  ↓ Parser (lookahead, struct/import/literal/field, lambda, match arm, extern fn)
  ↓ AST (nodes + s-expr dump)
  ↓ Semantic Analysis (namespace, scope, struct, builtin registry)
  ↓ Type Checker (alias, generic monomorphisasi, trait bound, exhaustiveness, ?)
  ↓ MIR (v2.3+, typed values/blocks, verify/dump) → Opt (fold/DCE, fixpoint)
  ↓ LLVM Backend (LLVM 21.1.8 functions/blocks/arrays/structs/libc, DIBuilder -g)
  ↓ Native Binary (clang link + runtime/rt_*.c + -pthread, -O0..3/s/z, triple)
```

| ID | Requirement |
|---|---|
| REQ-031 | Halaman compiler WAJIB mencakup: Architecture (peta lapisan + aturan kopling), Compiler design (per tahap + file sumber), Runtime (`rt_handle/thread/sync/chan/time/net`, 30 simbol, registry handle + fail-fast), Memory model (owned + program-lifetime heap + audit), LLVM integration (21.1.8, triple, `--emit-ir/asm/-c`, `--sysroot`). |
| REQ-032 | Setiap batasan WAJIB jujur + berversi (cth. MIR belum rewire; DWARF tanpa engine interaktif; LSP satu-file tanpa typechecker; incremental tanpa daemon; bootstrap lexer-only). |

## 5.6 RFC Documentation

Status sah: Proposed, Accepted, Implemented, Rejected (+ Superseded
per `rfcs/README.md`).

| ID | Requirement |
|---|---|
| REQ-033 | Sistem RFC web WAJIB: indeks berfilter status, nomor `RFC-NNNN-slug` tanpa lompatan, badge status, taut spec/implementasi/test. |
| REQ-034 | Template web WAJIB memuat: Motivation, Design, Alternatives, Implementation, Testing (+ Drawbacks, Unresolved per TEMPLATE). Validasi mesin `e2e_rfc`-setara di CI. |

## 5.7 Changelog System

| ID | Requirement |
|---|---|
| REQ-035 | Website WAJIB membaca `CHANGELOG.md` saat build (`gen.py`): Version page per `## [X.Y.Z]`, Release notes (Ditambahkan/Diubah/Diperbaiki/Batasan/Diverifikasi), Feature timeline (filter versi + topik). |
| REQ-036 | Entri changelog web WAJIB menaut balik ke docs terdampak (cth. 2.7.0 → concurrency + `chan_len`; 2.17.0 → bindgen; 2.18.0 → wasm32). |

User stories & acceptance lengkap untuk §5 lihat Appendix B
(US-001–US-010).

---

# 6. Search System

## 6.1 Spesifikasi

| ID | Requirement |
|---|---|
| REQ-037 | Search WAJIB mendukung 5 mode: keyword, code (identifier/snippet), API (modul/fungsi/signature), error-message (verbatim `runtime error:` + diagnostics `file:line:col`), version filtering (dropdown versi, default = stabil). |
| REQ-038 | Search WAJIB offline-first: indeks statis (`search/index.json`, per-halaman `keywords` + heading anchor) + JS ≤ 30 KB; tanpa server saat offline; degradasi ke daftar indeks alfabetis bila JS mati. |
| REQ-039 | Hasil WAJIB menampilkan: judul, kategori, versi, snippet ±2 baris, skor, dan deep-link anchor. Contoh wajib: query `chan_len` → (1) Concurrency doc, (2) Stdlib sync/thread, (3) Changelog 2.7.0. |

Contoh kontrak:

```text
Input:  "chan_len"
Output:
  1. language/concurrency.html#chan-len — Concurrency documentation (v2.7.0+)
  2. stdlib/sync.html / stdlib/thread.html — Standard library
  3. changelog/v2.7.0.html — Changelog (observability + stress suite)
```

## 6.2 Korpus & ranking

- Korpus: semua halaman + `keywords` front-matter + identifier dari
  `nusa doc` + pesan error dari `compiler/diagnostics` + judul
  `CHANGELOG.md`.
- Ranking: exact identifier (100) > judul (80) > API signature (70) >
  keyword (50) > body (20); boost versi stabil +15; penalti historis −20
  (tetap tampil dengan badge).
- Version filter: `all | 2.x | 1.x | <exact>`; default stabil.

## 6.3 Technical notes

- Build: `scripts/build-search.py` memindai `website/**/*.html` +
  `std/*.ns` + diagnostics → `search/index.json` (jamin < 500 KB; split
  per-kategori bila lebih).
- E2E: 50 query uji (termasuk `chan_len`, `Option.None`, `runtime error:
  invalid handle`, `wasm32`, `bindgen`) — top-3 relevan.

---

# 7. Code Documentation System

| ID | Requirement |
|---|---|
| REQ-040 | Syntax highlighting WAJIB untuk 6 bahasa: Nusa (`.ns`), C++, LLVM IR, Bash, JSON, TOML — tanpa CDN (highlight mandiri ≤ 15 KB atau pre-render saat build). |
| REQ-041 | Setiap blok kode WAJIB punya tombol Copy (clipboard + fallback select; tanpa JS = teks tetap terbaca). |
| REQ-042 | Setiap contoh `.ns` runnable WAJIB punya tombol "Coba di Playground" (deep-link dengan kode ter-encode) + Output preview (verbatim, `<samp>`). |
| REQ-043 | Snippet WAJIB diuji: `.ns` lolos `fmt --check` + `run`; C++/IR/Bash/JSON/TOML lolos parse/highlight tanpa error; CI gagal bila snippet basi. |

Technical notes:

- Bahasa `nusa` didaftarkan sebagai grammar sendiri (keyword:
  `fn let match enum struct trait impl for self type extern return if
  else while import`, operator `? => |`, literal, komentar `//`/`///`).
- `///` doc-comment WAJIB dipertahankan pewarnaannya (regresi formatter
  2.8.0 tidak boleh terulang di web).
- Output preview dibungkus `<details>` agar mobile hemat ruang.

---

# 8. Interactive Playground

## 8.1 Desain (sesuai arsitektur master)

```text
Browser (playground/index.html + app.js)
  ↓  POST /api/run {code, stdin?, timeout?} JSON, header/body cap
Playground API (nusa playground --port P, 127.0.0.1-only, sekuensial)
  ↓  tulis tmpdir + nusa build --fresh
Compiler Service (pipeline penuh + IR di samping source)
  ↓  nusa sandbox (fork, grup proses, tmpdir, env scrub, /dev/null stdin)
Sandbox (RLIMIT_CPU + polling VmRSS, output cap, timeout total)
  ↓  verdict ok/timeout/mem-limit/output-limit/signal/build-fail + stdout/stderr/rc
Output (JSON → browser render + share-link ?code=)
```

| ID | Requirement |
|---|---|
| REQ-044 | Playground web WAJIB mengikuti rantai Browser → Playground API → Compiler Service → Sandbox → Output; implementasi server MEMAKAI ULANG `Driver::runSandbox/runPlayground` yang ada (tanpa engine baru). |
| REQ-045 | Model keamanan (§8.2), limit resource (§8.3), dan error handling (§8.4) WAJIB diimplementasikan + didokumentasikan jujur di halaman playground. |

## 8.2 Security model

- Loopback-only (`127.0.0.1`, `--port 0` ephemeral didukung); tanpa auth
  (didokumentasikan: JANGAN expose publik tanpa reverse-proxy + auth +
  rate-limit di depan).
- Tiap run: tmpdir acak, env scrub, `/dev/null` stdin, grup proses
  sendiri (kill grup saat timeout), `import` lokal nonaktif di mode
  playground.
- Batasan jujur (dari `docs/playground.md`): memori via polling VmRSS
  (RLIMIT_AS merusak startup Bionic — diverifikasi); tanpa namespace;
  NPROC sengaja tak dipakai; jaringan TIDAK diblokir (jangan jalankan
  kode tak dipercaya di host sensitif); server sekuensial.

## 8.3 Resource limitation

| Batas | Default web | Dapat dikonfigurasi |
|---|---|---|
| Timeout total (build+run) | 10 dtk | `--timeout S` |
| Memori (polling VmRSS) | 256 MB | `--memory-mb M` |
| Output (stdout+stderr) | 64 KB | `--output-kb K` |
| Header/body HTTP | 16 KB / 256 KB | tetap (DDoS guard) |
| Concurrency | sekuensial (antrean FIFO + pesan "busy") | dokumentasikan |

## 8.4 Error handling

Verdict mesin: `ok | build-fail | timeout | mem-limit |
output-limit | signal`. Browser menampilkan: badge verdict + `rc` +
stdout + stderr (terpotong dengan penanda) + saran next-step
(taut diagnostics). Bad-request + 404 diuji (`e2e_playground`-setara).

---

# 9. AI Documentation Support

Tujuan: dokumentasi siap training/retrieval tanpa scraping liar.

## 9.1 Dataset export format (kontrak)

```json
{
  "title": "chan_len — Channel Observability",
  "category": "language/concurrency",
  "version": "2.7.0",
  "content": "Fungsi chan_len(c: i64) -> i64 ... race-tolerant by-design ...",
  "code_example": "let c = chan_new();\nchan_send(c, 1);\nprint(int_to_str(chan_len(c)));",
  "keywords": ["chan_len", "channel", "concurrency", "observability"]
}
```

| ID | Requirement |
|---|---|
| REQ-046 | Setiap halaman WAJIB mengekspos JSON-LD + `<meta name="nusa:*">` + endpoint `?format=json` dengan 6 field di atas; skema tervalidasi mesin di CI. |
| REQ-047 | Build WAJIB menghasilkan `ai/dataset.json` (dump massal berversi, ≤ 5 MB per versi; split per kategori bila lebih) + `ai/index.html` panduan. |

## 9.2 Konsumsi oleh AI

| Konsumen | Cara pakai | Requirement |
|---|---|---|
| Retrieval system | Chunk per halaman (≤ 512 token, overlap 50) + embedding `keywords` + filter `version`; sitasi wajib (judul + URL + versi). | REQ-048: chunking deterministik + ID stabil (`<slug>#<anchor>`). |
| Chat QA | Prompt template: jawab HANYA dari konteks + sebut versi + akui bila di luar korpus; evaluasi 100 Q/A emas (50 should-answer, 50 should-refuse). | REQ-049: halaman `ai/` memuat template + set evaluasi. |
| Nusa Helper | Fine-tune/rebuild dari `dataset/` existing (55 dokumen → 1006 baris) + dataset web baru; notebook Colab dapat rebuild index offline. | REQ-050: dataset web kompatibel skema `dataset/` + notebook teruji. |

Technical notes: versi adalah first-class (`version` wajib; konten tanpa
versi ditolak CI); konten Proposed (RFC-0004/0007/0008) bertag
`stability: proposed` agar AI tidak menyajikannya sebagai fitur stabil.

---

# 10. UI/UX Design

Tema: **"Nusa Terminal Theme"** — dark-mode, developer-style, modern, minimal.

## 10.1 Color system

| Token | Nilai | Pakai |
|---|---|---|
| `--bg` | `#0b0e11` | background |
| `--bg-2` | `#11151a` | panel/sidebar |
| `--fg` | `#e6edf3` | teks utama |
| `--muted` | `#8b949e` | teks sekunder |
| `--accent` | `#3fb950` (fosfor hijau) | link/CTA primer |
| `--accent-2` | `#58a6ff` (biru ledger) | tautan sekunder/info |
| `--warn` | `#d29922` | peringatan/batasan |
| `--danger` | `#f85149` | error/breaking |
| `--border` | `#21262d` | garis |
| `--code-bg` | `#0d1117` | blok kode |

Kontras: teks/body ≥ 7:1; muted ≥ 4.5:1. Light-mode opsional via
`prefers-color-scheme` (inversi token, default tetap dark).

## 10.2 Typography

- Body: system stack (`-apple-system, Segoe UI, Roboto, Noto Sans,
  Ubuntu, sans-serif`), 16 px / 1.65.
- Code: `ui-monospace, SFMono-Regular, Menlo, Consolas, monospace`,
  14 px / 1.55. Tanpa webfont eksternal (offline-first).
- Heading: 32/24/20/18 px, weight 700/600; anchor `#` on-hover.

## 10.3 Layout & navigation

- Layout: sidebar kiri (navigasi kategori, collapsible) + konten
  tengah (max 76 ch) + TOC kanan (anchor H2/H3, desktop saja).
- Header: logo ▚ Nusa + versi badge + nav global + search box +
  [Unduh] CTA.
- Footer: versi, taut repo/docs, sitemap, "Offline-ready (SW)".
- Breadcrumb + prev/next di tiap halaman learn/language.

## 10.4 Responsive behavior

| Breakpoint | Perilaku |
|---|---|
| ≥ 1100 px | 3 kolom (sidebar + konten + TOC) |
| 700–1099 px | 2 kolom (sidebar overlay + konten); TOC jadi dropdown |
| < 700 px | 1 kolom; nav jadi hamburger; tabel scroll-x; kode wrap dengan scroll; playground editor full-width |

| ID | Requirement |
|---|---|
| REQ-051 | Design system WAJIB mengimplementasikan token §10.1–10.4 dalam satu `assets/style.css` (+ `details.pick`-style untuk dropdown tanpa-JS mengikuti pola `e2e_deb`). |
| REQ-052 | Website WAJIB keyboard-navigable (skip-link, focus-visible, ESC menutup overlay) + `prefers-reduced-motion` dihormati. |

---

# 11. Technical Specification

## 11.1 Rekomendasi (dipilih + alasan)

| Lapisan | Rekomendasi | Alasan |
|---|---|---|
| Frontend | **Static HTML/CSS/JS murni** (tanpa framework modern). Satu `style.css` + satu `app.js` minimal. | Offline-first, < 1 dtk, tanpa build-step; konsisten dengan situs statis existing (tanpa JS eksternal/CDN/tracking). Framework (Astro/Docusaurus) DITOLAK untuk v1 karena menambah toolchain + JS. |
| Documentation engine | **Markdown → HTML via `website/gen.py`** (stdlib saja) + front-matter `title/category/version/keywords`. Parser: Python-Markdown atau mistune (tanpa Node). | Sumber sudah Markdown (`docs/`, `tutorials/*/README.md`, `rfcs/`); generate per-dokumen 1 HTML (pola "53 halaman via gen.py"); tautan `.md` mentah dilarang di situs. |
| Search index | `scripts/build-search.py` → `search/index.json` statis. | Offline search tanpa backend. |
| Playground server | Reuse `nusa playground`/`nusa sandbox` (C++ existing). | Tanpa reinvent sandbox; keamanan sudah diaudit. |
| Deployment | **Opsi 1: GitHub Pages** (default publik) · **Opsi 2: Cloudflare Pages** (preview + cache) · **Opsi 3: Local hosting** (`python3 -m http.server`, wajib jalan). | Semua serve file statis; tidak butuh backend kecuali playground opsional. |

## 11.2 Requirement lintas-fungsi

| ID | Requirement |
|---|---|
| REQ-053 | Fast loading: HTML ≤ 60 KB/halaman (tanpa gambar), CSS ≤ 25 KB, JS ≤ 45 KB total; gambar hanya SVG/PNG terkompresi + `loading=lazy`. |
| REQ-054 | Offline support: Service Worker cache-first untuk HTML/CSS/JS + indeks search; halaman baca 100% offline; playground/search-server ditandai "butuh koneksi/lokal". |
| REQ-055 | SEO friendly: semantic HTML, `<title>` + meta description per halaman, canonical, sitemap.xml, robots.txt, OpenGraph minimal, JSON-LD dataset (§9). |
| REQ-056 | Accessibility: WCAG 2.2 AA (kontras, alt, label form, heading order, focus, 200% zoom tanpa scroll horizontal konten). |

---

# 12. Version Roadmap

Versi website (Docs) independen dari versi bahasa; tiap rilis Docs
mencantumkan bahasa yang didukung.

| Docs | Bahasa | Isi | Exit criteria |
|---|---|---|---|
| **v0.1.0** Basic documentation | 2.18.0 | IA §4 + Homepage + Learn 01–08 + Language 12 topik (format 7-seksi) + `gen.py` + style Nusa Terminal + deploy Pages + serve lokal. | DoD-1–4 (lihat §16); e2e link-check hijau; snippet run hijau. |
| **v0.2.0** Reference system | 2.18.x | Stdlib 18 modul (dari `nusa doc`) + CLI reference + diagnostics catalog + Search offline (5 mode + version filter). | Query `chan_len` top-3 benar; katalog error 100% pesan umum. |
| **v0.3.0** Compiler documentation | 2.19.x | Compiler section 14 halaman + RFC index berstatus + Changelog otomatis + Downloads picker + sha256. | Tiap klaim compiler menaut sumber+test; changelog generate tanpa edit manual. |
| **v0.4.0** Interactive playground | 2.20.x | Playground UI → reuse `nusa playground` + sandbox verdict + share-link + rate-limit docs; hardening audit. | 9 kasus `e2e_playground`-setara hijau via web; nol escape. |
| **v0.5.0** AI documentation layer | 2.21.x | JSON-LD per halaman + `ai/dataset.json` + chunking + 100 Q/A emas + notebook Nusa Helper rebuild. | Export tervalidasi; retrieval sitasi benar versi. |

| ID | Requirement |
|---|---|
| REQ-057 | Roadmap WAJIB dieksekusi berurutan v0.1.0 → v0.5.0; tiap rilis WAJIB mencantumkan versi bahasa yang didukung + delta spec. |

---

# 13. Database / File Structure

## 13.1 Jika static system (PILIHAN UTAMA — dipakai v1)

```text
docs/            # sumber Markdown (49 file, tidak diubah strukturnya)
tutorials/       # 01–08 (README.md + main.ns)
examples/        # 18 .ns runnable
rfcs/            # README + TEMPLATE + RFC-0001..0008
std/             # 18 modul sumber API
CHANGELOG.md     # sumber changelog
website/         # OUTPUT statis (§4.1)
scripts/
├── gen.py               # md → html (front-matter, nav, prev/next, sitemap)
├── build-search.py      # → search/index.json
├── build-dataset.py     # → ai/dataset.json (skema §9)
├── check-links.py       # e2e link-check (resolusi per-file incl. subdir)
├── test-snippets.sh     # run + fmt --check semua .ns web
└── validate-dataset.py  # validasi skema JSON-LD
assets/ → website/assets/ (style.css, app.js, svg)
```

| ID | Requirement |
|---|---|
| REQ-058 | Mode statis WAJIB tanpa database; build deterministik (`gen.py` idempoten; output byte-stabil untuk input sama; `clean` menghapus artefak via pola `nusa clean`). |

## 13.2 Jika menggunakan backend (OPSIONAL — hanya playground + search-server)

Database: **tanpa RDBMS** (SQLite opsional untuk log/rate-limit saja).

```sql
-- playground_runs (opsional, retensi 7 hari, tanpa PII)
CREATE TABLE runs (
  id TEXT PRIMARY KEY, created_at INTEGER NOT NULL,
  code_sha256 TEXT NOT NULL, verdict TEXT NOT NULL,
  rc INTEGER, duration_ms INTEGER, truncated INTEGER
);
```

API endpoint (loopback, didokumentasikan OpenAPI-lite di `ai/`):

| Method & path | Fungsi | Batas |
|---|---|---|
| `GET /` | Halaman playground embedded | — |
| `POST /api/run` `{code, stdin?}` | Compile+run di sandbox | body ≤ 256 KB, timeout 10 dtk |
| `GET /api/health` | Versi nusa + LLVM + kapasitas | — |
| `GET /api/examples` | Daftar contoh runnable | cache 1 jam |

| ID | Requirement |
|---|---|
| REQ-059 | Backend (jika ada) HANYA untuk playground/search-server; konten docs TETAP statis; skema + endpoint WAJIB sesuai §13.2; tanpa akun/PII. |

---

# 14. Security Requirement

| ID | Requirement |
|---|---|
| REQ-060 | Sandbox security: reuse `nusa sandbox` (tmpdir acak, env scrub, grup proses, RLIMIT_CPU, polling VmRSS, output cap); dokumentasikan batas jujur (tanpa namespace, jaringan tak diblokir, NPROC tak dipakai). |
| REQ-061 | XSS protection: escape semua kode/output (`<pre><code>` + `<samp>`); `Content-Security-Policy: default-src 'self'`; tanpa inline-script kecuali nonce; tanpa `innerHTML` untuk output program. |
| REQ-062 | Content validation: front-matter wajib (`title/category/version`); `version` cocok pola `X.Y.Z`; kode playground ≤ 256 KB + tolak NUL byte; `import` lokal ditolak di playground dengan pesan jelas. |
| REQ-063 | Safe code execution: eksekusi HANYA di sandbox/server loopback; situs statis (Pages) TIDAK mengeksekusi kode; tombol Run nonaktif elegan + pesan "jalankan lokal: `nusa run`" saat API mati. |

Threat model ringkas: penyerang = pembaca web (XSS via snippet) +
pengirim kode playground (escape, fork-bomb, pencurian env, spam).
Mitigasi: CSP + escape + cap + timeout + grup-kill + env scrub +
sekuensial + rate-limit di reverse-proxy (bila go-public).

---

# 15. Performance Requirement

| ID | Target | Cara ukur |
|---|---|---|
| REQ-064 | Lighthouse score > 95 (Performance; A11y/SEO = 100). | `lighthouse --preset=desktop --throttling` di CI untuk homepage + 3 halaman sampel. |
| REQ-065 | First load < 1 dtk (4G throttled, tanpa cache). | WebPageTest/Lighthouse TTI; budget §11.2 ditegakkan (`check-budget.sh`). |
| REQ-066 | Mobile friendly: ≥ 90 Performance mobile; tanpa scroll-x konten; tap-target ≥ 44 px. | Lighthouse mobile + uji manual 360 px. |
| REQ-067 | Minimal JavaScript: total ≤ 45 KB (gzip), nol dependensi eksternal; situs 100% terbaca + navigable tanpa JS (progressive enhancement). | `wc -c assets/*.js`; uji `noscript`. |

---

# 16. Definition of Done

Checklist rilis (semua WAJIB hijau sebelum merge ke `main`/Pages):

| # | Checklist | Verifikasi |
|---|---|---|
| DoD-1 | Dokumentasi lengkap (cakupan §4–5: learn/language/stdlib/compiler/ecosystem/rfc/changelog/downloads). | `check-coverage.py`: tiap sumber (`docs/`, `std/`, `rfcs/`, `tutorials/`) terindeks; nol yatim. |
| DoD-2 | Search bekerja (5 mode + version filter; `chan_len` top-3 benar). | `test-search.py` 50 query. |
| DoD-3 | Tutorial tersedia (8 modul format 5-elemen + run hijau). | `test-snippets.sh` + review konten. |
| DoD-4 | API reference tersedia (18 modul format 6-elemen + run hijau). | `nusa doc` diff + `test-snippets.sh`. |
| DoD-5 | Compiler docs tersedia (14 halaman + pipeline + batasan jujur). | Review compiler-dev + taut sumber/test. |
| DoD-6 | Mobile responsive (breakpoint §10.4). | Lighthouse mobile + screenshot 360/768/1280. |
| DoD-7 | Offline support (SW cache-first; baca offline 100%). | Uji airplane-mode + `python3 -m http.server`. |
| DoD-8 | AI dataset export tersedia (JSON-LD + `ai/dataset.json` tervalidasi). | `validate-dataset.py` + rebuild notebook. |
| DoD-9 | Keamanan (CSP, escape, sandbox cap, disclosure batas). | Audit §14 + `e2e_playground`-setara. |
| DoD-10 | Performa (budget §11.2 + target §15). | Lighthouse + `check-budget.sh`. |

---

# Appendix A — Requirement Traceability Matrix

| ID | Bagian | Judul singkat | Prioritas | Target rilis Docs |
|---|---|---|---|---|
| REQ-001 | §1 | Pusat informasi tunggal 5 audiens | P0 | v0.1.0 |
| REQ-002 | §1 | Akurasi per versi + tandai historis | P0 | v0.1.0 |
| REQ-003 | §2 | 4 jalur persona | P0 | v0.1.0 |
| REQ-004 | §2 | Format tutorial 5-elemen | P0 | v0.1.0 |
| REQ-005 | §2 | Format language-ref 7-seksi | P0 | v0.1.0 |
| REQ-006 | §3 | Onboarding ≤ 15 menit | P0 | v0.1.0 |
| REQ-007 | §3 | Cakupan 100% sumber | P0 | v0.1.0 |
| REQ-008 | §3 | Offline-first + cepat | P0 | v0.1.0 |
| REQ-009 | §3 | Label versi + uji pengunci | P0 | v0.1.0 |
| REQ-010 | §3 | Search akurat | P1 | v0.2.0 |
| REQ-011 | §3 | Playground aman | P1 | v0.4.0 |
| REQ-012 | §3 | Dataset export AI | P1 | v0.5.0 |
| REQ-013 | §3 | SEO + A11y | P1 | v0.2.0 |
| REQ-014 | §4 | Struktur folder + nav | P0 | v0.1.0 |
| REQ-015 | §4 | Homepage 5 blok | P0 | v0.1.0 |
| REQ-016 | §4 | Changelog otomatis | P0 | v0.3.0 |
| REQ-017 | §5.1 | Hero | P0 | v0.1.0 |
| REQ-018 | §5.1 | Intro | P0 | v0.1.0 |
| REQ-019 | §5.1 | Highlights 6 kartu | P0 | v0.1.0 |
| REQ-020 | §5.1 | Install button | P0 | v0.1.0 |
| REQ-021 | §5.1 | Quickstart | P0 | v0.1.0 |
| REQ-022 | §5.2 | Modul tutorial 5-elemen | P0 | v0.1.0 |
| REQ-023 | §5.2 | Tutorial run+fmt di CI | P0 | v0.1.0 |
| REQ-024 | §5.2 | Progres localStorage | P2 | v0.2.0 |
| REQ-025 | §5.3 | Language-ref 7-seksi | P0 | v0.1.0 |
| REQ-026 | §5.3 | Taut spec + Tests | P0 | v0.1.0 |
| REQ-027 | §5.3 | Tandai breaking 2.0 | P0 | v0.1.0 |
| REQ-028 | §5.4 | Stdlib dari `nusa doc` | P0 | v0.2.0 |
| REQ-029 | §5.4 | Since-version per API | P0 | v0.2.0 |
| REQ-030 | §5.4 | Snippet stdlib run CI | P0 | v0.2.0 |
| REQ-031 | §5.5 | Compiler 5 area | P0 | v0.3.0 |
| REQ-032 | §5.5 | Batasan jujur berversi | P0 | v0.3.0 |
| REQ-033 | §5.6 | RFC index berstatus | P0 | v0.3.0 |
| REQ-034 | §5.6 | Template 5(+2) bagian | P0 | v0.3.0 |
| REQ-035 | §5.7 | Changelog generate | P0 | v0.3.0 |
| REQ-036 | §5.7 | Taut balik changelog→docs | P1 | v0.3.0 |
| REQ-037 | §6 | 5 mode search | P1 | v0.2.0 |
| REQ-038 | §6 | Indeks offline ≤ 30 KB JS | P1 | v0.2.0 |
| REQ-039 | §6 | Format hasil + contoh chan_len | P1 | v0.2.0 |
| REQ-040 | §7 | Highlight 6 bahasa | P0 | v0.1.0 |
| REQ-041 | §7 | Copy button | P0 | v0.1.0 |
| REQ-042 | §7 | Coba-di-playground + output | P1 | v0.4.0 |
| REQ-043 | §7 | Snippet diuji CI | P0 | v0.1.0 |
| REQ-044 | §8 | Rantai 5 tahap reuse existing | P1 | v0.4.0 |
| REQ-045 | §8 | Security+limit+error wajib | P1 | v0.4.0 |
| REQ-046 | §9 | JSON-LD 6 field | P1 | v0.5.0 |
| REQ-047 | §9 | Dump `ai/dataset.json` | P1 | v0.5.0 |
| REQ-048 | §9 | Chunking retrieval | P1 | v0.5.0 |
| REQ-049 | §9 | Template Chat QA + eval | P1 | v0.5.0 |
| REQ-050 | §9 | Kompatibel dataset Nusa Helper | P1 | v0.5.0 |
| REQ-051 | §10 | Token Nusa Terminal 1 CSS | P0 | v0.1.0 |
| REQ-052 | §10 | Keyboard + reduced-motion | P0 | v0.2.0 |
| REQ-053 | §11 | Budget ukuran | P0 | v0.1.0 |
| REQ-054 | §11 | SW offline | P0 | v0.1.0 |
| REQ-055 | §11 | SEO | P0 | v0.2.0 |
| REQ-056 | §11 | WCAG AA | P0 | v0.2.0 |
| REQ-057 | §12 | Roadmap berurutan | P0 | v0.1.0–v0.5.0 |
| REQ-058 | §13 | Static tanpa DB | P0 | v0.1.0 |
| REQ-059 | §13 | Backend hanya playground | P1 | v0.4.0 |
| REQ-060 | §14 | Sandbox reuse | P1 | v0.4.0 |
| REQ-061 | §14 | XSS/CSP | P0 | v0.1.0 |
| REQ-062 | §14 | Validasi konten | P0 | v0.1.0 |
| REQ-063 | §14 | Eksekusi aman | P1 | v0.4.0 |
| REQ-064 | §15 | Lighthouse > 95 | P0 | v0.1.0 |
| REQ-065 | §15 | First load < 1 dtk | P0 | v0.1.0 |
| REQ-066 | §15 | Mobile ≥ 90 | P0 | v0.1.0 |
| REQ-067 | §15 | JS ≤ 45 KB, noscript OK | P0 | v0.1.0 |

Total: **67 requirements** (REQ-001–REQ-067) + 5 non-goals (NON-001–005)
+ 10 DoD (DoD-1–10).

---

# Appendix B — User Stories & Acceptance Criteria

| ID | User story | Acceptance criteria |
|---|---|---|
| US-001 | Sebagai pemula, saya ingin tombol Install + Quickstart sehingga program pertama jalan ≤ 15 menit. | Quickstart verbatim jalan di Termux AArch64 fresh + playground; tombol unduh → `.deb` + sha256 cocok. |
| US-002 | Sebagai pemula, saya ingin tiap tutorial ada exercise + next step sehingga tahu harus apa setelahnya. | 8/8 modul punya Exercise (2–4 soal + solusi `<details>`) + Next step bertaut; progres tersimpan localStorage. |
| US-003 | Sebagai developer, saya ingin referensi Option/Result + `?` dengan rules + limitations sehingga migrasi 1.x→2.0 benar. | Halaman error-handling memuat ctor qualified + 5 penolakan umum + taut migrasi; snippet run hijau. |
| US-004 | Sebagai developer, saya ingin API `std/http` + `chan_len` dengan error behavior sehingga tahu gagalnya seperti apa. | Tiap fungsi: signature + param + return + example run + error (`-1`/`""`/exit 1) terdokumentasi. |
| US-005 | Sebagai compiler dev, saya ingin diagram pipeline + peta lapisan sehingga tahu file mana yang diubah untuk fitur X. | Tiap tahap menaut file `compiler/*` + e2e test; aturan "Backend nol-include semantic" terdokumentasi + diuji. |
| US-006 | Sebagai kontributor, saya ingin RFC index berstatus sehingga tahu RFC-0004/0007/0008 belum boleh diimplementasi. | Filter Proposed/Accepted/Implemented/Rejected/Superseded jalan; halaman Proposed ber-badge "dilarang implementasi sebelum Accepted". |
| US-007 | Sebagai pengguna, saya ingin search `chan_len` sehingga dapat doc + stdlib + changelog sekaligus. | Top-3 = concurrency + stdlib + v2.7.0; version filter mengubah hasil benar. |
| US-008 | Sebagai pengguna, saya ingin copy + coba-di-playground sehingga contoh langsung jalan. | Copy tanpa JS tetap seleksi manual; playground deep-link membawa kode; verdict + rc tampil. |
| US-009 | Sebagai maintainer AI, saya ingin `ai/dataset.json` berversi sehingga Nusa Helper tidak menghalusinasi API. | Skema 6-field valid; 100% halaman ter-export; eval 100 Q/A lolos ambang. |
| US-010 | Sebagai mobile user, saya ingin baca offline sehingga tutorial bisa dibuka tanpa internet. | SW cache-first; airplane-mode: navigasi + search-index + 8 tutorial tetap bisa dibaca. |

---

# Appendix C — Technical Notes

1. **Sumber tunggal:** `docs/*.md`, `tutorials/*/README.md+main.ns`,
   `examples/*.ns`, `std/*.ns` (`///`), `rfcs/*.md`,
   `CHANGELOG.md`, `nusa --help` adalah input; `website/` adalah output.
   Jangan duplikasi konten manual — generate.
2. **`gen.py` pola yang dipertahankan:** per-dokumen 1 HTML, tautan
   relatif (`../docs|tutorials|examples|rfcs/` dilarang mentah `.md` di
   situs), daftar versi stabil tanpa-JS (`details.pick`), sitemap +
   indeks.
3. **Snippet CI:** `test-snippets.sh` = `nusa run` + `nusa fmt --check`
   untuk semua `.ns` yang tampil di web; `///` tidak boleh didegradasi
   menjadi `//` (regresi 2.8.0).
4. **Search build:** tokenisasi identifier (`chan_len`, `Option.None`,
   `runtime error: invalid handle`); normalisasi `const`/`volatile`
   tidak diperlukan di web, tetapi contoh FFI harus konsisten dengan
   `bindgen` (polos `unsigned` = int).
5. **Playground reuse:** jangan tulis ulang sandbox; panggil
   `nusa playground`/`nusa sandbox` dengan flag
   `--timeout/--memory-mb/--output-kb/--port`; hormati guard existing
   (sysroot/host untuk cross; `native=true` untuk FFI).
6. **Versioning:** label versi per halaman wajib; konten pra-2.12
   (lock tanpa pin compiler) dan pra-2.0 (Option/Result struct) diberi
   badge Historis; Proposed (async/GC/FFI-callback) diberi badge
   Proposed + larangan implementasi.
7. **Perf budget:** tegakkan via `check-budget.sh` (HTML/CSS/JS bytes)
   + `lighthouse` di CI; gambar SVG inline bila < 4 KB.
8. **Risiko + mitigasi:** (a) `website/` hilang → rebuild dari sumber
   via PRD ini; (b) snippet basi → CI run; (c) playground disalahgunakan
   → loopback + cap + sekuensial + docs jujur; (d) AI halusinasi →
   version-tag + eval set.

---

*End of PRD v1.0.0 — 67 requirements, 5 non-goals, 10 DoD, 10 user stories.
Siap di-review dan dipecah menjadi issue per Docs-release (v0.1.0 → v0.5.0).*
