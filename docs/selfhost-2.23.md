# Self-Hosting Milestone — Design (Phase 72, target 2.23.0)

Status: Langkah 2 (parser+AST) selesai; payung: `docs/selfhosting.md`.
Spec bahasa tak berubah (murni aditif: 1 CLI debug + 1 program Nusa).

## 1. Arsitektur (fused lexer-parser)

Buffer token tak mungkin di Nusa v2 (array fixed-size tanpa
indexed-assign/param/return; tanpa global/tuple) — maka
`selfhost/parse.ns` = lexer+parser fused atas string sumber
(hasil `file_read`), cermin gaya streaming `lex.ns` (loop
`end`/`lim`, tanpa break; semua loop maju strikt).

Threading state tanpa tuple: struct by-value (didukung v1.2.0,
diverifikasi) + rekursi (diverifikasi: `fact(10)` = 3628800):

- `ShTok { kind: string, lex: string, pos: i64 }`
- `ShRes { s: string, pos: i64 }` (potongan dump + posisi lanjut)

`kind`: nama keyword persis (`fn`, `const`, ...) atau
`ident/int/str/true/false`, 1 char untuk simbol, 2 char untuk
`== != <= >= -> =>`, `eof`, `unknown` (di-skip cermin C++
yang membuang Unknown pra-parse).

## 2. Cakupan vs C++ Parser (ekuivalen dump pada korpus valid)

Top-level: import/type/const/trait/impl/struct/enum/extern/fn
(urutan cetak program: imports, aliases, traits, impls,
structs, enums, consts, functions — 7 string kategori di main).
Stmt: let/print/return/if/while/match/assign/call-stmt.
Expr: precedence climbing equality→unary, postfix `?`/index/
field/method, atom (lambda `|..|`, `self`, int/str/bool,
call, struct-lit + generik, array, paren) + disambiguasi
lookahead (`Name {`, `E<T>.V(`, `Point<T> {`).

Normalisasi yang direplika: lambda 1-expr → `(return e)`;
match single-stmt → Block; `else if` → Block 1-stmt;
`(fn name... -> void )` body kosong; int via parse_int.

## 3. Oracle: `nusa ast <file>`

CLI debug cermin `nusa lex` (Phase 38): parse + cetak
`program.dump()`; rc!=0 bila gagal. Diferensial
`e2e_parse.sh`: korpus valid (`nusa ast` rc 0 atas
std+examples+bench+selfhost) byte-identik; N≥25.

## 4. Batas yang jujur (divergensi terdokumentasi)

1. Input invalid: best-effort (seperti lexer); korpus hanya valid.
2. Nesting dalam adversarial: rekursi native (tanpa cap
   `too deeply nested`); korpus dangkal (terverifikasi).
3. Escape `\0`: string Nusa = C string (warisan lexer);
   korpus bebas (hazard check).
4. `123abc`/overflow literal: korpus bebas (hazard check).

## 5. Acceptance criteria

- `nusa ast` + `parse.ns` byte-identik ≥25 file korpus;
  `e2e_selfhost`/`e2e_bootstrap` tetap hijau (lex.ns tak
  tersentuh); suite hijau; bench identik; `.ns.ll` bersih.
