# Memory Safety — Audit & Design (Phase 54, target 2.5.0)

Status: audit selesai → Implementation (milestone 1: registry
handle fail-fast). Spec 2.0 tidak berubah kecuali edge misuse
(UB → error keras, didokumentasikan di sini + CHANGELOG).

## 1. Hasil audit (evidence, bukan asumsi)

Model bahasa (`docs/memory-model.md`): owned values + heap
program-lifetime; tanpa referensi/pointer/free di level bahasa.
Diverifikasi: kode Nusa murni TIDAK DAPAT mengekspresikan
use-after-free / double-free / ownership transition invalid —
tidak ada konstruksi bahasa untuk itu (tanpa `unsafe{}` dialect;
sengaja tidak diperkenalkan di fase ini — itu language-level dan
wajib RFC tersendiri bila kelak dibutuhkan).

Kompiler: disiplin `unique_ptr` AST + `malloc` ber-`emitOomCheck`
di `str_concat`/`file_read`; ASan+UBSan bersih untuk MIR/opt
(Phase 52/53). Full-suite ASan build ditunda (keterbatasan RAM
perangkat; lihat §4).

TERBUKTI BERBAHAYA (ASan, program `.ns` valid-secara-tipe):

1. `thread_join(h); thread_join(h);` → heap-use-after-free di
   `nusa_thread_join` (nusa_rt.c:57; non-ASan: Bionic abort
   `invalid pthread_t`, tetap crash tanpa diagnostic Nusa).
2. `chan_free(c); chan_send(c, 1);` → heap-use-after-free di
   `nusa_chan_check` (nusa_rt.c:142).
3. Pola sama untuk mutex (`free`→`lock`) dan atomic (`free`→`get`).

Akar: handle = pointer mentah sebagai `i64` yang bebas dicopy;
`join`/`free` merebut kembali tanpa invalidasi salinan.

## 2. Batas unsafe (definisi eksplisit, tanpa sintaks baru)

Nusa tidak memiliki blok `unsafe { ... }`. Permukaan yang MAMPU
melanggar safety dibagi tiga, dengan aturan masing-masing:

- **Builtin runtime** (`isBuiltin`): implementasi C/LLVM di
  codegen + `nusa_rt.c`. Aturan: semua handle lifecycle fail-fast
  (milestone 1); OOM → pesan + exit(1); mismatch → pesan + exit(1).
- **`extern fn` FFI**: pustaka dipercaya penuh (kontrak Phase 32 —
  pin sumbernya). Tetap di luar analisis safety.
- **Nusan murni**: aman by-construction (§1); misuse handle
  (salinan `i64`) DITRAP runtime, bukan UB (milestone 1).

## 3. Milestone 1 (SATU-SATUNYA perubahan runtime fase ini)

Registry handle hidup di `nusa_rt.c` (mutex-global + list):

- `new/spawn` → daftarkan; `join/free` → keluarkan + bebaskan;
  pakai/`free` atas handle tak-terdaftar (ganda, basi, atau nol)
  → `runtime error: invalid handle in <op>` + exit(1) — konsisten
  dengan OOB/div-0/channel-mismatch (bukan abort diam).
- Perilaku berubah (sengaja, fail-fast): `thread_join(0)`,
  `mutex_*(0)`, `atomic_*(0)` yang dulu diam kini error keras;
  `chan_*(0)` sudah error sebelumnya. Tak ada test yang bergantung
  pada perilaku diam (telah digrep).
- Biaya: satu mutex lock/unlock per op (correctness > perf;
  dikuantifikasi di Phase 86).

DITUNDA beralasan (residual jujur, bukan klaim sembuh total):

- **ABA reuse**: free + alokasi-ulang di alamat sama + pakai basi
  masih lolos (butuh handle generasional = milestone berikut).
- **Free-while-borrowed konkuren** (free saat thread lain blocked
  di `recv`/`join`) tetap UB kelas data-race (butuh refcount/
  epoch — milestone berikut; aturan sementara: jangan free selagi
  dipinjam thread lain).
- Lifetime/borrow-checker statis: butuh tipe referensi (`&T`)
  yang tidak ada di Nusa — ditinjau bila referensi ditambahkan
  (konsisten dengan penolakan ownership-model di `memory-model.md`).

## 4. Acceptance criteria

- Program bukti (double-join, send-after-free, double-mutex-free,
  atomic-use-after-free) → exit 1 + pesan `invalid handle`
  (bukan abort/sinyal); ASan bersih untuk pola sekuensial.
- `e2e_memsafe.sh` hijau + seluruh suite hijau + benchmark identik.
