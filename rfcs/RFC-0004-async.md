# RFC-0004: Async/await

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-08
- Menggantikan: —
- Digantikan oleh: —

## Summary

Menambahkan komputasi asinkron (`async fn`, `await`, executor)
DI ATAS primitif concurrency sinkron v1.9.0 (threads/channels/
mutex/atomics). RFC ini BELUM disetujui; implementasi dilarang
sebelum status Accepted.

## Motivation

`std.net` (TCP blocking) + `thread_spawn` memaksa model
satu-thread-per-koneksi; server Nusa butuh I/O multiplexing murah
tanpa menaikkan kompleksitas model memori (RFC-0003).

## Design

Sketsa (belum final, terbuka untuk direvisi saat diskusi):

- `async fn fetch(url: string) -> string` = fungsi yang mengembalikan
  future ber-tipe `Future<T>` (struct generik biasa, executor-agnostic).
- `await e` hanya legal di dalam `async fn`/blok async; desugar ke
  state machine (dipertimbangkan: transformasi di checker seperti
  closure-lifting, BUKAN codegen async LLVM).
- Executor awal: thread-pool di runtime C (menggunakan ulang
  primitif `nusa_rt.c`); tanpa work-stealing di fase pertama.
- Aturan memori: future MUST owned + `Send`-like (semua capture
  by-value, konsisten RFC-0003); data-race tetap tanggung jawab
  programmer + sinkronisasi eksplisit.

## Alternatives

- Stackful coroutines (ucontext/fiber): ditolak sementara
  (non-portabel di Bionic; interaksi LLVM tidak ramah).
- Async sebagai library murni (tanpa sintaks): dipertimbangkan
  sebagai prototype wajib SEBELUM sintaks dikunci.
- Structured concurrency (nursery/scope): kandidat kuat untuk
  fase kedua; RFC ini tidak menutupnya.

## Compatibility

Aditif (`async`/`await` menjadi keyword reserved — program yang
memakai sebagai identifier WAJIB dimigrasi; daftar di migrasi).

## Tests

Rencana: prototype library → `e2e_async.sh` (deterministik:
eksekusifuture + interleave terkontrol via channel, bukan timing);
penolakan (`await` di luar async, future tak-owned).

## Specification

Bila diterima: `docs/language-spec-1.0.md` §11 (perluasan) +
dokumen `docs/async.md` baru.

## Discussion

- 2026-09-08: dibuka sebagai Proposed. Pertanyaan terbuka: (1)
  sintaks vs library-dulu? (2) executor default di std atau runtime?
  (3) kanselasi (wajib/tidak di v1)? (4) interaksi dengan `?`
  (propagasi error lintas await)?
