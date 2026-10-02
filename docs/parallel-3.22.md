# Parallel Computing — Milestone 1: Partisi Statik (Phase 121)

Status: slice implementasi level-pustaka (target 3.22.0). Mandat
master: parallel loop, task paralel, work stealing, koleksi
paralel, abstraksi SIMD; vektorisasi compiler bila aman; LLVM
untuk optimasi level-rendah.

## 1. Cakupan: `std/parallel.ns` (murni Nusa, tanpa sintaks baru)

- `par_map_sum(n, w, f)`: jumlah `f(i)`, i in [0,n), atas `w`
  worker (dibatasi 1..n; n≤0 → 0). Partisi BLOCK
  `[n*k/w, n*(k+1)/w)` — statik, tanpa work-stealing.
- Mekanisme: thread per blok (closure capture, pola `par_map2`)
  + channel hasil + recv-semua sebagai barrier efek.
- Deterministik untuk `f` murni (penjumlahan komutatif;
  dibuktikan 3x run = 2646700 eksak).

## 2. Keputusan jujur

- Work stealing: TIDAK (butuh deque + select; follow-up).
  Beban tak-seimbang = blok tak-merata, didokumentasikan.
- Koleksi paralel: array tak bisa lewat batas fungsi (model
  bahasa) → loop atas RENTANG, bukan koleksi.
- SIMD/vektor: Phase 122 (butuh tipe vektor). Vektorisasi
  otomatis LLVM via `-O` = perilaku backend yang ada (tanpa
  klaim khusus pustaka ini).
- Task paralel umum: tercakup `par_map_sum` + `tasks.ns` +
  `par_map2` (komposisi, bukan primitif baru).

## 3. Diverifikasi

- `tests/parallel_test.ns` (sum/sq/clamp/nol); `e2e_parallel`
  3/3 (test-file, determinisme 3x, kanonis + fmt); concurrency/
  stress + `e2e_release` hijau; `.ns` + IR OK (NOL compiler).

## 4. Ditunda jujur

- Work stealing, koleksi paralel sejati (butuh slice bahasa),
  SIMD (122), pool/chunk adaptif.
