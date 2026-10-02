# Profiler & Observability — Design (Phase 65, target 2.16.0)

Status: audit → Implementation (milestone 1: `bench` + build
`--stats`). Spec tak berubah; tooling. Prinsip repo: angka resmi
tetap via `bench/` + EPOCHREALTIME (`docs/perf.md`); perintah baru
untuk iterasi harian.

## 1. Milestone 1

**`nusa build --stats`**: rincian waktu compile (ms) —
`frontend` (checkStages), `codegen` (generate+emit IR),
`clang`, `total` — + ukuran (`ir` + `exe` bytes). Ke stderr
(saluran log build). Hanya bila build sukses penuh; emit-only
(`--emit-ir`/`-c`/cross-gagal) tak mencetak (bukan build penuh).

**`nusa bench <file.ns> [-n N] [-- args...]`** (default N=5):

- Build `--fresh` sekali ke temporer (gagal → rc + pesan build).
- N run via fork+wait4: waktu dinding per run (ms, 1 desimal) +
  maxRSS anak (`ru_maxrss`, KiB, `RUSAGE_CHILDREN`).
- Output program diteruskan (stdio diwarisi, cermin `time`);
  statistik ke stderr: `runs`, `min/median/max ms`,
  `maxrss KiB` (maks antar-run).
- N invalid (<1 atau bukan angka) → error jelas. rc = rc program
  (non-nol bila program gagal — bench program rusak = gagal).

**`pkg build --stats`**: teruskan ke runBuild (satu baris).

## 2. DITUNDA beralasan

Sampling CPU profiler, alokasi/tracing runtime (butuh counter di
`nusa_rt.c` + ABI), `trace` eksekusi, flamegraph, LSP/daemon
stats — masing-masing milestone (butuh desain format + overhead
terukur). `nusa test --stats` follow-up kecil.

## 3. Acceptance criteria

- `e2e_prof.sh`: stats ada + konsisten (total ≥ tiap bagian;
  ukuran = byte berkas); bench 3 run median benar + maxrss>0 +
  output program utuh + N-bad ditolak + program gagal → rc≠0.
- Seluruh suite hijau; bench identik.
