# Modul Nusa — v2.0.0

```nusa
// project/main.ns
import math;

fn main() {
    print(add(2, 3));
}
```

```nusa
// project/math.ns
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
```

## Resolution

`import NAME;` dicari berurutan (pertama yang ada menang):

1. Direktori file pengimpor (`<dir>/NAME.ns`).
2. Entri `$NUSA_PATH` (dipisah `:`).
3. `<direktori-executable>/../std` (standard library).

Lokal menang atas std (bisa override). `import string;` valid
(keyword tipe diterima sebagai nama modul). Flat saja: tanpa
subdir (`import a/b`), tanpa `::`. Dependensi antar-paket
dikelola package manager (`nusa.toml`, lihat `docs/pkg.md`) —
level file tetap flat seperti di sini.

## Namespace dan visibilitas

- Tiap modul namespace sendiri; `import` menggabung nama **publik**.
  Konflik (nama sama dari dua modul, atau dengan definisi lokal) →
  error duplikat.
- Visibilitas dasar: nama `_`-awalan **module-private** — dipakai dari
  file lain → error `is private to module`. Berlaku untuk fungsi dan
  struct (field selalu publik).
- Import transitif (modul mengimpor modul) dan duplikat diam-diam
  (idempoten) didukung.

## Circular dan error

- Siklus → `circular import detected: a -> b -> a` (exit 1, tanpa hang).
- Modul hilang → `cannot read module file` + hint lokasi.
- Rantai > 100 modul ditolak bersih (anti stack overflow).
- Diagnostic selalu memakai file yang tepat (engine per file).

## Batasan (stabil v2.0.0)

- Level file: tanpa versioning, `pub` eksplisit, re-export,
  glob-import, subdir. Versioning + dependensi = layer paket
  (`docs/pkg.md`), bukan sintaks `import`.
