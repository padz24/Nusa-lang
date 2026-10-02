# NUSA REPL — Milestone 1: AOT-Loop (Phase 106)

Status: slice implementasi (`nusa repl`; target 3.7.0). Prinsip
master: infrastruktur compiler yang sama, tanpa bahasa kedua.
Dipilih AOT-loop (bukan JIT): subsystem JIT belum ada (Phase 105),
sementara kebutuhan iterasi terpenuhi transkrip + rebuild cache.

## 1. Cara kerja (`compiler/driver/Repl.cpp`, `Driver::runRepl`)

- Sesi = daftar baris → `session.ns` (`fn main() { ... }`) di
  `mkdtemp` privat → `runBuild` (checkStages/codegen/clang +
  cache inkremental) → eksekusi → stdout diteruskan.
- Baris gagal (diagnostik compiler di stderr) TIDAK masuk sesi
  (anti-racun); `:reset` membuang sesi; EOF/`:quit` keluar 0 +
  bersih total (termasuk `.nusa-cache`).
- SEMANTIK PUTAR-ULANG (jujur, bawaan AOT-loop): tiap giliran sukses
  mengeksekusi SELURUH sesi dari awal — efek samping (print)
  giliran lama terulang. Untuk nilai, pakai ekspresi; `let` murni
  dan blok bisu tak mencetak. JIT inkremental (Phase 105) akan
  menghilangkan ini (follow-up tercatat).
- Konvensi input (REPL, bukan sintaks baru): akhir `;`/`{`/`}`
  = statement `.ns` apa adanya; sisanya ekspresi → `print(...);`.
- Prompt `nusa> `/`... ` hanya di TTY (terpipa bersih untuk skrip
  + uji). Blok multi-baris via penghitung `{[(` sadar string +
  komentar `//`.

## 2. Batas jujur (milestone 1)

- Tanpa definisi `fn`/`struct`/`trait`/`impl`/`import` (bukan
  statement — ditolak bersih, sesi utuh).
- Tanpa timeout eksekusi (program gantung = Ctrl-C; ikut model
  `nusa run`). Tanpa riwayat/edit baris (readline) — follow-up.
- Biaya per baris = rebuild sesi penuh (~2 dtk clang, tumbuh
  bersama sesi); JIT inkremental (Phase 105) = jalur cepat
  masa depan bila dibutuhkan.

## 3. Diverifikasi

- `e2e_repl` 5/5: sesi+blok, anti-racun, reset/bantuan/
  perintah-asing, tolak-fn, argumen+kebersihan.
- `e2e_release` hijau (help baru tak merusak gate); `.ns` + IR OK.
