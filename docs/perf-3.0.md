# Performance Validation — Suite Resmi (Phase 86, beta 3.0.0)

Mandat: ukur 8 dimensi vs baseline relevan; NOL klaim tanpa
angka. Suite: `bash bench/suite.sh <nusa-binary>` (semua di
TMPDIR; deterministik; tanpa network). Baseline relevan =
2.30.1 (`docs/stability-2.30.md` §6) + beta Phase 84.

## 1. Kedelapan dimensi (metode + satuan)

| # | Dimensi | Metode | Satuan |
|---|---|---|---|
| 1 | compile time | `nusa build` hello/nusa10/perf2000(gen)/parse.ns | s (wall) |
| 2 | incremental compile | build hello dingin vs hangat-best-of-3 (cache) | s + speedup |
| 3 | binary size | `wc -c` exe + biner compiler | bytes |
| 4 | startup time | exe hello 5× langsung, ambil min | ms |
| 5 | runtime performance | `nusa bench -n 3` 5 program bench/ | ms (min/med/max) |
| 6 | memory usage | maxRSS anak dari `nusa bench` (§5) | KiB |
| 7 | package resolution | fixture app→lib: `pkg update`+`verify`+`tree`+`build` | s (wall) |
| 8 | LSP latency | initialize→respons Content-Length pertama (cold, +spawn) | ms |

## 2. Aturan baca angka

- Deviasi antar-run wajar di perangkat yang sama (beban
  paralel menaikkan wall-time; Suite BUKAN gate
  perbandingan-ketat — gate = selesai + invarian
  (`warm <= cold`, ukuran > 0, 8/8 dimensi hadir).
- Klaim "lebih cepat" DILARANG tanpa dua angka berdampingan
  di tabel §4 + kondisi ukur identik.

## 3. Conformance

`compiler/tests/e2e_perf.sh`: suite rc 0 + 8/8 dimensi +
invarian incremental + ukuran positif.

## 4. Hasil ukur (beta 3.0.0; Termux AArch64; rentang 2 run)

| Dimensi | Beta 3.0.0 | 2.30.1 (baseline) |
|---|---|---|
| compile hello / nusa10 / perf2000 / parse.ns | 1.58 / 1.46 / 24.76 / 2.28 s | (closureapply 1.91–2.06 s; parse 3.4–3.7 s) |
| incremental hello dingin → hangat | 1.54 → 0.28 s (≈5.5×) | — (baru diukur) |
| exe hello / parse; compiler | 5816 / 96576 B; 1624040 B | 6776 (closureapply); 1522248–1623920 B* |
| startup hello (min 5×) | 42.8 ms | — (baru diukur) |
| runtime median fib/loopsum/strcat/match/closure | 50.8 / 80.8 / 57.5 / 71.1 / 128.8 ms | (closureapply run 0.15–0.17 s ≈) |
| maxRSS fib/loopsum/strcat/match/closure | 5976 / 5952 / 6208 / 6068 / 6712 KiB | (bench hello 5904 KiB ≈) |
| pkg tree / build (scaffold cli-app) | 0.24–4.35 / 1.41–5.86 s† | — (baru diukur) |
| LSP initialize cold (+spawn) | 171–209 ms | — (baru diukur) |

`*` Biner tumbuh 1522248→1624040 B antar-fase = kode baru
(`migrate` Phase 83, store F4 Phase 85); BUKAN regresi.
`†` pkg sensitif cache/registry-TMPDIR; rentang jujur 2 run.
Tanpa klaim "lebih cepat": semua ≈ baseline dalam varian
perangkat.
