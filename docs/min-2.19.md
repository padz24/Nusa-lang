# Mobile / Embedded — Design (Phase 68, target 2.19.0)

Status: audit → Implementation (milestone 1: biner minimal).
Spec tak berubah. Cocok model memori (tanpa GC/heap user =
tanpa runtime bagi program murni).

## 1. Audit ukuran (hello, 2.18.0)

Exe 22288 B = text 13304 + data/bss + header (±6KB); text
didominasi `nusa_rt.o` utuh (16.7KB: pthread/socket/channel
yang tak dipakai hello). Terukur: gc-sections → 8376 (−62%);
tanpa runtime → 5816 (−74%).

## 2. Milestone 1 (perilaku program TETAP)

1. `nusa_rt.o` dikompilasi `-ffunction-sections -fdata-sections`.
2. Link selalu `-Wl,--gc-sections` (Bionic/glbc ld modern;
   toolchain pin clang 21.1.8).
3. Runtime dilewati TOTAL bila IR tak merujuk simbol fungsi
   runtime (`@nusa_thread_|mutex_|chan_|atomic_|sleep_|time_|
   random_|process_|tcp_` — presisi `@`, tidak tertipu string/
   global `argc/argv`). Arah aman: positif-palsu = link seperti
   dulu; negatif-palsu mustahil (ref → simbol).
4. `-pthread` dipertahankan selalu (nol risiko).

## 3. DITUNDA

Freestanding penuh (tanpa libc/startup), LTO, UPX/packing,
`--no-runtime` eksplisit (inferensi cukup), anggaran memori
statis, startup terukur terpisah (sudah ~ms).

## 4. Acceptance criteria

- hello < 9KB, concurrency < baseline-2.18; run identik.
- `e2e_min.sh` hijau (batas byte + fungsional + fallback
  tanpa-`.o` tetap minimal) + suite hijau; bench identik
  (kecuali ukuran mengecil).
