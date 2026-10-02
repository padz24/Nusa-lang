# RFC-0011: Cryptography Builtins (slice 1)

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-11
- Menggantikan: —
- Digantikan oleh: —

## Summary

Dua builtin global untuk fondasi kriptografi (mandat Phase 76,
gate kualitas "hanya jika dapat dijamin"): `sha256_hex(s)` dan
`crypto_rand_hex(n)`. Tanpa algoritma baru: SHA-256 = FIPS 180-4
tak termodifikasi; entropi = `/dev/urandom` OS (gagal eksplisit,
tanpa fallback diam-diam).

## Motivation

Yang sakit hari ini: hashing dan randomness kripto tak mungkin
murni-Nusa (tanpa builtin `ord`/bitwise — tercatat di
`docs/stdlib-2.26.md` dan `random.ns` "NOT cryptographic").
`random_u64` membaca `/dev/urandom` tetapi reduksi-nya di Nusa
(LCG, bias modulo) sehingga tak layak kunci.

## Design

- `sha256_hex(s: string) -> string`: hex lowercase digest
  32 byte atas byte string (NUL mustahil di string Nusa —
  konsisten dgn batas lexer). Implementasi C di runtime
  (FIPS 180-4 verbatim, single-purpose); vektor NIST dikunci
  di unit + e2e (+ cross-check implementasi independen).
- `crypto_rand_hex(n: i64) -> string`: `2*n` hex dari
  `/dev/urandom`; GAGAL EKSPLISIT (`""`) bila sumber tak
  terbaca; `n <= 0` → `""`. Tanpa fallback `rand()`
  (berbeda dari `random_u64` — disengaja untuk kripto).
- Lapisan std (`crypto.ns`, murni Nusa): `crypto_key_hex`
  (fail-closed: panjang salah → die), `crypto_compare`
  (tanpa early-exit; beda panjang → false + bocor panjang
  didokumentasikan), `sha256_file` (komposisi).

## Alternatives

- Link libcrypto (OpenSSL ada di host): DITOLAK — dependensi
  eksternal baru untuk semua biner (melanggar "portabel";
  rapuh untuk cross-compilation).
- Hash di codegen via LLVM `SHA256.h`: DITOLAK — hanya untuk
  string konstan kompilasi; string dinamis butuh runtime.
- Hash murni-Nusa: DITOLAK — butuh `ord`/bitwise (RFC lain).
- Base64/AES/HMAC: DITUNDA (butuh `ord`; bukan slice ini).

## Compatibility

Aditif: 2 builtin + 1 file runtime + 1 modul std. Tanpa
perubahan sintaks/IR/ABI. Klaim audit: TIDAK mengklaim
"teraudit" — provenance + vektor + batas ditulis eksplisit.

## Tests

- `test_builtins`: vektor NIST (kosong/abc/448-bit/2-blok)
  + rand (panjang, heks, gagal-eksplisit via n≤0).
- `e2e_crypto.sh`: vektor run, cross-check implementasi
  independen (bila ada), keygen (panjang + unik + fail-closed),
  compare, file hash, misuse bertipe.

## Specification

Spec bahasa: +2 builtin global (non-breaking).
`docs/standard-library.md` + `docs/crypto-2.27.md`.

## Discussion

- 2026-09-11: gate kualitas LULUS untuk hashing (FIPS
  verbatim + vektor) dan randomness (OS entropy, fail-explicit);
  GAGAL untuk klaim "teraudit" (dinyatakan apa adanya).
