# Bootstrap Chain (Phase 39, v1.20.0; multi-komponen Phase 73, v2.24.0; full Phase 74, v2.25.0)

> Dokumentasi proses lengkap (peta 4 stage, tabel Nusa-vs-C++,
> reproduksi manual, diagnosis): `docs/bootstrap-full-2.25.md`.

```
Stage 0                    Stage 1                    Stage 2
C++ compiler  ──build──▶  <base>_stage1 (native,  ──run──▶  dump
(build/nusa)              ditulis dalam Nusa)            ║
      │                                                  ║ bandingkan
      └──────────── referensi C++ ═══════════════════════╝
                        (--as lex: nusa lex | --as ast: nusa ast)
```

Perintah: `nusa bootstrap [file.ns] [--as lex|ast] [-o DIR] [--rebuild RDIR]`
(default file `<exe>/../selfhost/lex.ns` lalu `selfhost/lex.ns`;
default DIR `bootstrap-out`; default `--as lex`; `--rebuild`
menjalankan Stage 3: fresh-rebuild + banding D2).

Rantai yang dikunci:

| Komponen | Stage 1 | Stage 2 (self-application) | Stage 0 |
|---|---|---|---|
| `selfhost/lex.ns` | `lex_stage1` | token dump dirinya | `nusa lex` |
| `selfhost/parse.ns` | `parse_stage1` | AST dump dirinya | `nusa ast` |

## Yang dibuktikan (dan yang belum)

- **Stage 1**: compiler C++ mampu mengkompilasi komponen compiler
  yang ditulis dalam Nusa (lexer) menjadi biner native — build nyata
  (`--fresh`, bukan cache).
- **Stage 2 = Stage 0**: biner Nusa menokenisasi `lex.ns` sendiri
  (self-lexing) byte-identik dengan dump lexer C++ (34.648 byte).
- **Reproduksibilitas**: Stage 2 dijalankan 2x — byte-identik
  (determinisme output; hash implisit via perbandingan penuh).
- **Belum**: rantai penuh compiler (parser/AST/semantik masih C++ —
  lihat `docs/selfhosting.md`). Setiap komponen Nusa berikutnya
  mengikuti pola yang sama: tulis → differential test → masuk rantai
  (argumen `lex.ns` pada perintah menerima komponen masa depan yang
  memenuhi kontrak "biner menerima path sumber, mencetak dump ke
  stdout").

## Kontrak komponen bootstrap

1. Program Nusa valid (`nusa check` hijau) dengan `fn main()`.
2. Membaca satu path sumber dari `process_argv(1)`.
3. Mencetak hasil ke stdout dengan format stabil (untuk lexer:
   `kind|lexeme|line|col` + escape di `docs/selfhosting.md`).
4. Deterministik (tanpa waktu/acak/PID di output).
5. Korpus valid: batas `selfhosting.md` berlaku (\0/overflow/invalid
   di luar kontrak).

## Artefak & reproduksi manual

```
nusa bootstrap selfhost/lex.ns -o bootstrap-out
./bootstrap-out/lex_stage1 selfhost/lex.ns | cmp - <(nusa lex selfhost/lex.ns) \
  && echo "Stage 2 == Stage 0"
nusa bootstrap selfhost/parse.ns --as ast -o bootstrap-out-parse
./bootstrap-out-parse/parse_stage1 selfhost/parse.ns | cmp - <(nusa ast selfhost/parse.ns) \
  && echo "Stage 2 == Stage 0"
```

Kegagalan mencetak byte beda pertama (kedua sisi) agar dapat
didiagnosis tanpa mengulang rantai.
