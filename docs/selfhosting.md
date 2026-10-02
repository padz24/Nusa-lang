# Self-Hosting Bootstrap (Phase 38, v1.19.0)

```
C++ compiler
      ↓  (langkah 1, fase ini)
Nusa lexer (selfhost/lex.ns) — dibuktikan ekuivalen via differential test
      ↓  (fase mendatang)
parser → AST → semantic … hingga Nusa mengkompilasi Nusa
```

Aturan main: **jangan tulis ulang sekaligus** — satu komponen,
dibuktikan ekuivalen, baru lanjut. Compiler C++ tetap bootstrap
(kompilasi komponen Nusa).

## Langkah 1: lexer (SELESAI)

- `selfhost/lex.ns`: lexer Nusa streaming (tanpa buffer token —
  Nusa v1 tanpa array growable), dijalankan
  `nusa run selfhost/lex.ns <file>`.
- `nusa lex <file>`: dump token C++ (alat debug) dengan format
  yang SAMA: `kind|lexeme|line|col` (+ baris `end of file`).
- **Differential test** (`e2e_selfhost.sh`): 33 file korpus
  (std + examples + bench + lexer itu sendiri) byte-identik.
  Korpus dinyatakan bebas hazard di bawah (dicek test).

## Tabel escape dump (kontrak kedua implementasi)

| Byte nilai | Cetakan | Catatan |
|---|---|---|
| `\` | `\\` | duluan (agar tak ganda) |
| newline | `\n` | |
| tab | `\t` | |
| CR | `\r` | |
| `\|` | `\|` | separator baris |
| NUL | `\0` | hanya C++ (di bawah) |
| sisanya | apa adanya | byte mentah |

## Batas yang jujur (divergensi terdokumentasi)

1. **`\0` dalam string**: string Nusa = C string (NUL = terminator)
   sehingga byte NUL tak terwakili; C++ mempertahankannya.
   Korpus dijamin bebas NUL (test `hazard`).
2. **Overflow literal**: C++ menolak `> max i64` (Unknown + error);
   Nusa mencetak apa adanya. Korpus bebas literal ≥20 digit.
3. **Input invalid** (`!` tunggal, `123abc`, string tak-terminasi):
   C++ mencetak baris `unknown` + error ke stderr (rc 1); Nusa
   mencetak baris `unknown` best-effort (rc 0). Bentuk baris SAMA,
   kanal/status beda — korpus diferensial hanya file valid.
4. **Performa**: char-by-char di atas `str_char_at` (O(n) per
   panggil karena `strlen`) → ~O(n²); 15 KB ≈ 3 dtk. Cukup untuk
   CI/korpus; API string berkursor akan memperbaikinya.
5. **Struktur**: satu `main` raksasa + helper murni (tanpa tuple/
   global/state komposit di Nusa v1); pola loop `end`/`lim`
   (tanpa `break` — latch tanpa progres = hang, pelajaran tercatat).

## Berikutnya (bukan fase ini)

Semantic Nusa (butuh: tabel simbol growable atau encoding setara).
Masing-masing dengan differential testnya sebelum menggantikan
apa pun.

## Langkah 2: parser + AST (SELESAI — Phase 72, v2.23.0)

- `selfhost/parse.ns`: parser Nusa **fused lexer+parser**
  (buffer token tak mungkin di Nusa v2 — array fixed-size tanpa
  indexed-assign/param/return; tanpa global/tuple). Membaca sumber
  via `file_read`, mencetak dump AST s-expr byte-identik dengan
  `nusa ast` (C++ `program.dump()`).
- `nusa ast <file>`: oracle C++ (parse file tunggal tanpa merge
  modul + cetak dump; rc!=0 bila gagal), cermin `nusa lex`.
- Threading state: struct by-value (`ShTok`/`ShRes`) + rekursi;
  semua loop maju strikt + guard anti-hang (terminasi terjamin
  bahkan atas input invalid — output best-effort).
- **Differential test** (`e2e_parse.sh`): 43 file korpus
  (std + examples + bench + selfhost, termasuk `parse.ns`
  memparse dirinya sendiri) byte-identik; ambang N≥25.
- Normalisasi yang direplika: lambda 1-expr → `(return e)`,
  match single-stmt → Block, `else if` → Block 1-stmt, int via
  `parse_int`, escape dump `" \` newline tab, urutan kategori
  program (imports…functions), `(empty)` untuk file kosong.
- Batas baru (lihat `docs/selfhost-2.23.md` §4): input invalid
  best-effort; nesting adversarial tanpa cap C++; `\0` di-drop;
  `123abc`/overflow diasumsikan tak ada (korpus dijamin).
