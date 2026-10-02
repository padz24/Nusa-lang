# 05 — Match & enum

Tujuan: statement `match`, pola literal/bind/wildcard/struct,
enum payload + exhaustiveness.
Prasyarat: `tutorials/04-generics-traits/`.

```bash
./build/nusa run tutorials/05-match-enum/main.ns
# 1
# 2
# 10
# 30
```

Aturan: match = statement-only; arm `pola => { ... },` berurutan;
`bool` + pasangan varian tertutup wajib exhaustive (literal umum
butuh `_`); pola unreachable/invalid ditolak; konstruktor enum
qualified (`Color.Red`, `Shape.Circle(5)`), pola varian telanjang
(`Red`, `Circle(r)`).

Lihat: `docs/pattern-matching.md`, `docs/enums.md`.
Lanjut: `tutorials/06-error-handling/`.
