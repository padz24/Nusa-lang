# Memory Model 3.0 — Audit Menyeluruh (Phase 115, milestone 1)

Status: audit 8 area + kunci mesin cakupan OOM (target 3.16.0).
NOL perubahan semantik (model tetap: owned values + heap
program-lifetime + handle fail-fast). Temuan kode: NIHIL
(disiplin terverifikasi bersih) — nilai slice ini = matriks
keputusan + register residual + kunci anti-regresi.

## 1. Matriks audit (bukti per area)

| Area | Putusan | Bukti |
|---|---|---|
| Stack | aman-terbatas | satu-satunya alloca = entry-block locals/struct/array (`entryAlloca`); ukuran bounded per fungsi; tanpa alloca dinamis |
| Heap | fail-closed penuh | SEMUA alokasi via `mallocFor` + 4 situs langsung, SEMUANYA `emitOomCheck` (pesan + exit 1); dikunci `oom-coverage` |
| Ownership | by-construction | nilai di-copy; tanpa move/borrow/referensi di bahasa → use-after-move dkk TAK TERREPRESENTASI |
| Borrowing/lifetimes | N/A (tak ada fitur) | tanpa referensi = tanpa lifetime checker; penambahannya = RFC bahasa |
| References | N/A | idem; handle i64 BUKAN referensi (lihat bawah) |
| Concurrency safety | fail-fast sekuensial | registry `rt_handle.c` (add/has/remove + mutex); misuse ganda/basi/nol → exit 1 (`e2e_memsafe` 6/6 lama) |
| FFI boundaries | kontrak (tak dienforce) | string C = pinjaman (JANGAN free — `headers`); `bool` i1 vs i32 dipetakan checker; variadik/struct-return/callback ditolak |
| OOM runtime C | terdokumentasi per-situs | chan/mutex/atomic/thread/crypto/net: NULL → 0/sentinel/drop (kode dibaca; perilaku tercatat) |

## 2. Register residual jujur (bukan blocker)

- ABA reuse alamat handle (sejak 2.5.0): pointer mentah + remove
  → alokasi-ulang alamat sama + salinan basi = lolos cek.
  Perbaikan sejati = id generasional (perubahan runtime luas +
  biaya lookup; butuh desain — BUKAN slice ini).
- Rekursi dalam → stack overflow = sinyal (kelas hang-by-design;
  probe stack codegen = follow-up; `-fstack-clash` hanya
  mengubahnya jadi SIGSEGV andal, bukan error bersih).
- `free`-while-borrowed konkuren (sejak 2.5.0): tetap UB.
- Tumbuh-per-iterasi string/loop (by-design, §model): diamati,
  bukan diperbaiki di sini.
- C me-free string Nusa / menahan pointer: kontrak kepercayaan
  FFI (Phase 59); pelanggaran = UB hulu yang tak terdeteksi.

## 3. Diverifikasi

- `e2e_memsafe` + `oom-coverage` (malloc==oom-check di IR,
  fixture closure+concat+int_to_str); suite lama hijau;
  `e2e_release` hijau; `.ns` + IR OK.

## 4. Ditunda jujur

- Id generasional, probe stack, borrow checker/`unsafe{}` (semua
  butuh RFC/desain — kandidat Phase 116+; unsafe TETAP tak ada
  di bahasa).
