# Model Memori Nusa — v1.4.0 (Phase 23, Design Document)

Dokumen ini MEMILIH model memori resmi Nusa (mandat Phase 23:
jangan implementasi sebelum dokumen ini ada) dan mencatat
konsekuensinya untuk compiler, runtime, dan programmer.

## 1. Evaluasi opsi

| Opsi | Cocok? | Alasan |
|---|---|---|
| Ownership/borrowing (Rust) | TIDAK (1.x) | Butuh tipe referensi (`&T`), lifetime, borrow checker — Nusa tak punya referensi sama sekali. Ditinjau bila referensi ditambahkan. |
| Garbage collector (tracing) | TIDAK | Butuh runtime GC (tak ada yang cocok di Android-Termux), barrier, root map LLVM — overkill untuk bahasa tanpa pointer user. |
| Reference counting | TIDAK | Butuh pointer berbagi di kode user — tak ada. RC hanya-untuk-string internal tak membayar kompleksitasnya. |
| Arena per-scope/iterasi | DITOLAK | Butuh analisis escape (nilai lolos ke scope luar) yang tak dimiliki compiler; salah-bebas = use-after-free. |
| **Owned values + heap program-lifetime** | **DIPILIH** | Lihat §2. |

## 2. Model terpilih: owned values + program-lifetime heap

**Satu kalimat:** setiap nilai dimiliki binding/scope-nya di stack;
heap adalah buffer runtime yang hidup selama program (OS merebut
kembali saat exit) — string (§3), pair/env closure (v1.8.0),
handle concurrency yang belum di-`join`/`free` (v1.9.0).

**Rasional:** Nusa (s.d. v1.4.0) tak memiliki pointer, referensi,
atau alokasi user — semua nilai agregat inline dan fixed-size.
Satu-satunya alokasi dinamis adalah buffer string internal
(`str_concat`, `input`, `file_read`) yang immutable dan tak pernah
dibagikan secara mutabel. Model ini aman (tanpa use-after-free —
tak ada free manual), predictable (biaya = malloc per string baru),
cocok LLVM (alloca + malloc biasa), cocok Android/Bionic, dapat
dijelaskan dalam satu paragraf, dan performanya masuk akal untuk
domain Nusa (skrip/CLI/alat berumur pendek).

## 3. Allocation

- Nilai user (skalar, array, struct, function pointer): **stack**,
  slot LLVM `alloca` per binding, ukuran statis (array/struct inline).
- String literal: segmen read-only biner (pointer global).
- String runtime (`str_concat`, `input`, `file_read`): **heap**
  (`malloc` + cek OOM → `runtime error: out of memory (sumber)`).
- Closure (v1.8.0): pair `{trampoline, env}` + struct env per EKSEKUSI
  kreasi (`malloc` + OOM-check; capture by-value, tak dibebaskan).
- Concurrency (v1.9.0): handle heap runtime; `join`/`free` merebutnya
  (lihat `docs/concurrency.md`).
- Tak ada `malloc` di kode user; tak ada API alokasi publik.

## 4. Deallocation

- Stack: otomatis saat scope/fungsi selesai (semantik LLVM).
- Heap runtime: **tidak dibebaskan manual** — program-lifetime;
  OS (Linux/Bionic) merebut kembali saat proses exit.
- File (`fopen` di `file_*`): ditutup segera (`fclose`) di builtin
  yang sama — bukan heap, tapi resource; tak pernah bocor.
- Konsekuensi jujur: loop yang memanggil `input`/`str_concat`/
  `file_read` tumbuh sebesar buffer per iterasi. Program terbatas →
  memori terbatas. Programanken pola streaming (proses per baris
  tanpa menimbun) untuk input besar.

## 5. Ownership

- Setiap nilai dimiliki tepat satu binding (`let`, parameter, field).
- Assignment/passing/return = **copy**: skalar by-value; agregat
  (array/struct) `memcpy` semantik; string/pointer = copy pointer
  (aman: string immutable, buffer tak pernah ditulis ulang setelah
  dibuat — kecuali strip `\n` internal `input` sekali saat lahir).
- Tak ada move/borrow/pinjaman — copy selalu legal dan murah
  (ukuran kecil; string besar hanya copy pointer).

## 6. References

- **Tidak ada** tipe referensi (`&T`), pointer user, atau `null`
  di level bahasa. Akses field/index bekerja pada value.
- Pointer LLVM hanya ada di dalam codegen (slot, buffer) dan tak
  pernah terekspos ke program.

## 7. Mutability

- Binding `let` dapat di-assign ulang (`=` menyalin nilai baru).
- Tak ada `const`/`mut`: semua binding mutabel, semua string
  immutable secara konvensi runtime (tak ada primitif tulis-string).
- Tak ada interior mutability; data race hanya via pola concurrency
  yang didokumentasikan (sejak v1.9.0, lihat `docs/concurrency.md`;
  single-thread s.d. v1.8.0).

## 8. Lifetime

- Leksikal: nilai hidup selama scope binding-nya; parameter selama
  panggilan; return value disalin ke pemanggil.
- Buffer string hidup selama program (lihat §4).
- Tak ada lifetime annotation (tak ada referensi yang butuh).

## 9. Stack

- Semua slot user di stack frame fungsi (entry `alloca`).
- Rekursi dalam memakai stack C (tanpa TCO): kedalaman ~ribuan
  aman; rekursi infinit → stack overflow (SIGSEGV) seperti C —
  batasan terdokumentasi, bukan error rapi.
- Tak ada watering: ukuran frame statis per fungsi.

## 10. Heap

- Buffer string runtime + pair/env closure (v1.8.0, §3). Tak ada heap
  untuk array/struct inline/trait object (dispatch statis).
- Runtime concurrency (v1.9.0): heap handle thread/mutex/channel/
  atomic (`join`/`free` merebutnya — pengecualian sadar dari §4,
  lihat `docs/concurrency.md`).
- OOM (`malloc` null, kecuali `getline` yang gagal → diperlakukan
  EOF → `""`): `runtime error: out of memory (sumber)` + exit 1.
- Pola yang dilarang model ini (masa depan): referensi heap user,
  struktur rekursif heap (`Cons`), string builder inkremental —
  menunggu alokator eksplisit / fase lanjutan bila dibutuhkan.

## 11. Yang TIDAK berubah di v1.4.0

- Tak ada sintaks/tata-bahasa baru; tak ada perubahan IR untuk
  nilai user; komentar "bocor sengaja" diganti referensi ke dokumen
  ini (perilaku sama, status resmi: by-design program-lifetime).
- Revisi dijadwalkan bila Nusa mendapat referensi/heap user
  (lihat opsi ditolak di §1).

## 12. Adendum v2.5.0 (Phase 54 — handle lifecycle fail-fast)

- Handle concurrency (thread/mutex/channel/atomic) kini dilacak
  registry hidup di `nusa_rt.c`: `join`/`free`/pakai ganda atau
  basi (sekuensial) → `runtime error: invalid handle in <op>` +
  exit 1 (dulu: UAF/double-free, terbukti ASan). Aturan: tiap handle
  dipakai selagi hidup dan di-`join`/`free` tepat sekali; salinan
  tidak memperpanjang lifetime; jangan free selagi dipinjam thread
  lain. Residual jujur (ABA reuse, free-while-borrowed konkuren):
  lihat `docs/mem-2.5.md`. Tanpa sintaks baru; tanpa `unsafe{}`.
