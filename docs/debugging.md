# Debugging Nusa (Phase 37, v1.18.0)

```sh
nusa build app.ns -o app -g   # DWARF penuh (juga via `pkg build -g`)
```

## Yang dihasilkan (`-g`)

- Compile unit + subprogram per fungsi (`DW_TAG_subprogram` +
  `DW_AT_decl_line`), parameter formal (`DW_TAG_formal_parameter`),
  variabel lokal (`DW_TAG_variable`: skalar `i64`/`bool`/`string`-
  sebagai-pointer sejak v1; struct + array sejak v2.15.0), dan
  line table per statement (`DW_AT_decl_line` + `file_names`).
- Struct: `DW_TAG_structure_type` + member + offset (aturan layout
  LP64; nested didukung; slot fn/env dilewati). Array:
  `DW_TAG_array_type` + bound statis.
- Bahasa host DWARF = C (tak ada tag bahasa Nusa; `DW_LANG_C`).
- Verifikasi: `llvm-dwarfdump --debug-info/--debug-line app`.

## Pemakaian interaktif (bila ada debugger)

`lldb app` / `gdb app` (tidak tersedia di CI Termux ini —
diverifikasi via `llvm-dwarfdump`):

```
(lldb) breakpoint set --file app.ns --line 2
(lldb) run
(lldb) frame variable c
(lldb) bt
```

## Batasan v2 (jujur)

- Closure env, enum-payload mentah, dan struct berslot-fn tak
  di-`declare` (inspek via memori mentah).
- Tanpa lexical block bersarang (satu scope per fungsi) — breakpoint
  berbasis baris tetap tepat.
- Tanpa pretty-printer Nusa (string tampil sebagai pointer; baca
  via `x/s`).
- `-g` + `-O2` valid (lokasi bisa meleset akibat optimasi —
  perilaku clang standar).
- Tanpa `-g`: nol metadata (IR dan biner bersih, teruji).
