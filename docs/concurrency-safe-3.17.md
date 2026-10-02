# Safe Concurrency Model — Milestone 1: Klaim-Join Atomik (Phase 116)

Status: slice implementasi (target 3.17.0). Mandat master:
data-race resistance, semantik send/sync, thread safety,
ownership lintas-task, shared state aman; memori + konkurensi
tak boleh bertentangan.

## 1. Model yang diformalkan (share-nothing + handle)

- Semua nilai Nusa adalah Send-by-move: capture closure
  by-value, argumen/return by-value, channel menyalin i64 —
  TAK ADA memori bersama yang bisa di-race dari bahasa.
- Berbagi eksplisit HANYA via handle (thread/mutex/chan/atomic)
  + registry lifecycle: pakai-selalu-hidup, join/free tepat
  sekali; salinan tak memperpanjang lifetime. Aturan ini SATU
  sistem untuk memori dan konkurensi (bukan dua yang bertentangan).
- Semantik send/sync ala-Rust DINYATAKAN tak perlu: checker
  tambahan akan melarang pola yang hari ini sound (handle
  memang dirancang dibagikan). Bila pola berbagi baru muncul
  (select/close, actor), keputusannya dicatat saat itu.

## 2. Bug nyata yang ditutup (TOCTOU join)

`nusa_thread_join` dulu check → pthread_join (blokir) → remove +
free: double-join KONKUREN sama-sama lolos cek → pthread_join
ganda (UB) + double-free + panggil kedua dapat 0 diam-diam
(terbukti 3/3 run pra-fix: `{0,7}` rc=0). Semua `*_free`
sudah claim-first sejak Phase 54 — join satu-satunya yang tidak.

Perbaikan (`runtime/rt_thread.c`, runtime-only, NOL ABI):
klaim atomik DULU (`hremove`, kalah = `invalid handle` exit 1),
baru pthread_join + free. Sekuensial identik (pesan/rc sama);
konkuren: tepat satu pemenang, yang kalah fail-fast, tanpa UB.

## 3. Diverifikasi

- `e2e_stress` + `s_racejoin` (3x: rc=1 + pesan klaim + tanpa
  baris `0` + tanpa sinyal); concurrency/memsafe/async/stress
  + `e2e_release` hijau; `.ns` + IR OK (NOL jalur compiler).

## 4. Ditunda jujur

- Free-while-borrowed konkuren (pakai-vs-destroy balapan):
  tetap UB — butuh refcounting (desain tersendiri).
- ABA reuse alamat (Phase 115); select/close channel; pola
  berbagi baru mengikuti fitur baru.
