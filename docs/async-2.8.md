# Async Runtime — Design (Phase 57, target 2.8.0)

Status: audit + fondasi → Implementation (milestone 1: `std/tasks`
di atas thread/channel). TANPA sintaks `async`/`await` baru:
keyword tetap reserved, RFC-0004 tetap Proposed, implementasi
sintaks DILARANG sebelum Accepted (kontrak Phase 41). Spec 2.0
tak berubah; murni stdlib + test + docs.

## 1. Posisi desain

Contoh master (`async fn fetch_data`) = tujuan bahasa, BUKAN
fase ini. Fase ini = runtime foundation yang kelak diekspos
sintaks: Future, Task, Executor (pola), dengan lifetime,
cancellation, error propagation, shutdown yang eksplisit —
divalidasi di atas primitif stabil (threads/channels/atomics).

## 2. Milestone 1: `std/tasks.ns` (murni Nusa, teruji prototipe)

```nusa
struct Future { chan: i64; thread: i64; }
fn task_spawn(f: fn() -> i64) -> Future  // thread + channel hasil
fn future_poll(f: Future) -> bool        // non-blocking (chan_len)
fn future_await(f: Future) -> i64        // recv + join + free (alih milik)
fn cancel_new() -> i64                   // token atomic
fn cancel(tok: i64) -> void
fn is_cancelled(tok: i64) -> bool
fn cancel_free(tok: i64) -> void
```

- **Task lifetime**: `task_spawn` memiliki thread+channel;
  `future_await` satu-satunya penutup (join + free keduanya).
  Await ganda / pakai pasca-await = `invalid handle` (registry).
- **Cancellation**: kooperatif via token atomic (pekerja polling
  `is_cancelled`; pola spin-tunggu deterministik untuk shutdown).
  Preemptive kill TIDAK ADA (by-design: join-or-cancel).
- **Error propagation**: konvensi nilai — tugas mengembalikan
  kode (`0`/negatif = error, cermin `main` C); pembungkus
  `Result` di sisi await (match, bukan channel).
- **Synchronization**: channel hasil (single-shot) + `chan_len`
  untuk poll; pola fan-out/fan-in warisan `par_map2`.
- **Shutdown**: batalkan token → join semua → free (urutan
  terdokumentasi di contoh; executor = pola, bukan tipe —
  array/N-handle generik mustahil tanpa array-param).

## 3. DITUNDA (butuh RFC/sintaks)

`async`/`await`/`Future<T>` generik sintaks, executor
work-stealing, select, timeout/ cancel-otomatis, error
tertip (`?` lintas task). Semua menunggu RFC-0004 Accepted.

## 4. Acceptance criteria

- `e2e_async.sh` hijau (await, poll, cancel-shutdown
  deterministik, konvensi error, stress 8 task, misuse).
- `nusa doc`/`fmt`/`test` atas `std/tasks.ns` hijau.
- Nama modul `tasks` (bukan `async`): `async` reserved sejak 2.0
  sehingga `import std.async` tak terparse (segmen path butuh
  identifier) — ditemukan saat implementasi; tanpa perubahan
  compiler (pemilihan nama, bukan fix parser).
- Seluruh suite hijau; benchmark identik baseline 2.7.0.
