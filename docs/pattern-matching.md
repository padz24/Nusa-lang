# Pattern Matching (Phase 25, v1.6.0)

Statement `match` memilih satu dari banyak arm berdasarkan BENTUK nilai,
bukan rantai `if`/`else` manual:

```nusa
fn f(x: i64) -> i64 {
    match x {
        1 => { return 10; },
        2 => { return 20; },
        _ => { return 30; },
    }
    return 0;
}
```

Aturan umum:

- `match` adalah STATEMENT (bukan ekspresi): tiap arm berakhir `{ ... }`
  atau single-statement, dipisah koma (trailing comma boleh).
- Subject dievaluasi SEKALI (`let $matchK = subject` internal).
- Arm pertama yang cocok menang; sisanya tak dieksekusi.

## Pola

| Pola | Contoh | Arti |
|---|---|---|
| Literal int/bool/string | `1`, `true`, `"a"` | kesetaraan (`==`; string via perbandingan isi) |
| Wildcard | `_` | selalu cocok, tanpa binding |
| Bind | `n` | selalu cocok + `let n = <nilai>` untuk body |
| Struct | `Point { x: 0, y }` | tiap field dicocokkan rekursif; `y` = bind, `x: 0` = literal |
| Varian std | `Some(v)`, `None`, `Ok(v)`, `Err(e)` | Option/Result (lihat bawah) |
| Guard | `n if n > 10` | pola cocok DAN guard `bool` true |

String dibanding isi (bukan pointer); literal negatif didukung (`-1`).

## Guard (`if`)

```nusa
match x {
    n if n > 10 => { return 1; },
    _ => { return 0; },
}
```

- Guard harus bertipe `bool` (siap pakai binding pola: `n` di atas).
- Arm ber-guard SELALU dianggap refutable: gagal guard → lanjut ke arm
  berikut, dan tak menyumbang exhaustiveness (lihat bawah).

## Varian std: Option/Result (v2.0: enum natif)

`Option<T>`/`Result<T, E>` adalah enum natif (lihat
`docs/error-handling.md`), sehingga variannya memakai jalur enum
umum: uji `tag` + destruktur slot payload:

```nusa
import option;
fn descr(o: Option<i64>) -> i64 {
    match o {
        Some(v) => { return v; },   // tag Some, v = payload
        None => { return 0 - 1; },  // tag None (tanpa kurung!)
    }
    return 0;
}
```

- `None` ditulis TANPA kurung (ditulis `None`, bukan `None()`).
- `Some`/`Ok`/`Err` telanjang (tanpa argumen) ditolak dengan pesan arity.
- Di luar enum, `Name(...)` ditolak sebagai
  `cannot match variant` — kecuali subject bertipe enum, yang memakai
  pola variannya sendiri (lihat `docs/enums.md`).
- Nama `Some`/`None`/`Ok`/`Err` di posisi pola SELALU dibaca sebagai
  varian, tak pernah sebagai binding.

## Exhaustiveness (wajib lengkap)

Compiler MENOLAK `match` yang tak menutup semua kasus
(`non-exhaustive match`):

- `_` atau bind polos menutup segalanya (arm sesudahnya `unreachable`).
- `true` + `false` menutup `bool` (pesan menyebut sisi hilang:
  `missing pattern for 'false'`).
- `Some` + `None` menutup `Option`; `Ok` + `Err` menutup `Result`.
- Literal ganda dilaporkan (`this literal is already covered above`).
- Arm ber-guard tak menutup apa pun (walau `_ if ...`).

## Implementasi: desugar (bukan codegen baru)

`match` di-desugar typechecker menjadi `let` + rantai `if`/`else` biasa
SEBELUM codegen — backend LLVM tak tersentuh:

- Uji pola digabung via flag internal (`$mK`); tiap uji murni dan aman
  di-evaluasi lolos-singkat-semantik (efek bersih: baca `tmp` saja).
- Binding pola = `let` di awal body arm (scope arm terisolasi).
- Guard di-inline (binding → klon aksesor murni) lalu digabung ke flag,
  sehingga dievaluasi SEBELUM `let` body dieksekusi — benar di checker
  maupun codegen.
- Satu arm (`match x { n => ... }`) menjadi `if true { ... }` yang
  transparan untuk analisis all-paths return.

Konsekuensi: pesan error di dalam body menunjuk baris asli (lokasi
dipertahankan), dan `nusa fmt` memformat `match` (koma arm mengakhiri
baris; idempoten).

## Batasan Phase 25

- Statement-only (tak ada `let y = match ...`; bungkus fungsi).
- Tanpa pola or (`1 | 2`), tanpa range, tanpa nesting varian-dalam-varian
  di luar struct (struct boleh bersarang).
- Tanpa exhaustiveness untuk int/string umum (butuh `_`; bool, pasangan
  varian std, dan seluruh varian enum pengguna tertutup tanpa wildcard).
- Varian enum pengguna = Phase 26, v1.7.0 (`docs/enums.md`).
