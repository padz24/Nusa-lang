# Distributed Tooling — Milestone 1: Tracing (Phase 119)

Status: slice implementasi (target 3.20.0). Mandat master: logging,
tracing, metrics, request ID, structured error; CLI `trace` +
`inspect`. Cakupan slice: request ID + span/event (`std/trace`)
+ `nusa trace run/check`. Yang sudah ada dipakai ulang: logging
(level), metrics (Prometheus), config. `nusa inspect` = follow-up
(butuh desain kueri: flame/timeline/filter).

## 1. Desain (stateless, propagasi via env)

- ID: `TRACE_ID` env bila ada (antar-layanan), selainnya
  `<ms>-<randhex-OS>` (crypto_rand_hex; tabrakan butuh ms + rand
  sama). Tanpa global mutabel (gaya state-eksplisit repo).
- Span: `trace_begin(op) -> Trace{id,op,t0}` (cetak begin),
  `trace_end(t) -> ms` (cetak end). Event: `trace_event(op,
  status, note)`. Satu baris JSON per baris; escape `\" \\`.
- `nusa trace run prog [args]`: bangkitkan ID (`tr-<ms>-<hex>`),
  eksekusi dengan env tersebut (semua baris berbagi ID —
  terbukti e2e), teruskan rc program.
- `nusa trace check log`: validator BENTUK nusa-trace (bukan JSON
  umum — jujur di pesan/help): `{"type":...` pertama + kunci wajib
  per tipe; ringkasan + nomor baris pertama-rusak; rc 1 bila ada
  invalid. Baris non-trace = invalid (filter dulu bila campuran).

## 2. Batas jujur

- Tanpa sampling/tail-policy, tanpa korelasi induk-anak
  (parent-span = follow-up), tanpa kolektor jaringan (stdout =
  kolektor), tanpa jam tersinkron (durasi lokal saja).
- `inspect` (analisis: agregasi p95, orphan-end, critical path)
  = follow-up dengan desain.

## 3. Diverifikasi

- `tests/trace_test.ns` via `nusa test` (ID unik/tak-kosong,
  span, escape); `e2e_trace` 4/4 (test-file, run+ID-tunggal+
  check, bad-log, fmt); `e2e_release` hijau; `.ns` + IR OK.

## 4. Ditunda jujur

- `nusa inspect`, parent-span, sampling, kolektor, jam global.
