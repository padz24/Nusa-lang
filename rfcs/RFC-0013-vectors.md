# RFC-0013: Vector/SIMD types

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-22
- Menggantikan: —
- Digantikan oleh: —

## Summary

Menambahkan tipe vektor fixed-width `vector<T, N>` (slice 1: `T =
i64`, `N` ∈ {2, 4, 8, 16}) yang diturunkan ke LLVM vector types,
dengan arith elementwise, komparasi, load/store/index, dan reduksi
horizontal. RFC ini Proposed; TANPA implementasi di slice ini
(aturan: fitur bahasa via RFC dulu; butuh juga keputusan ABI di
bawah). Slice disengaja minimal agar dapat diuji tuntas sebelum
mask/boolean-vector/float (Nusa belum punya float — F2 3.0).

## Motivation

Bukti kebutuhan (bukan asumsi): loop Nusa pada `-O2` mengeluarkan
NOL instruksi SIMD — diverifikasi pada loop reduksi skalar dan
`array_sum` (`--emit-asm`, grep `q[0-9]/v[0-9].*`: 0). Penyebab:
alloca skalar + pointer opak + dependensi bawa-loop; autovektorisasi
LLVM tak tersentuh pola IR hari ini. Jalur jujur ke SIMD adalah
tipe vektor eksplisit (programmer opt-in, compiler menurunkan
1:1), BUKAN janji "LLVM akan menanganinya". Kasus: agregasi
numerik, DSP mini, pra-pemrosesan data (`csvsum`-kelas), grafika
2D integer.

## Design

Sketsa normatif-informal (slice 1):

```nusa
let v: vector<i64, 4> = vector.fill(7);  // [7,7,7,7]
let w: vector<i64, 4> = vector.from4(1, 2, 3, 4);
let s: vector<i64, 4> = v + w;           // elementwise + - *
let t: i64 = vector.sum(s);              // reduksi horizontal
let x: i64 = s[2];                       // index konstan/variabel
```

Aturan (mengikat bila Accepted):

1. Sintaks tipe `vector<T, N>`: `T` = `i64` (slice 1), `N` =
   literal 2/4/8/16. `vector` menjadi keyword tipe (nol pemakaian
   korpus — hanya `std::vector` di C++ bench, bukan `.ns`).
   N general/bukan-literal DITOLAK (monomorphisasi butuh layout
   statis; cermin cap-64 generik).
2. Semantik nilai (copy) seperti struct: parameter/return
   by-value; tanpa aliasing tersembunyi (selaras owned-values).
3. Operasi: `+ - *` elementwise (div menyusul — semantik
   div-nol per-lane butuh keputusan), `==`/`!=` → `vector<bool>`
   DITUNDA (tanpa bool-vector slice 1; komparasi via
   `vector.any_eq(v, x)` helper?), index baca `v[i]` (OOB =
   fail-fast cermin array), `vector.fill/sum/min/max`.
4. Penurunan LLVM: `<N x i64>`, alignment 16, operasi =
   instruksi vektor natif (`add <4 x i64>`). Tanpa fallback
   skalar diam-diam: target tanpa SIMD → error compile eksplisit
   (keputusan portabilitas, bukan UB senyap).
5. ABI: vektor by-value mengikuti PCS target untuk tipe vektor
   C sepadan (AArch64: register SIMD untuk N kecil) —
   DIDOKUMENTASIKAN per-target di `docs/abi-*.md` saat Accepted
   (risiko: divergensi x86_64 vs AArch64 untuk N besar).
6. Determinisme: operasi murni, urutan lane tetap, hasil
   bit-identik antar-run (syarat reproducible-builds).

Di luar slice 1 (RFC lanjutan): float (butuh tipe float dulu —
F2), bool-vector/mask, lane-div, shuffle/blend, gather/scatter,
intrinsik bernama vendor (DITOLAK permanen sebagai permukaan
bahasa — vendor di paket, bukan inti).

## Alternatives

- Builtin intrinsik (`simd_add4(...)`): ditolak (permukaan
  kombinatorial, tak terkomposisi, tiap backend beda nama).
- Hanya autovektorisasi: ditolak BERDASAR BUKTI (0 SIMD di
  keluaran `-O2` hari ini; memperbaikinya butuh model alias/TBAA
  atas model memori baru — proyek tersendiri yang lebih besar).
- Array + syntax-sugar: ditolak (array fixed-size + tanpa
  operasi elementwise + layout beda `[N x i64]` vs `<N x i64>`).
- Tanpa SIMD selamanya: opsi sah — RFC ini boleh di-Reject bila
  beban ABI/portabilitas dinilai lebih besar dari manfaat.

## Compatibility

Aditif: keyword `vector` baru (nol korpus; diverifikasi grep),
konstruktor `vector.fill` sebagai namespace fungsi (bukan
keyword baru). IR baru (`<N x i64>`) hanya muncul bila tipe
dipakai; ABI lama tak berubah. Migrasi: tidak perlu.

## Tests

Unit: lexer (keyword), parser (bentuk + 5 penolakan: N
non-literal/bukan-{2,4,8,16}, T bukan-i64, payload campur),
semantic (unknown/arity), typecheck (elementwise sejenis,
index-OOB-statis, sum→i64), codegen (bentuk `<4 x i64>` +
`add <4 x i64>` di IR). E2E `e2e_simd.sh`: runtime 4 nilai
(fill/arith/index/sum), 5 penolakan, `--emit-asm` mengandung
register vektor (bukti mesin, bukan klaim), fuzz 20 program
bervektor, bench skalar-vs-vektor TERUKUR (bukan janji).

## Specification

Bila Accepted: `docs/language-spec-3.0.md` + EBNF tipe
(`VectorType`), § semantik nilai/operasi, § ABI vektor
per-target; `docs/abi-3.10.md` + baris vektor.

## Discussion

- 2026-09-22: Proposed tanpa implementasi (Phase 122,
  milestone 1). Accepted menunggu: (a) review sintaks N-sebagai-
  nilai (preseden pertama value-arg di posisi tipe); (b) keputusan
  ABI per-target; (c) bukti backend: prototipe codegen
  `<4 x i64>` + run benar di AArch64 SEBELUM janji slice.

## Security

Tanpa permukaan baru: tanpa IO, tanpa input eksternal, tanpa
alokasi dinamis (nilai register/stack), OOB di-fail-fast-kan.
Risiko satu-satunya = divergensi antar-target (butir 5) —
dimitigasi dokumentasi + penolakan eksplisit, bukan perilaku
diam-diam.
