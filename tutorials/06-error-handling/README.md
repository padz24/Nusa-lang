# 06 — Error handling (Option/Result 2.0)

Tujuan: enum natif `Option`/`Result`, konstruktor eksplisit,
kombinator, operator `?`, `match` atas varian.
Prasyarat: `tutorials/05-match-enum/`.

```bash
./build/nusa run tutorials/06-error-handling/main.ns
# 42
# 0
# 21
# true
```

Aturan 2.0 (BREAKING dari struct 1.x — lihat
`docs/migration-2.0.md`): `Option<i64>.None()`,
`Result<i64, string>.Ok(v)` / `.Err(e)`; helper `some(v)` inferensi;
`?` hanya atas `Result` yang sama (operan = return); field
`.value`/`.has_value`/`.is_ok` sudah tidak ada (pakai
match/kombinator).

Lihat: `docs/error-handling.md`, `docs/migration-2.0.md`.
Lanjut: `tutorials/07-closures-concurrency/`.
