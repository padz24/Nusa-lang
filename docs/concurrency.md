# Concurrency (Phase 28, v1.9.0)

Model awal Nusa: **threads (pthread) + channel + mutex + atomik**.
Target lanjutan (`async`/`await`) belum.

```nusa
fn work() -> i64 {
    return 40 + 2;
}
fn main() {
    let h = thread_spawn(work);
    print(thread_join(h)); // 42
}
```

## Threads

- `thread_spawn(f: fn() -> i64) -> i64` — luncurkan thread menjalankan
  `f` (fungsi biasa atau closure tanpa parameter); mengembalikan
  HANDLE opak (i64).
- `thread_join(h: i64) -> i64` — tunggu selesai + kembalikan nilai
  `f`; handle direbut kembali (jangan join dua kali).
- Entry adalah trampolin closure: meneruskan capture by-value heap
  (lihat `docs/closures.md`) — aman dibagikan untuk DIBACA.
- Gagal spawn (OOM/pthread) → handle `0`; pakai/`join`/`free`
  atas handle `0`/basi/ganda → `runtime error: invalid handle` +
  exit(1) (fail-fast v2.5.0, bukan diam/UB).

## Channel (`i64`)

```nusa
let c = chan_new();
let h = thread_spawn(|| produce(c)); // kirim dari thread
let sum = chan_recv(c) + chan_recv(c); // FIFO, blokir bila kosong
thread_join(h);
chan_free(c);
```

- `chan_new() -> i64`, `chan_send(c, v)`, `chan_recv(c) -> i64`
  (FIFO tak-terbatas; recv memblokir), `chan_free(c)`.
- Payload `i64`; v2.0 (RFC-0006): channel bertag untuk `bool`
  (`chan_new_bool/send_bool/recv_bool`) dan `string`
  (`chan_new_str/send_str/recv_str`). Tag dicatat di handle dan
  diverifikasi runtime — salah tipe = `runtime error: channel
  type mismatch` + exit(1), bukan garbage. `chan_*` lama = tag 0
  (kompatibel penuh).
- Pola deterministik: jumlahkan hasil N recv (urutan kirim = FIFO),
  sinkronisasi tanpa sleep.
- v2.7.0: `chan_len(c) -> i64` — panjang antre sesaat, non-blocking
  (untuk polling: `while chan_len(c) == 0 {}` lalu `recv`; nilai
  race-tolerant — benar saat dibaca, bisa berubah segera).
- v3.19.0: `chan_new_bounded(cap) -> i64` — FIFO berkapasitas
  (cap ≤ 0 = `runtime error`, bukan rendezvous); `chan_try_send(c,
  v) -> bool` — false bila penuh tanpa menunggu (tak-terbatas =
  selalu true). Backpressure: semaphore + produsen-terkendali
  (lihat `docs/dist-3.19.md`). Bertag = follow-up.

## Mutex

- `mutex_new() -> i64`, `mutex_lock(m)`, `mutex_unlock(m)`,
  `mutex_try_lock(m) -> i64` (1 = terkunci sekarang, 0 = sibuk),
  `mutex_free(m)`.
- Nusa tanpa interior mutability: mutex berguna untuk menyerikan
  EFEK (cetakan, kirim channel) dan melindungi handle bersama.
  State bersama yang bermutasi = atomic (di bawah).

## Atomik (`i64`)

- `atomic_new(v) -> i64`, `atomic_get(a)`, `atomic_set(a, v)`,
  `atomic_add(a, delta) -> i64` (nilai BARU), `atomic_free(a)`.
- Sequentially-consistent (`__ATOMIC_SEQ_CST`); counter lintas
  thread deterministik pada join.

## Utilitas

- `sleep_ms(ms)` — tidur mili-detik (jangan dipakai untuk
  sinkronisasi test; pakai join/channel).

## Aturan main (model memori)

- Nilai Nusa = owned; capture closure = salinan heap per-closure.
  Dua thread MEMBACA capture yang sama = aman. Menulis ke capture
  (salinan privat) tak terlihat thread lain — sinkronisasi eksplisit
  (channel/atomic) untuk komunikasi.
- Handle (thread/mutex/chan/atomic) = pointer heap runtime; `join`/
  `free` merebutnya (bukan program-lifetime — dikelola runtime,
  didokumentasikan di sini, pengecualian sadar dari §4).
- Tanpa data race pada primitif bahasa; race hanya mungkin via pola
  unsafe yang didokumentasikan (berbagi handle tulis tanpa lock —
  jangan).

## Implementasi

- `runtime/nusa_rt.c` (pthread + condvar + `__atomic`): dikompilasi
  dan di-link otomatis driver (`clang prog.ll <rt> -o prog -pthread`).
  Penemuan: `<exe>/../runtime/nusa_rt.c`, `./runtime/nusa_rt.c`,
  `$NUSA_PATH`. Tanpa runtime (instalasi parsial) program
  non-concurrency tetap jalan; builtin concurrency gagal link
  dengan pesan clang yang jelas.
- Builtin biasa (reserved, dicek `checkBuiltin`, diemisikan
  `genBuiltinCall` ke `nusa_*`); Bionic + glibc didukung.
- ABI thread: trampolin `(ptr env) -> i64` dipanggil sebagai start
  routine pthread (LP64: arg + return di x0; cast eksplisit).

## Batasan Phase 28 (dipertahankan di 2.0)

- Tanpa `select`/close semantics (`recv` blokir selamanya bila
  kosong — desain agar tak deadlock);
- tanpa thread pool, tanpa scoped threads, tanpa `async`/`await`,
  tanpa TLS, tanpa prioritas/scheduling API;
- tanpa deteksi deadlock/data-race statis (aturan di atas ditegakkan
  konvensi + review, bukan checker).
