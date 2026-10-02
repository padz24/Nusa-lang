# RFC-0012: Declarative macro system

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-20
- Menggantikan: —
- Digantikan oleh: —

## Summary

Menambahkan makro deklaratif (pattern → template) ke Nusa secara
bertahap, dengan desain yang mengutamakan: type safety (ekspansi
dicek penuh seperti kode tulis-tangan), hygiene sederhana,
determinisme, batas keamanan eksplisit, dan keterlacakan
diagnostik. RFC ini Proposed; TANPA implementasi di slice ini
(aturan: fitur bahasa besar via RFC dulu). Ambang Phase 69 tetap
berlaku: implementasi dimulai hanya bila pola boilerplate nyata
tak terpecahkan fungsi/generik/trait/`const`. Makro prosedural
DITUNDA eksplisit (butuh API compiler + sandboxing, Phase 137/138).

## Motivation

Pola berulang yang hari ini disalin-tempel: konstruktor +
kombinator `Option`/`Result` per tipe baru, pasangan
`is_empty`/`len` per koleksi, pembungkus FFI repetitif
(`extern fn` + wrapper + capability), dan kode uji tabular.
Fungsi/generik/trait menutup sebagian (sudah dievaluasi Phase 69:
belum cukup alasan untuk makro SAAT ITU) — RFC ini menyiapkan
desain agar kebutuhan masa depan tak dijawab dengan ekspansi
string ad-hoc (tak higienis, diagnostik rusak, tooling buta).

## Design

Sketsa normatif-informal (satu-satunya bentuk yang diusulkan):

```nusa
macro repeat3($body) {
    $body
    $body
    $body
}

fn main() {
    repeat3!(print("hi"));
}
```

Aturan (mengikat bila Accepted):

1. Definisi `macro NAME($p1, ...) { template }` adalah item
   top-level (keyword `macro` reserved sejak 2.20.0; nol
   pemakaian korpus sebagai konstruksi — hanya tabel keyword
   lexer/selfhost/nstok).
2. Argumen = fragmen sintaks (ekspresi/statement/tipe/item),
   BUKAN teks: parsing argumen memakai parser Nusa yang sama
   (menolak fragmen invalid di situs call, sebelum ekspansi).
3. Substitusi level-AST: template di-parse sekali saat definisi;
   `$hole` diganti klon AST argumen. Tanpa token-pasting mentah
   (tanpa `concat_idents` di slice ini).
4. Hygiene murah-eksplisit: identifier di template resolve di
   DEFINISI kecuali berasal dari `$hole` (resolve di call-site).
   Tanpa capture implisit dari scope call-site (kasus klasik
   `let tmp` bertabrakan = error duplikat biasa, bukan bug
   senyap). Aturan satu kalimat ini diajarkan, bukan alpha-rename
   penuh (kompleksitas ditolak untuk slice 1).
5. Rekursi: makro boleh memanggil dirinya; budget ekspansi =
   kedalaman 64 + total node 65536 (cermin cap 64 generik +
   guard 500 parser — angka final saat Accepted). Lewat =
   error compile bersih, bukan hang.
6. Determinisme: ekspansi murni (tanpa IO/clock/env/acak);
   urutan ekspansi = urutan tekstual; output byte-identik untuk
   input identik (syarat reproducible-builds §61).
7. Diagnostik: tiap error di kode ekspansi membawa STACK ekspansi
   (`expanded from macro 'repeat3!' invoked at file:line:col`);
   `nusa fmt` memformat ulang hasil ekspansi secara idempoten;
   `nusa doc` menampilkan definisi makro + situs pemakaian.

Di luar slice 1 (butuh RFC lanjutan): pattern-matching
multi-arm (`($a, $b) => ...`), repetisi (`$(...)*`), makro
atribut/item, makro prosedural (fungsi compiler ter-sandbox —
prasyarat API compiler Phase 137 + eksekusi terkurung).

## Alternatives

- Ekspansi teks/C-preprocessor: ditolak (tak higienis,
  diagnostik menunjuk kode tak terlihat, fmt/LSP buta).
- Makro prosedural dulu: ditolak untuk slice 1 (permukaan
  keamanan: kode arbitrer saat kompilasi; butuh sandbox +
  API stabil yang belum ada).
- Tak ada makro selamanya: dipertahankan sebagai opsi —
  RFC ini sah di-Reject bila pola nyata tetap terpecahkan
  tanpa makro (ambang Phase 69 tidak kedaluwarsa).
- `comptime` eksekusi fungsi (RFC-0009): ortogonal (ekspansi
  sintaks vs evaluasi nilai); koeksis bila keduanya Accepted.

## Compatibility

Aditif: keyword `macro` sudah reserved (parser menolak alami
hari ini — pesan dipertahankan hingga Accepted). Tanpa makro
yang dipakai = tanpa perubahan perilaku/IR/ABI. Migrasi: tidak
perlu. Ekosistem: registry menandai paket bermakro (capability
baru di manifest, mengikuti preseden `native`) — detail saat
Accepted.

## Tests

Unit: lexer (sudah: reserved), parser (definisi/call valid +
tolak: pola invalid, `$` liar, rekursi-lewat-budget),
semantic (hygiene: capture-tak-diundang ditolak; resolve
definisi vs call-site), typecheck (ekspansi dicek penuh;
mismatch di kode ekspansi menunjuk stack), fmt (idempoten),
doc (render definisi). E2E `e2e_macro.sh`: runtime 3 nilai,
5 penolakan, fuzz 20 program bermakro (oracle tanpa-crash),
benchmark ekspansi-vs-tulis-tangan (overhead kompilasi
terkuantifikasi, bukan klaim).

## Specification

Bila Accepted: `docs/language-spec-3.0.md` + EBNF item
`MacroDecl`/call `!`, § semantik ekspansi (aturan 1–7),
§ hygiene, Appendix batas budget; `docs/macro-2.20.md`
evaluasi → desain.

## Discussion

- 2026-09-20: Proposed tanpa implementasi (Phase 107,
  milestone 1). Keputusan Accepted menunggu: (a) ≥2 pola
  boilerplate nyata dari `realworld/`/ekosistem yang gagal
  dipecahkan fungsi/generik/trait/`const` (bukti, bukan
  selera); (b) review desain hygiene-budget-diagnostik;
  (c) rencana tooling fmt/lsp/doc. Prosedural tetap di luar
  cakupan hingga API compiler stabil.

## Security

Batas konstruksi (bukan konvensi): TANPA IO/network/env/clock/
acak saat ekspansi (tak ada sintaks untuk itu — fragmen murni);
budget kedalaman×node dienforce (anti-bom-ekspansi ala
billion-laughs); makro dari paket registry = kode sumber yang
dibaca saat build (model kepercayaan = dependensi sumber,
cermin Phase 59; kapabilitas manifest menjadikannya eksplisit);
pesan error tak boleh membocorkan path absolut di luar proyek
(relatif seperti diagnostik hari ini). Perluasan APAPUN
(prosedural, akses file, network) = RFC baru + sandbox.
