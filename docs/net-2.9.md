# Networking — Design (Phase 58, target 2.9.0)

Status: audit → Implementation (milestone 1: HTTP primitives murni
Nusa di atas TCP loopback). Tanpa framework web di compiler
(mandat): semua di `std/`. Spec tak berubah (stdlib aditif).

## 1. Audit lapisan net (2.8.0)

Ada: TCP client (`dial/send/recv/close`), server loopback
(`listen_on/accept_one`), `recv_line`; DNS via getaddrinfo di
`dial` (nama host, bukan hanya IP — teruji e2e_stdlib2? hanya
localhost; DNS eksternal butuh jaringan, di luar CI).
Belum: UDP, DNS API eksplisit, HTTP, WebSocket.

## 2. Milestone 1 (SATU-SATUNYA penambahan fase ini)

**`std/http.ns`** (murni Nusa, nol builtin/compiler baru):

- `http_status(resp) -> i64` (baris status → kode; -1 bila
  malformed), `http_body(resp) -> string` (setelah CRLFCRLF/LFLF).
- `http_get(host, port, path) -> string` (GET/1.0, baca hingga
  close; `""` bila dial gagal).
- `http_serve_once(port, body) -> i64` (listen→accept→baca
  request→kirim `200 OK` + Content-Length→tutup; 0 ok, -1 gagal).
- Helper privat `_dial_retry`, `_read_all`, `_read_request`
  (prefix `_` = privat antar-file).

Pola uji hermetik (tanpa python/jaringan luar): server Nusa di
thread + client Nusa + `dial_retry` (200×5ms), loopback
127.0.0.1, port uji 18080/18081. Deterministik via join.

## 3. DITUNDA beralasan

UDP (`SOCK_DGRAM` + builtin `udp_*`: butuh plumbing 3 lapis —
milestone tersendiri), DNS API eksplisit (di atas getaddrinfo),
HTTP client async/timeout, WebSocket (butuh SHA-1 + framing —
setelah crypto Phase 76), framework router (ekosistem, bukan std).

## 4. Temuan implementasi

- `import std.string` TAK TERPARSE (`string` keyword; segmen path
  butuh identifier) — gunakan `import string;` (alias resmi).
  Sama kelasnya dengan `std.async` (Phase 57). Kandidatichever:
  izinkan keyword sebagai segmen path (parser, follow-up kecil).

## 5. Acceptance criteria

- Parse unit (200/404/malformed/`""`) + round-trip
  serve-once→client hermetik hijau (`e2e_http.sh`).
- `fmt --check` + `doc` + seluruh suite hijau; bench identik.
