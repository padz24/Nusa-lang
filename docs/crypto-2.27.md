# Cryptography Foundation — Design (Phase 76, target 2.27.0)

Status: RFC-0011 Proposed → Implementation slice 1 (2 builtin
+ `std/crypto.ns`). Bukan pustaka kripto umum (tanpa AES/HMAC/
base64 — butuh `ord`, ditunda jujur).

## 1. Hasil evaluasi gate kualitas

| Prioritas master | Putusan | Dasar |
|---|---|---|
| hashing | LULUS | FIPS 180-4 verbatim di runtime; vektor NIST + cross-check independen |
| randomness | LULUS | `/dev/urandom` langsung; gagal-eksplisit (`""`), tanpa fallback diam |
| key representations | LULUS | hex string (codec `encoding.ns` sudah ada) |
| secure comparison | LULUS | tanpa early-exit murni-Nusa (bocor panjang didokumentasikan) |
| encoding (base64) | GAGAL | butuh `ord` (blokir bahasa, tercatat) |
| klaim "teraudit" | GAGAL | dinyatakan apa adanya: provenance + vektor, bukan audit |

## 2. Kontrak builtin

- `sha256_hex(s: string) -> string`: 64 hex lowercase; input =
  byte string (`strlen`; NUL mustahil — konsisten batas lexer).
  Deterministik total.
- `crypto_rand_hex(n: i64) -> string`: `2*n` hex; `""` bila
  `n <= 0`, malloc gagal, atau `/dev/urandom` tak terbaca
  penuh. PANGGILAN WAJIB dicek panjangnya (std melakukannya).
- Keduanya: arity/tipe dicek checker (cermin `file_read`/
  `int_to_str`); malloc tanpa free (konvensi runtime).

## 3. Lapisan std (`crypto.ns`, tanpa import)

- `crypto_key_hex(n)`: kunci `n` byte; panjang salah → die
  (fail-closed di sini, bukan di primitif).
- `crypto_compare(a, b) -> bool`: pindai SEMUA char, akumulasi
  beda (tanpa early-exit); panjang beda → false.
- `sha256_file(path)`: `sha256_hex(file_read(path))`.

## 4. Peta implementasi

`runtime/rt_crypto.c` (baru; include di `nusa_rt.c`) →
`Builtin.cpp` + `TypeChecker` + `Codegen` (pola `file_read`) →
`std/crypto.ns` → `test_builtins` + `e2e_crypto.sh` →
docs + CHANGELOG + regen.

## 5. Acceptance criteria

- Vektor NIST hijau (unit + e2e) + cross-check independen;
  keygen panjang/unik/fail-closed; compare benar; misuse
  bertipe ditolak; suite hijau; bench identik; `.ns.ll` bersih.
