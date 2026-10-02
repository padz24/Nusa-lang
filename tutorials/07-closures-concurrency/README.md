# 07 — Closures & concurrency

Tujuan: lambda `|x|`, capture by-value, `thread_spawn`/`join`,
atomic (pola channel: lihat `examples/concurrency.ns`).
Prasyarat: `tutorials/06-error-handling/`.

```bash
./build/nusa run tutorials/07-closures-concurrency/main.ns
# 3
# 42
# 15
# 42
# 1
```

Aturan: capture SELALU by-value (snapshot per pembuatan closure);
`thread_spawn` butuh `fn() -> i64` ketat (lambda `|| ...` OK);
penjadwalan undefined — sinkronisasi eksplisit via join/channel;
tanpa select/close/async.

Lihat: `docs/closures.md`, `docs/concurrency.md`.
Lanjut: `tutorials/08-package-project/`.
