# Compile-Time Programming — Design (Phase 70, target 2.21.0)

Status: RFC-0009 Proposed → Implementation slice 1 (`const`).
Spec 2.0 berubah aditif (§1/§2/§3/§4). Payung RFC: `rfcs/RFC-0009-comptime.md`
(keamanan di sana §Security; ringkas di §4 sini).

## 1. Sintaks & semantik slice 1

```ebnf
ConstDecl := "const" Ident [":" Type] "=" Expr [";"]
```

- Top-level (seperti `type`/alias; merge modul; duplikat = error,
  termasuk tabrakan dengan nama fungsi — ambiguitas VarRef).
- `expr`: literal i64/bool/string, unary `-`/`+` (i64), arith
  i64 (`+ - * /`), perbandingan i64, `==`/`!=` homogen non-void,
  ref const lain (urutan-bebas via fixpoint; siklus = error).
- Anotasi dicek persis (`mismatched types`, cermin `let`).
- Substitusi tekstual scope-aware di checker (let/param men-shadow,
  seragam shadowing): codegen nol baris (menerima literal).
- `x = ...` ke nama const = `cannot assign to constant ...`.
- Div-nol literal/dinamis-konstan = error; overflow i64
  (via `__int128`) = error; string hanya literal (tanpa op).

## 2. Peta implementasi

Lexer `const` (+selfhost/spell) → AST `ConstDecl` (+dump/clone)
→ Parser top-level (+recovery/error-list) → Modules merge →
Semantic Pass-1f (set + dup + VarRef/assign terima) → Checker
(eval fixpoint + substitusi + anotasi) → fmt/doc.

## 3. DITUNDA (butuh RFC lanjutan)

Fungsi/loop/IO di const, `comptime {}`, `const fn`, param
generik-konstanta, asersi-CT, forward... (urutan-bebas DIDUKUNG
via fixpoint; hanya eksekusi umum yang ditunda).

## 4. Keamanan (ringkas; normatif di RFC §Security)

Terminating trivial (tanpa call/loop/var), deterministik,
tanpa efek/alloc; overflow/div-nol = error (bukan wrap/trap).

## 5. Acceptance criteria

- Unit per tahap + `e2e_const.sh` (runtime, 10+ penolakan, fmt,
  doc) hijau; suite hijau; bench identik; bootstrap hijau
  (selfhost/lex.ns tak memakai `const` — diverifikasi).
