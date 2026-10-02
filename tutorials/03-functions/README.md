# 03 — Functions, structs, modules

Tujuan: `fn` + `return`, struct literal + akses field, gambaran modul.
Prasyarat: `tutorials/02-variables/`.

```bash
./build/nusa run tutorials/03-functions/main.ns
# 30
# Nusa
# 2
```

Modul (multi-file) tidak dicontohkan di sini agar tiap tutorial tetap
satu file — lihat contoh kanonis:

```bash
cd examples && ../build/nusa run main.ns
# 6
# Ada
```

Aturan: `fn main()` tepat satu per program; struct di-pass by-value
(boleh sebagai param/return); field selalu publik.

Lihat: `docs/functions.md`, `docs/structs.md`, `docs/modules.md`.
Lanjut: `tutorials/04-generics-traits/`.
