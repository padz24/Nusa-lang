# Embedded / Freestanding — Milestone 1: Seam Alokator (Phase 123)

Status: slice implementasi (target 3.24.0). Mandat master: no_std,
runtime minimal, custom allocator, target freestanding; tak semua
stdlib harus tersedia (`Nusa Core → Minimal Runtime → Platform`).

## 1. Cakupan: seam alokasi heap (satu-satunya yang dijanjikan)

- Fakta audit: codegen TAK PERNAH manggil `free` (heap
  program-lifetime); SEMUA alokasi heap (closure/env/string/
  file/intstr) kini via `nusa_alloc` (dulu `malloc` langsung,
  5 situs) + `nusa_free` simetri-masa-depan.
- `runtime/rt_alloc.c` (bagian agregator, kontrak single-TU utuh):
  definisi WEAK default = malloc/free. Target menimpa dengan
  definisi kuat via `--link` (arena/pool/penghitung/fail-fast uji)
  TANPA menyentuh compiler; fallback `.c` ikut serta.
- Deteksi link-minimal diperluas (`@nusa_alloc`): program heap
  menarik `nusa_rt.o` (gc-sections tetap membuang sisanya);
  hello tetap 5816 B; `mix` tetap <9000 B.
- Terbukti dua arah: penghitung (`alloc_count()==1` untuk satu
  concat) + NULL (OOM bersih `str_concat`, rc 1).

## 2. Batas jujur (profil minimal hari ini)

- BUKAN freestanding penuh: `print` butuh puts/printf,
  `main`/`exit` butuh libc; `-nostdlib`/`_start` = follow-up
  (butuh entry + syscall langsung — kriteria: hello tanpa libc).
- BUKAN no_std: modul std bertumpu builtin libc; subset
  `no_std`-kompatibel = follow-up (audit per-modul).
- Alokator kustom = kebijakan target (kontrak: NOL gagal-diam;
  NULL = OOM path yang ada).

## 3. Diverifikasi

- `e2e_min` +2 (override-hitung, override-NULL); kunci simbol
  `e2e_runtime` diperbarui sadar (simbol bounded/try_send Fase 118
  yang tertinggal + weak alloc/free); memsafe/concurrency/stdlib
  + `e2e_release` hijau; ukuran minimal utuh.

## 4. Ditunda jujur

- Entry freestanding, subset no_std, OOM-handler kustom
  (di luar NULL), pool/arena bawaan (kebijakan ekosistem).
