# Distributed System Foundation — Design (Phase 79, target 2.30.0)

Status: implementasi slice ini. Mandat master: PHASE 79
(kondisional: concurrency stabil — TERPENUHI, e2e_concurrency
13/13; library-level primitives; mayoritas di ekosistem/paket,
bukan compiler). Spec bahasa tak berubah.

## 1. Peta mandat → aktual

| Mandat | Aktual 2.30.0 |
|---|---|
| serialization | ADA (csv/json/toml Phase 77; framing di rpc) |
| RPC | **baru** `std/rpc.ns` (JSON-RPC-min atas TCP newline) |
| network protocols | ADA (tcp/http); RPC = pola di atasnya |
| service discovery | **baru** `std/discovery.ns` (daftar statis + pick) |
| timeouts | **baru** `std/timeout.ns` (task + poll deadline) |
| retry policies | **baru** `std/retry.ns` (struct policy + backoff) |

Ekosistem (bukan compiler): semua modul std murni-Nusa;
implementasi service = aplikasi (contoh di e2e + docs).

## 2. Kontrak modul

- `rpc.ns` (`import net`, `import json`, `import string`):
  framing = 1 baris JSON per pesan (`recv_line`).
  `rpc_call(host, port, method, params_json)` → body hasil
  mentah (`""` bila dial/gagal — bedakan via `rpc_ok`?
  TIDAK: "" = gagal, didokumentasikan).
  `rpc_serve_once(port, handler: fn(string)->string)` →
  0 ok / -1 (handler terima baris request, kembalikan baris
  respons; dispatch metode di dalam via json_get_string).
  Helper amplop: `rpc_request(id, method, params)`,
  `rpc_response(id, result)`, `rpc_error(id, msg)`,
  `rpc_method(req)`, `rpc_params(req)`, `rpc_result(resp)`.
  Blocking (tanpa timeout — tasks hanya i64; didokumentasikan).
- `discovery.ns` (tanpa import — murni atas string):
  endpoint teks `"host:port"` (1/baris). `svc_count(list)`,
  `svc_at(list, i)` (wraparound negatif aman → `""` bila
  kosong), `svc_host(ep)`, `svc_port(ep)` (-1 bila buruk),
  `svc_round_robin(list, n)` (ke-n, modulo — counter di
  pemanggil). IPv6 literal tak didukung (split `:` terakhir?
  PERTAMA — didokumentasikan IPv4/hostname).
- `timeout.ns` (`import tasks`, `import time`):
  `with_timeout_ms(ms, op: fn()->i64, fallback)` — task +
  poll 5ms hingga deadline; timeout = fallback + worker
  DITINGGALKAN (tanpa preemptive kill — by-design tasks;
  join/gabung tak mungkin pasca-abandon).
- `retry.ns` (tanpa import — `sleep_ms` builtin):
  `struct RetryPolicy { attempts, base_ms, factor }`,
  `retry_default()` (3x, 50ms, x2),
  `retry_with(p, op: fn()->i64, is_err: fn(i64)->bool)`
  → hasil terakhir (tidur base*factor^(k) antar-coba).

## 3. Batas jujur

- RPC string-only (biner butuh `ord`); tanpa ID korelasi
  otomatis (id dari pemanggil); tanpa notify (baca-buang
  rawan SIGPIPE — didokumentasikan).
- Timeout i64-only (channel generik tak ada); granularitas
  ~5ms; abandon-leak hingga worker selesai.
- Discovery statis (tanpa DNS-SRV/health-check).
- Backoff tanpa jitter (rand state-threading di pemanggil).
- Pelajaran harness (e2e_dist): `wait` polos atas server bg =
  gantung selamanya bila klien gagal-diam (bounded-wait +
  kill + bukti got); port dinamis via probe-tutup (kebal
  stray/paralel); retry panggilan langsung (jendela race
  close→relisten vs dial); JANGAN `pkill -f` berpola (bunuh
  ancestor shell sendiri — PID tercatat saja).

## 4. Pengujian

- `tests/rpc_test.ns` (amplop murni, tanpa net),
  `tests/retry_test.ns` (kebijakan + op file-counter),
  `tests/timeout_test.ns` (cepat-ok + lambat-fallback),
  `tests/discovery_test.ns` (murni).
- `e2e_dist.sh`: `nusa test` + RPC Nusa↔Nusa localhost
  (server binary bg + client golden + unknown-method) +
  timeout/retry golden + negatif (port tutup).

## 5. Acceptance criteria

- `nusa test` +4 hijau; `e2e_dist` hijau; suite lama hijau;
  bench identik; bersih.
