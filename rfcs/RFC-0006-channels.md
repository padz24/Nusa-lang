# RFC-0006: Tagged channels

- Status: Implemented
- Penulis: Nusa team
- Tanggal: 2026-09-08
- Menggantikan: —
- Digantikan oleh: —

## Summary

Channel untuk `bool`/`string` di samping `i64` TANPA generik
statis: tag tipe dicatat di handle saat create (`chan_new_bool`,
`chan_new_str`) dan diverifikasi runtime di send/recv (mismatch =
`runtime error: channel type mismatch` + exit 1). `chan_*` lama =
tag 0 (kompatibel penuh). Terimplementasi di 2.0.

## Motivation

Pola request/response dan pipeline string/bool antar-thread
membutuhkan channel non-i64; monomorphisasi statis (`chan<T>`)
terblokir dua hal: (a) tanpa propagasi tipe-ekspektasi, `let c =
chan_new()` tak bisa mengikat `T`; (b) turbofish untuk builtin
tak ada di grammar. Tag dinamis memberi 90% nilai dengan 10%
risiko.

## Design

- Tag: 0 = i64, 1 = bool (0/1), 2 = string (pointer).
- Runtime: `nusa_chan_new_tagged(tag)`; `send_tagged`/`recv_tagged`
  memverifikasi tag handle; `nusa_chan_new/send/recv` lama =
  wrapper tag-0 (ABI sumber kompatibel).
- Checker: 6 builtin bertipe statis (`chan_send_str(h, s: string)`…);
  codegen: zext/trunc bool, ptrtoint/inttoptr string.
- Mismatch = abort bersih (cermin OOB/div-0), BUKAN garbage.

## Alternatives

- `chan<T>` statis: ditolak untuk 2.0 (butuh propagasi ekspektasi +
  turbofish builtin — RFC lanjutan bila dibutuhkan).
- Union tak-bertanda (C-style): ditolak (baca-silang-tipe diam =
  segfault misterius; tag menutupnya).
- `select`/close: tetap di luar lingkup (ortogonal).

## Compatibility

Aditif murni (6 builtin baru; perilaku 6 lama tak berubah —
terkunci e2e lama yang tetap hijau).

## Tests

Unit builtins (IR declares + 4 penolakan tipe); `e2e_concurrency`
v7 (string/bool/i64) + e6 (mismatch runtime).

## Specification

Spec 2.0 §11; `docs/concurrency.md` (tabel tag).

## Discussion

- 2026-09-08: diterima langsung (keputusan ekspansif 2.0).
  `RLIMIT_AS` tidak dipakai untuk enforcement (temuan Phase 42);
  tag ≠ batas memori (polling VmRSS sandbox tak berubah).
