# Real-Time / Low-Latency Profile — Milestone 1: Ukur + Aturan (Phase 124)

Status: slice profil (target 3.25.0). Mandat master: profil
low-latency BILA model+runtime memungkinkan; alokasi prediktabel,
latensi terbatas, overhead minimal; TANPA klaim hard real-time
tanpa bukti formal (dipatuhi: nol klaim semacam itu di sini).

## 1. Vonis kelayakan (bukti, bukan asumsi)

- MENDUKUNG latensi: TANPA GC = tanpa jeda koleksi by-construction;
  alokasi heap O(1) fail-closed; bounded-channel (3.19.0) membatasi
  antre; mutex/cond POSIX standar; `-O2` backend.
- MELAWAN latensi: heap tak pernah dibebaskan (jejak tumbuh;
  fragmentasi malloc jangka-panjang), mutex default TANPA
  priority-inheritance (inversi prioritas mungkin), tanpa
  deadline-scheduler, tanpa probe stack (overflow = sinyal),
  wall-clock satu-satunya jam (tanpa monotonik terpisah).
- Vonis: profil "latensi-lunak terdokumentasi" YA; hard
  real-time TIDAK (butuh bukti formal + scheduler + analisis
  WCET — di luar Nusa hari ini).

## 2. Aturan profil low-latency (disiplin, bukan flag)

1. Prealokasi di luar loop panas (heap tumbuh = jejak, bukan
   jeda — tapi RSS menekan cache/TLB).
2. Antre pakai `chan_new_bounded` (bukan tak-terbatas) agar
   backlog TAK PERNAH tak-terbatas.
3. Hindari kontensi mutex di jalur panas (partisi ala
   `par_map_sum`; actor per-bagian).
4. Build `-O2` + runtime minimal (seam alokator bila target
   mengharuskan arena).
5. Ukur dengan `bench/latency.sh` di target SEBENARNYA; angka
   CI ini BUKAN janji perangkat lain.

## 3. Angka acuan CI (Termux AArch64, ping-pong 2000x)

n=15: min 90ms, median 100ms, max 119ms, spread 29ms
(≈45–60µs per putaran send/recv+malloc/free-node). Output
deterministik 1999000. Disimpan sebagai baseline, bukan batas.

## 4. Diverifikasi

- `e2e_latency` 3/3 (selesai + dimensi + urutan robust);
  `e2e_release` hijau; `.ns` + IR OK (NOL jalur compiler).

## 5. Ditunda jujur

- Flag `--profile=realtime`, priority-inheritance, deadline,
  jam monotonik, analisis WCET, bukti formal (prasyarat
  klaim hard real-time — tidak dijanjikan).
