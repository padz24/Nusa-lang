# Control Flow Nusa — v1.0.0

```nusa
fn main() {
    let x: i64 = 10;

    if x > 5 {
        print(x);
    } else {
        print(0);
    }

    let n: i64 = 3;
    while n > 0 {
        print(n);
        n = n - 1;
    }
}
// 10, 3, 2, 1
```

## `if` / `else`

```
IfStmt := "if" Expr Block ["else" (Block | IfStmt)]
```

- Kondisi wajib `bool` (`if 1` → error).
- Tanpa parens di kondisi (gaya Rust). `else if` menempel:
  `if a { } else if b { } else { }` (dangling-else ke `if` terdekat).
- Backend: block `then`/`else`/`merge` + `CondBr`.

## `while`

```
WhileStmt := "while" Expr Block
```

- Kondisi `bool`, dicek tiap iterasi; `while false` melewati body.
- Backend: block `loop.cond`/`loop.body`/`loop.after`.
- Semua alloca di entry — tidak ada pertumbuhan stack per iterasi.

## Assignment

`n = n - 1;` — menimpa variabel yang **sudah ada** (tak mendeclare),
tipe harus sama. Inilah cara loop membuat progres. Shadowing-aware
(menimpa slot terdekat).

## Return di dalam cabang

`return` boleh di cabang `if`/`else`/`while`; aturan all-paths
(`docs/functions.md`) digeneralisasi: block selalu-return bila diakhiri
`return` bernilai, atau `if`+`else` yang kedua cabangnya selalu-return.
`while` tak pernah dihitung (bisa nol iterasi — konservatif):
`while true { return 1; }` saja tak cukup untuk fungsi non-`void`.

## Batasan (stabil v1.0.0; `match` sejak v1.6.0)

- Tanpa `break`/`continue`, tanpa `+=`.
- Statement `match` tersedia sejak v1.6.0 (Phase 25) — lihat
  `docs/pattern-matching.md` (literal/bind/struct/guard/Option/Result,
  exhaustiveness wajib). Tanpa `switch` gaya-C.
- Kondisi non-`bool` ditolak (tanpa truthiness implisit).
