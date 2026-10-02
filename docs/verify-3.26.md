# Compiler Formal Verification — Milestone 1: Properti Metamorfik (Phase 125)

Status: slice verifikasi (target 3.26.0). Mandat master: verifikasi
bagian compiler formal; prioritas parser/typechecker/MIR-verifier/
ownership/IR-invarian; pakai property-based testing, model checking
bila cocok, bukti formal bila feasible; JANGAN seluruh compiler
sekaligus.

## 1. Cakupan: transparansi-semantik formatter (metamorfik)

Properti: `dump(parse(f)) == dump(parse(fmt(f)))` — byte-identik.
Mengapa ini "verifikasi", bukan uji biasa:

- Tanpa oracle eksternal (kelemahan klasik testing): properti
  menghubungkan DUA subsistem independen (fmt token-based vs
  parser precedence-climbing). Menangkap: token hilang/tertukar/
  string rusak/gagal-parse pasca-fmt. TIDAK menangkap: bug
  spacing murni (spacing tak masuk AST — cakupan `fmt --check`
  korpus, bukan properti ini; dinyatakan eksplisit).
- Himpunan uji = SELURUH korpus (87 file) + 25 program fuzz
  berseed (bentuk di luar korpus) + determinisme dump.
- Prasyarat yang dikunci dulu: `nusa ast` deterministik
  (kasus `fmtprop-det` — dump tak-stabil = properti nir makna).

## 2. Evaluasi metode formal (jujur)

- Property-based testing: DIPAKAI (infrastruktur fuzz + LCG
  Phase 71 + properti ini). Alat tepat untuk invarian pipeline.
- Model checking: BELUM COCOK — butuh model eksekutabel
  (Promela/TLA+) atas checker/semantik yang belum ada; menulisnya
  = proyek sendiri (follow-up bila invarian kritis terpilih).
- Bukti formal (proof assistant): BELUM FEASIBLE — tanpa
  Coq/Lean/Isabelle di CI Termux + tanpa spesifikasi mekanis;
  klaim "terbukti" hari ini = dusta. DITOLAK untuk slice ini.

## 3. Batas properti (dinyatakan, bukan disembunyikan)

- Buta terhadap bug yang SAMA di kedua sisi (keduanya salah
  mengurai konstruksi identik secara konsisten) dan terhadap
  bug spacing murni (di luar AST).
- Tidak mencakup: preservasi tipe (checker), preservasi
  perilaku runtime (differential O0≡O2 yang ada), terminasi.

## 4. Diverifikasi — termasuk SATU TEMUAN NYATA

- Properti MENANGKAP bug pada run pertama: `tests/string2_test.ns`
  (`"  hi"` → `" hi"` oleh cleanup spasi-ganda global yang buta
  string). Akar: pass rapikan `Formatter.cpp` men-collapse spasi
  ganda di SELURUH teks termasuk isi literal (dan komentar).
- Perbaikan (slice ini): pass sadar-string + sadar-komentar
  (`//` disalin verbatim — collapse juga merusak `///` tersejajar
  → keluaran `nusa doc`); regresi `testStringCommentKept`.
- `test_fmt` + `e2e_fmtprop` 3/3 (det + korpus 87 + fuzz);
  `e2e_verify` + `e2e_release` hijau; `.ns` + IR OK.

## 5. Ditunda jujur

- Properti preservasi-tipe/run, model checker, proof assistant,
  invarian ownership/IR-generation (butuh lowering MIR dulu).
