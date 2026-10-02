# Distributed Programming Foundation — Milestone 1: Backpressure (Phase 118)

Status: slice implementasi (target 3.19.0). Mandat master:
serialization, RPC, timeouts, retries, backpressure, komunikasi
layanan; mayoritas di ekosistem; compiler hanya primitif yang
benar-benar perlu. Inventarisasi: serialization (json/toml/csv),
RPC, timeout, retry, discovery SUDAH ADA (Phase 77–79); HILANG =
backpressure (channel tak-terbatas + tanpa kirim non-blocking).

## 1. Yang dibangun: channel bounded + try_send (i64)

- Runtime (`runtime/rt_chan.c`): field `cap` (0 = tak-terbatas,
  kompatibel); `nusa_chan_new_bounded(cap)` (cap ≤ 0 = fail-fast,
  bukan rendezvous implisit); `nusa_chan_try_send(h, v) -> 0/1`
  (penuh = 0 tanpa menunggu; cek-ulang di bawah lock melawan
  balapan dua-pengirim; OOM = 0 cermin drop send).
- Bahasa: `chan_new_bounded(cap: i64) -> i64`,
  `chan_try_send(c, v: i64) -> bool` (builtin + checker + codegen
  pola tcp_send + LSP completion). Bertag = follow-up eksplisit
  (tag dipaksa 0 → mismatch bersih).
- Pola layanan: semaphore N = bounded(N) + try_send (coba) /
  send (tunggu); produsen-konsumen overflow-terkendali.

## 2. Bug laten ditemukan & diperbaiki (di luar rencana)

Grup emisi void codegen TANPA guard (`mutex_lock` … `sleep_ms`
berurutan tanpa `if` — benar HANYA via exhaustiveness): builtin
bernilai baru yang jatuh ke sana dieksekusi sebagai `sleep_ms`
+ Void (gejala: print macet "cannot print 'void'", `let` =
SIGSEGV compiler via deref null!). Guard eksplisit ditambahkan.
Pelajaran: exhaustiveness implisit rapuh terhadap penambahan —
kini dikunci komentar + regresi.

## 3. Diverifikasi

- Unit `test_builtins` (valid + 4 penolakan tipe/arity).
- `e2e_concurrency` +3 (penuh/kosong-isi-ulang, cap-0 fail-fast,
  unbounded-selalu-true); concurrency/stress/memsafe + release
  hijau; `.ns` + IR OK.
- `nusa test tests/` (actor memakai channel tak-terbatas —
  perilaku identik).

## 4. Ditunda jujur

- try_send bertag (bool/string), blocking-send ber-timeout,
  `select`, mailbox aktor bounded, spesialisasi/escape
  (prasyarat analisis), devirtualisasi (N/A di MIR).
