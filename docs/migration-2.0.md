# Migrasi 1.x → 2.0

Dua breaking change (B1, B2-keywords) + selebihnya aditif. Semua
pola lama yang ditolak memiliki pesan dengan arahan.

## B1: Option/Result struct → enum natif

| 1.x (ditolak di 2.0) | 2.0 |
|---|---|
| `none(0)` / `none("")` | `Option<i64>.None()` / `Option<string>.None()` |
| `ok(5, "")` | `Result<i64, string>.Ok(5)` |
| `err(0, "boom")` | `Result<i64, string>.Err("boom")` |
| `opt_map(o, f, dummy)` | `opt_map(o, f)` |
| `res_map(r, f, dummy)` | `res_map(r, f)` |
| `res_map_err(r, f, dt, df)` | `res_map_err(r, f)` |
| `o.value`, `o.has_value` | `match o { Some(v) => …, None => … }` atau kombinator |
| `r.value`, `r.is_ok`, `r.error` | `match` / `is_ok(r)` / `?` / kombinator |
| `struct Option`/`struct Result` sendiri + pola `Some`/`None` | ganti ke enum (atau ganti nama tipe) |

TIDAK berubah sintaksnya: pola `Some(v)`/`None`/`Ok(v)`/`Err(e)`,
operator `?`, `is_some`/`is_none`/`opt_unwrap_or`/`res_unwrap_or`,
`parse_int_opt`, `file_read_opt`, `Error`/`error()`.

Aturan praktis `None`: tipe eksplisit wajib
(`Option<i64>.None()` — cermin `Point<i64> { … }` dan literal `[]`
yang butuh anotasi). Helper `some(v)` tetap inferensi.

## B2: keyword `async`/`await` di-reserved

Program yang memakai `async`/`await` sebagai identifier (fungsi,
variabel, field, modul) WAJIB diganti namanya. Korpus repo
nol pemakaian (diverifikasi). Tanpa sintaks async di 2.0
(RFC-0004 tetap Proposed).

## Aditif (tanpa migrasi)

- Channel bertag: `chan_new_bool/send_bool/recv_bool`,
  `chan_new_str/send_str/recv_str` (RFC-0006). `chan_*` lama
  identik (tag 0, kini diverifikasi runtime).
- TCP server: `tcp_listen`/`tcp_accept` + `std.net`
  `listen_on`/`accept_one` (loopback-only).
- Bare `return;` di tengah `main` kini legal (dulu gagal
  verifikasi IR — bug, bukan fitur).

## Cek migrasi paket Anda

```
nusa check main.ns        # baca semua error sekaligus (recovery)
nusa test                 # hijau = selesai
nusa fmt --check .        # gaya tetap
```
