# 04 — Generics & traits

Tujuan: `fn f<T>`, `struct P<T>`, `trait`/`impl`, bounds `T: Trait`.
Prasyarat: `tutorials/03-functions/`.

```bash
./build/nusa run tutorials/04-generics-traits/main.ns
# 42
# hi
# A
```

Aturan: inferensi di call-site (tanpa turbofish); monomorphisasi per
kombinasi argumen; fungsi generik sebagai value ditolak; dispatch
trait selalu statis (rewrite ke fungsi mangled); satu impl per
(trait, tipe) program-wide.

Lihat: `docs/` generics (via `docs/language-spec-2.0.md` §8–§9),
`docs/traits.md`, RFC-0001/0002.
Lanjut: `tutorials/05-match-enum/`.
