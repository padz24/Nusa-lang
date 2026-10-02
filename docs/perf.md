# Performance — Benchmark & Optimasi (Phase 34, v1.15.0)

Prinsip: **tanpa optimasi tanpa benchmark**. Semua angka di bawah
diukur di Android AArch64 (Termux, clang 21.1.8) via `bench/` +
`EPOCHREALTIME`; ulangi ≥3x (selang-seling bila membandingkan)
karena noise termal.

## Anatomi waktu build (fib.ns, O0)

| Tahap | Waktu | Bagian |
|---|---|---|
| frontend + IR (`--emit-ir`) | ~0.21 dtk | ~10% |
| kompilasi `runtime/nusa_rt.c` | ~0.76 dtk | ~35% |
| link + codegen clang (total `build`) | ~2.2 dtk | 100% |

File besar (`perf2000.ns`, 2000 let): frontend ~0.41 dtk —
frontend BUKAN bottleneck. clang mendominasi (>85%).

## Optimasi yang diambil (berbukti)

1. **Runtime precompiled** (`build/nusa_rt.o`, target CMake
   `nusa_runtime`; driver memakai `<exe>/nusa_rt.o` bila ada,
   fallback ke `.c`): build fib 2.21 → 1.40 dtk (**−36%**,
   stabil 4 pasang selang-seling). Tanpa perubahan perilaku
   (objek identik yang dulu dikompilasi per build).
2. **`-Wno-override-module`** di link: hilangkan noise warning
   triple (kosmetik, nol risiko).
3. **Flag `-O0..-Oz`** (`nusa build -O2`, terus ke clang; default
   O0 = perilaku lama): O2 mengecilkan exe fib 17880 → 16272
   byte (−9%); runtime fib(30) 88 → 80 mdtk, loopsum 2M iterasi
   58 → 62 mdtk (noise — tak ada kemenangan runtime signifikan
   pada IR Nusa saat ini).

## Yang SENGAJA tidak diubah (berbukti tak worth it)

- **Micro-opt frontend** (lexer/parser/checker): <15% build —
  Amdahl menolak; kompleksitas tak terbayar.
- **Default `-O2`**: kemenangan runtime ~0–10% vs risiko waktu
  build + perilaku; tetap opt-in eksplisit.
- **Incremental compilation** = Phase 35 (bukan di sini).

## Cara ukur ulang

```sh
bash bench/bench.sh build/nusa            # build time + ukuran
# runtime: bench/programs/*.ns, bangun O0 vs -O2, time manual
```

Program bench: `fib` (rekursi), `loopsum` (loop 2M),
`strcat` (alokasi), `matchdispatch` (rantai if desugar),
`closureapply` (trampolin). Baseline runtime (O0):
fib(30) ~88 mdtk, loopsum ~58 mdtk (angka acuan, bukan janji).
