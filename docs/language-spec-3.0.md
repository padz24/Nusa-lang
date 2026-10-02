# Nusa Language Specification 3.0

- Status: **normative** (mengikat implementasi, bukan sebaliknya).
  Meratifikasi dan menggantikan `docs/language-spec-2.0.md`
  (disimpan sebagai riwayat; bila ada klaim bertentangan antara
  dokumen riwayat dan dokumen ini, dokumen ini menang).
- Lampiran freeze Phase 81 (`docs/language-spec-3.0-candidate.md`)
  dinyatakan selesai dan historis.
- NOL perubahan sintaks/semantik dari 2.0: dokumen ini
  merumuskan ulang kontrak yang sama secara independen dari
  detail implementasi compiler (lihat §20).
- Konvensi: "MUST" = wajib, "MUST NOT" = dilarang,
  "undefined" = perilaku tak terdefinisi (lihat Appendix A).
- Detail tutorial per topik dirujuk ke `docs/*.md`; bila
  bertentangan, dokumen ini menang.

## 1. Lexical grammar

- Encoding sumber MUST UTF-8. Byte non-ASCII di luar
  string/komentar MUST ditolak.
- Whitespace: spasi, tab, CR, LF (pemisah tanpa makna).
- Komentar: `//` sampai akhir baris; `///` = dokumentasi API
  (tidak memengaruhi semantik).
- Keywords (reserved, MUST NOT dipakai sebagai identifier):
  `fn let return if else while true false i64 bool string void
  struct import type trait impl for self match enum extern const`
  ditambah kata reserved `async await macro`: ketiganya TAK
  memiliki sintaks — setiap pemakaian MUST ditolak (§11, §16).
- Identifier: `[A-Za-z_][A-Za-z0-9_]*` di luar keywords.
- Integer: `[0-9]+` desimal; nilai literal MUST dalam `i64`;
  overflow atau ekor-alpha (`123abc`) MUST ditolak.
- String: `"…"` dengan escapes `\" \\ \n \t \r \0`; newline
  mentah atau EOF sebelum penutup MUST ditolak.
- Operator/simbol: `+ - * / = == != < > <= >= -> => ? |`
  (`->` greedy; `!` tunggal ditolak dengan saran `!=`).

## 2. Syntax

- Unit kompilasi = himpunan file sumber; tiap file = urutan
  item top-level: `import`, alias tipe (`type`), deklarasi
  trait, blok `impl`, deklarasi struct, deklarasi enum,
  deklarasi fungsi, deklarasi `extern`, deklarasi `const`.
- Program runnable MUST mendefinisikan tepat satu
  `fn main()` tanpa parameter; file tanpa `main` adalah
  library (§12).
- Fungsi: `fn` nama [params generik] `(params)` [`->` tipe]
  blok. Parameter dan tipe kembalian eksplisit; `return`
  mengembalikan nilai ke pemanggil.
- `let` mendeklarasikan variabel dengan tipe eksplisit;
  `const` mendeklarasikan konstanta bertipe dengan inisialisasi
  ekspresi-konstan (§15).
- Pernyataan: deklarasi, assignment, `if`/`else`, `while`,
  `match` (§7), `return`, pemanggilan. NOL `break`, `continue`,
  `+=` atau operator gabungan lain — pemakaiannya MUST ditolak.
- Grammar rinci: `docs/syntax.md`.

## 3. Types

- Tipe primitif: `i64` (bulat 64-bit, §19), `bool`
  (`true`/`false`), `string` (teks UTF-8, §8), `void`
  (tipe kembalian tanpa nilai).
- Tipe komposit: struct bernama, enum (§6), dan instantiasi
  generik (§4). Alias via `type Nama = Tipe`.
- NOL tipe null/nil, NOL float, NOL unsigned, NOL tipe
  platform-spesifik: setiap tipe berperilaku identik di semua
  platform (§19).
- `Option<T>`/`Result<T, E>` adalah enum natif (§6); fungsi
  bantunya berasal dari modul standard library dan WAJIB
  diimpor eksplisit sebelum dipakai.
- Ketidakcocokan tipe MUST ditolak sebelum eksekusi dengan
  diagnostik §17.

## 4. Generics

- Fungsi, struct, dan enum MAY diparameterisasi daftar
  parameter tipe (`<T, …>`); tiap penggunaan dengan argumen
  tipe konkret berperilaku identik dengan salinan item yang
  ditulis manual untuk tipe tersebut.
- Argumen tipe disimpulkan atau ditulis eksplisit; ambiguitas
  MUST ditolak, bukan ditebak.
- DILARANG: metode generik, `impl` generik, tipe `Self`,
  associated function — pemakaiannya MUST ditolak.

## 5. Traits

- `trait` mendeklarasikan himpunan signature metode;
  `impl Trait for Tipe` menyediakan implementasinya.
  Pemanggilan metode didispatch ke implementasi tipe penerima.
- DILARANG: supertrait, body default, `Self` — pemakaiannya
  MUST ditolak.
- Detail: `docs/traits.md`.

## 6. Enums

- `enum` mendeklarasikan varian bernama, masing-masing dengan
  nol atau lebih payload bertipe; enum MAY generik (§4).
- Nilai enum dibangun via konstruktor varian eksplisit
  (`Varian(payload)`); inspeksi nilai HANYA via `match` (§7)
  — akses field/tag langsung MUST ditolak.
- `Option<T>` = `Some(T) | None`; `Result<T, E>` =
  `Ok(T) | Err(E)`.

## 7. Pattern matching

- `match` adalah STATEMENT atas nilai subject yang dievaluasi
  tepat sekali; arm pertama yang cocok menang, sisanya tak
  dieksekusi.
- Pola: literal int/bool/string (kesetaraan nilai; string
  dibanding isi), wildcard `_`, binding `n`, pola struct
  (`Titik { x: 0, y }`), pola varian (`Some(v)`, `Ok(v)`),
  dan guard `pola if kondisi-bool`.
- `match` atas enum MUST eksaustif (mencakup semua varian
  atau wildcard); ketidak-eksaustifan MUST ditolak. Arm
  ber-guard tak menyumbang eksaustifan.
- Operator `?` pada `Result` = return-dini `Err` ke pemanggil.
- Detail: `docs/pattern-matching.md`.

## 8. Memory model

- Setiap nilai dimiliki tepat satu pemilik pada satu waktu;
  assignment, pengiriman argumen, dan return MEMINDAHKAN
  kepemilikan (§9).
- NOL garbage collector. Memori yang tak lagi terjangkau
  dan tak dibebaskan = bocor-by-design dan TIDAK PERNAH
  direklamasi (kontrak, bukan bug).
- Nilai `string` immutable; operasi string tersedia via
  standard library. Byte NUL (`\0`) di dalam nilai string =
  undefined (Appendix A).
- Detail: `docs/memory-model.md`.

## 9. Ownership

- Move semantics: setelah nilai dipindah, variabel sumber
  MUST NOT dipakai lagi — pemakaian setelah-move MUST
  ditolak sebelum eksekusi.
- NOL borrow checker eksplisit, NOL lifetime annotation:
  aturan ini ditegakkan murni lewat analisis use-after-move.
- Fungsi `extern` yang mendeklarasikan tipe kembalian string
  MUST ditolak (kepemilikan lintas batas tak terdefinisi).

## 10. Concurrency

- Primitif konkurensi = thread/task BLOCKING via standard
  library (`std/thread.ns`, `std/tasks.ns`); NOL eksekusi
  async (§11).
- Urutan interleave thread = undefined; program yang
  outputnya bergantung pada penjadwalan adalah
  non-deterministik (BUKAN undefined behavior — output yang
  teramati MUST tetap berasal dari eksekusi valid).
- Timeout/retry/discovery = pola library di atas primitif
  blocking, BUKAN primitif bahasa.
- Detail: `docs/concurrency.md`.

## 11. Async

- NOL eksekusi asinkron di bahasa ini. Kata `async`/`await`
  adalah reserved tanpa sintaks: setiap pemakaian MUST
  ditolak sebelum eksekusi.
- Kebutuhan async (server, timeout, retry) dipenuhi pola
  blocking + thread/task (§10) dan paket ecosystem.
- Mengandalkan eksekutor/future/event-loop = undefined.

## 12. Modules

- Modul = file sumber. `import nama;` membuat item publik
  file `nama.ns` yang resolvable tersedia; import yang tak
  resolvable MUST ditolak.
- File tanpa `fn main` = library: `check` langsung atasnya
  MUST gagal TEPAT dengan `no 'fn main' found` (terkunci
  `e2e_compat.sh`).
- Siklus import MUST ditolak.
- Detail: `docs/modules.md`.

## 13. Packages

- Paket = direktori bermanifest dengan metadata (nama,
  versi, dependensi). Perintah paket yang observable:
  `pkg init`, `pkg publish`, install, `pkg update`,
  `verify`, `outdated`, `audit`, `tree`.
- Instalasi paket terverifikasi checksum: paket yang
  dimodifikasi setelah publikasi MUST gagal `verify` dengan
  baris `FAIL <nama-paket>`.
- Perbandingan versi monoton untuk deteksi `outdated`.
- Registry bersifat file-backed dan lokal; NOL akses network
  implisit.
- Detail: `docs/pkg.md`, `docs/ecosystem.md`.

## 14. Foreign function interface

- Blok `extern` mendeklarasikan fungsi asing berlinkage-C
  dengan tipe parameter/kembalian yang didukung
  (`i64`/`bool`/string-masuk/void); ketidakcocokan linkage
  MUST ditolak.
- Tipe kembalian string DILARANG (§9).
- Fungsi asing yang membutuhkan kapabilitas native WAJIB
  dideklarasikan di paket bermanifest `native=true`, jika
  tidak `pkg build` MUST menolaknya (terkunci `e2e_sec.sh`).
- Tarball paket yang memuat path `../` MUST ditolak bersih
  tanpa file tertanam di luar direktori paket.
- Detail: `docs/ffi.md`.

## 15. Compile-time execution

- NOL eksekusi fungsi sewenang-wenang saat kompilasi.
  `const Nama: Tipe = <ekspresi-konstan>;` adalah satu-satunya
  evaluasi statically: inisialisasi MUST ekspresi konstan
  (literal dan operator atas konstanta); selain itu MUST
  ditolak.
- NOL macro-expansion-time function call, NOL build-script
  dalam bahasa ini.

## 16. Macros

- NOL fasilitas macro. Kata `macro` adalah reserved tanpa
  sintaks: setiap pemakaian MUST ditolak sebelum eksekusi.
- Pengusul macro WAJIB melalui proses RFC; sampai
  diratifikasi, kode yang menyerupai definisi/pemanggilan
  macro MUST ditolak.

## 17. Error handling

- Gaya penanganan: enum `Result`/`Option` + operator `?`
  (§6–§7); NOL exception, NOL stack unwinding.
- Setiap diagnostik kompilasi: `file:line:col: error:` +
  pesan + snippet/caret + opsional `help:`, seluruhnya bahasa
  Inggris. Recovery per top-level dengan batas 25 error per
  tahap; compiler MUST keluar != 0 tanpa sinyal pada input
  apapun yang ditolak; hang DILARANG.
- Kesalahan runtime yang terdeteksi (mis. pembagi-nol
  bernilai-variabel) MUST menghentikan program dengan pesan
  `runtime error: …`, bukan melanjutkan dengan nilai fiktif.
- Detail: `docs/error-handling.md`.

## 18. Runtime semantics

- Eksekusi bermula di `fn main`; pernyataan dieksekusi
  berurutan; pemanggilan mengeksekusi body kalengan dengan
  argumen yang sudah dinilai. Urutan penilaian subekspresi
  dalam satu ekspresi = unspecified — program MUST NOT
  bergantung padanya.
- Aritmetika `i64` = two's complement 64-bit WRAPPING
  (overflow melilit, teramati). Pembagian/modulo dengan
  pembagi-nol: ditolak saat kompilasi bila konstan,
  `runtime error` bila variabel.
- Output teramati program = byte yang ditulis ke stdout;
  program sukses keluar dengan kode 0.
- Program yang terdefinisi (tanpa undefined Appendix A dan
  tanpa nondeterminisme §10) MUST menghasilkan output
  identik pada setiap eksekusi.

## 19. Platform semantics

- Semantik bahasa TAK bersyarat platform: program
  terdefinisi berperilaku identik di semua target yang
  didukung. Lebar integer tetap 64-bit di semua platform.
- Perbedaan antar-platform TERBATAS pada lapisan di luar
  bahasa: ketersediaan perintah sistem, path, dan kapabilitas
  native paket — semuanya dilaporkan via error biasa, bukan
  perubahan makna program.
- Representasi antara hasil kompilasi (file objek/artefak)
  BUKAN kontrak bahasa dan MUST NOT diandalkan program.

## 20. Conformance

- Kesesuaian implementasi dikunci uji yang WAJIB hijau:
  `compiler/tests/e2e_spec3.sh` (dokumen ini),
  `compiler/tests/e2e_spec2.sh` (riwayat 2.0),
  `compiler/tests/e2e_compat.sh` (87 file korpus),
  `compiler/tests/e2e_freeze.sh` (freeze 6 area).
- Pembagian dokumen: definisi bahasa = dokumen ini
  (`docs/language-spec-3.0.md`); implementasi compiler =
  `docs/compiler.md`; spesifikasi standard library =
  `docs/standard-library.md`.
- Setiap bagian §1–§19 menunjuk dokumen rinci dan/atau file
  uji pengunci (`Tests:`); pointer yang dangling = gagal
  conformance.
- Aturan independensi (§21): revisi dokumen ini MUST NOT
  memperkenalkan istilah/artefak detail implementasi.

## 21. Aturan independensi implementasi (mengikat revisi)

- Dokumen ini MUST NOT menyebut: versi/merk backend atau
  toolchain, nama file sumber compiler, representasi antara
  (termasuk teks/nama simbolnya), pipeline internal, atau
  API internal compiler.
- Perilaku WAJIB dirumuskan hanya via permukaan observable:
  teks sumber diterima/ditolak, kode keluar, byte stdout/
  stderr, dan file yang dinyatakan sebagai output perintah.
- Daftar istilah terlarang (non-ekshaustif) ditegakkan
  `e2e_spec3.sh`: `llvm`, `clang`, `verifyModule`,
  `codegen`, `typechecker`, `parser.cpp`, `lexer.cpp`,
  ekstensi artefak antara, dan contoh simbol internal.

## Appendix A. Undefined behavior (non-ekshaustif)

- Mengandalkan apa pun di luar §1–§19 (mis. GC, `&&`,
  float, `break`, `+=`, metode generik, supertrait, `async`,
  macro, eksekusi-comptime, borrow checker).
- Isi NUL di dalam nilai string; literal ≥ 2^63.
- Urutan interleave thread; nilai PRNG; waktu eksekusi.
- Kapan memori bocor-by-design direklamasi (tidak pernah).
- Representasi antara hasil kompilasi; format baris output
  perintah introspeksi leksikal untuk input invalid.
