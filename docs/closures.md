# Closures & Functional Features (Phase 27, v1.8.0)

Lambda `|params| body` adalah function value yang bisa menangkap
(capture) variabel enclosing:

```nusa
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    let k = 10;
    print(apply(|x| x + k, 1)); // 11
}
```

Body berupa ekspresi tunggal (`|x| x + 1`) atau blok (`|x| { return x + 1; }`,
ekspresi dinormalisasi menjadi `return <expr>;`).

## Aturan tipe

- Parameter boleh beranotasi (`|x: i64| ...`) atau diinferensi dari
  konteks (`fn`-typed: argumen panggilan, `let f: fn... = ...`, return).
  Tanpa keduanya → `cannot infer type of lambda parameter` (anotasi
  atau pakai di posisi bertipe-`fn`).
- Anotasi yang konflik dengan ekspektasi ditolak (`mismatched lambda
  parameter type`); arity harus pas (`lambda expects N ...`).
- Return: ekspektasi konkret dicek normal; selebihnya diinferensi dari
  `return` body (inkonsisten → `inconsistent lambda return types`;
  kosong → `cannot infer lambda return type` + all-paths berlaku).
- Lambda mengalir ke fungsi generik (`opt_map(o, |x| x * 2, 0)`):
  argumen non-lambda mengikat tparams dulu, lambda dicek sesudahnya,
  return-nya ikut mengikat.

## Capture: by-value snapshot (model memori)

- Free vars body yang merujuk nilai enclosing disalin NILAINYA saat
  lambda dibuat (snapshot, bukan referensi).
- Dibuat per EKSEKUSI: lambda dalam loop menangkap nilai iterasi
  berjalan (benar per-iterasi, tanpa gotcha-C).
- Assign ke capture hanya menyentuh salinan privat (outer tak berubah).
- Boleh: `i64`/`bool`/`string`/function values/struct (termasuk enum).
  Ditolak: array (`cannot capture array`; salin elemen ke lokal dulu).
- Self-reference tak bisa (`let f = |x| f(x)` → undefined, cermin
  `let x = x`); rekursi pakai fungsi bernama.
- Nested lambda bisa (inner di-lift saat outer dicek; capture param
  outer dan capture-capture valid).
- Lambda di match guard DITOLAK (`lambdas cannot appear in match
  guards`) — capture akan membaca binding yang belum materialisasi.

## Implementasi: lifting + trampolin seragam

- Tiap lambda → fungsi hidden `$lambda_N($env, params...)` + struct env
  `$env_N { c0, ... }`; body diawali `let <cap> = $env.c<j>;` (tanpa
  rewrite manual; shadowing aman via set sekuensial).
- Nilai fn = pair heap `{trampoline, env}` (program-lifetime, cermin
  buffer string): referensi-fungsi → `{tramp_biasa, null}`, lambda →
  `{tramp_lifted, env}`.
- Panggilan tak langsung SELALU via trampolin `(ptr env, args...)`:
  trampolin plain meneruskan (abaikan env), trampolin lifted cast env
  + panggil by-value. Panggilan langsung tak tersentuh; `let g = f`
  dan field fn ikut membawa signature.
- Konsekuensi model memori (§4, §10): pair + env tak pernah dibebaskan
  (OS merebut saat exit); aman dibagikan antar-thread (lihat
  `docs/concurrency.md`) selama capture tak dimutasi balapan.

## Batasan Phase 27

- Tanpa `move`/borrow semantics (selalu by-value); tanpa Fn/FnMut/
  FnOnce tiers; tanpa unsize/coerce khusus.
- Tanpa pola `||` ganda selain param-kosong; `|` biner/logis tak ada
  (hanya lambda).
- Rekursi closure, higher-ranked types, impl-Trait-return-position:
  belum.
