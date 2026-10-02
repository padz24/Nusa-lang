# Actor / Message-Passing — Milestone 1: Pustaka (Phase 117)

Status: slice implementasi level-pustaka (target 3.18.0). Mandat
master: bila bermanfaat, model actor/message-passing sebagai
LIBRARY atau abstraksi dukungan-bahasa; JANGAN fitur inti tanpa
RFC; pustaka DULU. Sintaks `actor Counter {...}` konseptual =
Bukan slice ini (butuh RFC).

## 1. Desain (`std/actor.ns`, murni Nusa, ~70 baris)

- `struct ActorRef { mb, lock }`: mailbox FIFO + mutex urutan.
- `actor_spawn(h, init)`: thread loop `actor_loop` (closure
  capture mailbox/handler/state — pola `par_map2` yang terbukti).
- Handler `fn(op, arg, reply, state) -> state-baru`: menjawab
  SENDIRI (`chan_send(reply, v)`), mengembalikan state (fungsional
  — selaras owned-values; tanpa mutat bersama).
- `actor_call` sinkron (triple-kirim atomik + tunggu);
  `actor_cast` tanpa-tunggu (reply-chan dibuang; handler tetap
  menjawab — protokol seragam).
- Kunci kebenaran: mutex-mailbox membuat triple (op,arg,reply)
  atomik antar-klien; tanpanya FIFO bersama interleave dan
  request korup (stres §3 memberi bukti positif: 4×25 = tepat
  100 hanya bila protokol atomik).

## 2. Batas jujur (level-pustaka)

- Hidup selama proses (tanpa stop — konsisten heap model);
  tanpa selective-receive; self-call sinkron = deadlock klasik
  (terdokumentasikan, bukan dideteksi).
- Pesan = i64 (tag + arg + reply-handle; handle ADALAH i64).
  Payload kaya (string/struct) = follow-up (butuh ownership
  lintas-aktor yang dirancang).
- Sintaks `actor` + supervision/linking + pola OTP = butuh RFC
  (di luar cakupan by-design).

## 3. Diverifikasi

- `tests/actor_test.ns` via `nusa test` (counter/isolasi/cast).
- `e2e_actor` 3/3: test-file, stres deterministik 100,
  `import std.actor` + fmt-clean.
- `e2e_concurrency`/`stress`/`stdlib3` + `e2e_release` hijau;
  `.ns` + IR OK (NOL jalur compiler — aditif std).

## 4. Ditunda jujur

- Sintaks + RFC, payload non-i64, stop/supervisi/selective,
  pool aktor, distribusi (ranah Phase 118 bila ada kebutuhan).
