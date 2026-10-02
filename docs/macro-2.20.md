# Macro System — Evaluasi & Desain (Phase 69, target 2.20.0)

Status: evaluasi SELESAI → TIDAK diimplementasikan (di bawah
ambang kebutuhan; mandat master kondisional: "jika diperlukan").
Yang dikerjakan: reservasi keyword `macro` + kriteria pemicu +
sketsa desain. Spec: +1 reserved word (aditif, non-breaking:
nol pemakaian di korpus).

## 1. Evaluasi kebutuhan (evidence, std/ + compiler)

Kandidat yang diperiksa: wrapper tipis thread/sync/net
(`spawn`/`join`, `lock_new/free`, `dial/close` — masing-masing
1 baris, fungsi biasa cukup); kombinator error/generik
(tercakup generics + traits); pola `par_map2`/executor
(terbatas array-param, BUKAN makro); DSL/config (belum ada
konsumen); derive/clone/debug-print (tanpa refleksi —
makro tak membantu tanpa itu dulu).

Kesimpulan: semua di atas tertangani fungsi/generik/trait/builtin.
BOLEH disappears: nol situs repetisi ≥3 yang menuntut codegen.
Ambang implementasi (salah satu): (a) 3+ kasus std/ekosistem
terdokumentasi yang hanya selesai via ekspansi sintaks;
(b) RFC yang Accepted dengan desain higiene.

## 2. Yang direservasi

`macro` → `TokenKind::Macro` (lexer C++ + selfhost, spell,
diagnostic alami "expected declaration"). Tanpa sintaks —
pemakaian ditolak parser seperti `async` (preseden 2.0).

## 3. Sketsa desain (bila ambang tercapai)

- Deklaratif dulu (`macro name(pattern) => expansion` atau
  setara via RFC); prosedural (compile-time Nusa) HANYA bila
  comptime execution (Phase 70) ada.
- Higiene: identifier introduksi difresh-kan per ekspansi;
  span call-site dipertahankan untuk diagnostic (`file:line:col`
  menunjuk pemanggil, ekspansi dapat di-dump via flag).
- Tooling: ekspansi AFTER fmt (fmt memformat sumber, bukan
  hasil); LSP/diagnostics bekerja pada kode ter-ekspansi
  DENGAN peta balik span; `nusa expand` (debug) mencetak hasil.
- Batas: tanpa akses IO/filesystem saat ekspansi; rekursi
  dibatasi (cermin guard 500); error ekspansi = diagnostic
  biasa, bukan crash (kontrak hardening).

## 4. Acceptance criteria fase ini

- `macro` ter-lex sebagai keyword di kedua lexer; korpus
  hijau (nol pemakaian); spec + docs sinkron; suite hijau.
