# Concurrency 2.0 — Design (Phase 56, target 2.7.0)

Status: audit selesai → Implementation (milestone 1: observability
+ stress). Spec 2.0 tak berubah kecuali satu builtin baru
(non-breaking, aditif). `async`/`await` tetap reserved
(RFC-0004 Proposed; audit §4).

## 1. Audit primitif (2.6.0)

Ada & sehat (e2e_concurrency deterministik): thread spawn/join,
channel i64/bool/string bertag, mutex (+try_lock), atomic seq-cst,
sleep_ms. Registry Phase 54 men-trap misuse sekuensial.

Kesenjangan: channel tak terobservasi (tak ada cara non-blocking
mengetahui isi — `recv` selalu blokir); tanpa stress suite;
tanpa koleksi thread-safe di std; tanpa select/close (tetap
ditunda — butuh desain semantik: blokir multi-channel +
sentinel, milestone tersendiri).

## 2. Milestone 1 (SATU-SATUNYA perubahan perilaku fase ini)

**`chan_len(c: i64) -> i64`** — jumlah pesan antre, non-blocking:

- Runtime (`rt_chan.c`): field `size` di `nusa_chan_t` (++/-- di
  bawah mutex channel yang sama; O(1)); `nusa_chan_len` =
  `hcheck` + lock + baca + unlock. Simbol publik 29 → 30.
- Compiler: `frontend::isBuiltin` + checker (grup arity-1
  i64→i64, cermin `chan_recv`) + codegen (grup i64-return,
  `nusa_chan_len`). Nol sintaks baru.
- Semantik: panjang antrean sesaat (race-tolerant: nilai benar
  pada instant pembacaan di bawah mutex; pemanggil polling harus
  siap nilai berubah — didokumentasikan, cermin `mutex_try_lock`).

**Stress suite** (`e2e_stress.sh`, semua deterministik via
join/collect — tanpa asersi timing):

- fan-out/fan-in: 4 produsen × 25 pesan → total 100 tepat.
- atomic hammer: 8 thread × 5000 `atomic_add` → 40000 tepat.
- mutex contention: 4 thread × 2000 increment terkunci → 8000.
- `chan_len`: 3 send → 3, recv → 2, drain → 0; polling loop
  hingga N pesan tiba (pola tugas non-blocking baru).
- misuse: `chan_len` sesudah `free` → `invalid handle`, exit 1.
- `async` reserved: dipakai sebagai identifier → error bersih.

## 3. DITUNDA beralasan

select/close channel, task executor, work-stealing pool (butuh
desain + RFC bila menyentuh bahasa); koleksi thread-safe std
(menunggu pola `chan_len` terbukti di ekosistem).

## 4. Audit async/await

Keyword reserved sejak 2.0; nol sintaks/AST; `nusa lex` +
parser menolak pemakaian (kasus e2e §2). RFC-0004 tetap Proposed;
implementasi dilarang sebelum Accepted (kontrak Phase 41).

## 5. Acceptance criteria

- `chan_len` berperilaku + misuse di-trap registry.
- `e2e_stress.sh` hijau (stabil diulang 3x) + seluruh suite hijau.
- Simbol runtime 30/30; benchmark `.ns` identik baseline 2.6.0.
