# Debugger 2.0 — Design (Phase 64, target 2.15.0)

Status: audit → Implementation (milestone 1: DWARF composite +
formal params). Tanpa debugger interaktif/DAP (tanpa engine;
alasan §4). Spec tak berubah.

## 1. Audit DWARF v1 (2.14.0)

Ada: CU, subprogram, line table, lokal skalar (param+let via
`createAutoVariable`). Celah: struct/array/enum = `diType null`
→ variabelnya HILANG dari DWARF; parameter bukan formal
(backtrace/views kurang presisi); DAP nihil.

## 2. Milestone 1

1. **Struct DWARF**: `DW_TAG_structure_type` + member
   (`DW_TAG_member`, offset bit) dari `StructLayout` codegen
   (rekursif; cache per nama; guard siklus → fallback null).
   Offset dihitung aturan layout Nusa (i64 8/8, bool 1/1,
   string/ptr/fn-ptr 8/8, struct rekursif, align-up) — EKSAS pada
   semua target LP64 (asumsi LP64 global Phase 33; field array
   ditolak checker sehingga tak ada kasus sulit).
2. **Formal parameters**: `createParameterVariable` + argNo
   (1-based) untuk param (let tetap auto).
3. **Array DWARF**: `DW_TAG_array_type` + subrange `[0,N)`,
   N dari tipe alokasi LLVM slot, elemen dari `Type.elem`.

## 3. DITUNDA beralasan

Debugger interaktif/DAP: butuh engine kontrol-proses (ptrace/
fork-exec + breakpoint/trap) — proyek tersendiri (rates
risiko; CI tanpa lldb/gdb). Pretty-printer, lexical block
bersarang: follow-up.

## 4. Acceptance criteria

- `llvm-dwarfdump`: `DW_TAG_structure_type Person` + member
  `x`, `DW_TAG_formal_parameter`, array bound.
- Unit IR (`DW_TAG_structure_type`, `formal_parameter`) +
  e2e_debug kasus baru hijau; suite hijau; bench identik.
