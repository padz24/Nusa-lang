/* Kriptografi runtime (Phase 76, v2.27.0 — RFC-0011 slice 1).
 *
 * Isi: SHA-256 FIPS 180-4 VERBATIM (tanpa modifikasi algoritma;
 * single-purpose, tanpa streaming API) + byte acak OS.
 * Klaim audit: TIDAK diklaim teraudit — provenance + vektor NIST
 * (lihat test_builtins/e2e_crypto) adalah jaminannya.
 * Portabel: C99 murni + fopen (ikut pola rt_time.c).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- SHA-256 (FIPS 180-4 §5-6) ---- */

static uint32_t sh_rotr(uint32_t x, unsigned n) {
  return (x >> n) | (x << (32 - n));
}

static const uint32_t sh_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/* Digest `len` byte; tulis 32 byte ke `out`. */
static void nusa_sha256_raw(const uint8_t *msg, uint64_t len, uint8_t out[32]) {
  uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  /* Padding: pesan + 0x80 + nol + panjang-bit BE 64 (per blok 64). */
  uint64_t total = len + 1 + 8;
  uint64_t npad = (64 - (total % 64)) % 64;
  uint64_t mlen = len + 1 + npad + 8;
  /* Blok dihitung streaming (tanpa alokasi pesan penuh). */
  uint8_t block[64];
  uint64_t off = 0;
  while (off < mlen) {
    size_t i;
    for (i = 0; i < 64; i++) {
      uint64_t p = off + i;
      uint8_t b;
      if (p < len) {
        b = msg[p];
      } else if (p == len) {
        b = 0x80;
      } else if (p >= mlen - 8) {
        /* 8 byte terakhir = panjang-bit big-endian. */
        unsigned sh = (unsigned)(8 * (mlen - 1 - p));
        b = (uint8_t)((len * 8) >> sh);
      } else {
        b = 0x00;
      }
      block[i] = b;
    }
    off += 64;
    uint32_t w[64];
    for (i = 0; i < 16; i++)
      w[i] = ((uint32_t)block[i * 4] << 24) |
             ((uint32_t)block[i * 4 + 1] << 16) |
             ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    for (i = 16; i < 64; i++) {
      uint32_t s0 = sh_rotr(w[i - 15], 7) ^ sh_rotr(w[i - 15], 18) ^
                    (w[i - 15] >> 3);
      uint32_t s1 = sh_rotr(w[i - 2], 17) ^ sh_rotr(w[i - 2], 19) ^
                    (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
             g = h[6], hh = h[7];
    for (i = 0; i < 64; i++) {
      uint32_t s1 = sh_rotr(e, 6) ^ sh_rotr(e, 11) ^ sh_rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + s1 + ch + sh_k[i] + w[i];
      uint32_t s0 = sh_rotr(a, 2) ^ sh_rotr(a, 13) ^ sh_rotr(a, 22);
      uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = s0 + mj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  for (size_t i = 0; i < 8; i++) {
    out[i * 4] = (uint8_t)(h[i] >> 24);
    out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
    out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
    out[i * 4 + 3] = (uint8_t)h[i];
  }
}

static const char sh_hexd[] = "0123456789abcdef";

/* Hex lowercase SHA-256 atas string C (malloc 65 byte; NULL bila gagal).
 * NUL internal mustahil di string Nusa (batas lexer) — strlen konsisten. */
char *nusa_sha256_hex(const char *s) {
  uint8_t digest[32];
  char *out;
  size_t i;
  if (!s)
    s = "";
  nusa_sha256_raw((const uint8_t *)s, (uint64_t)strlen(s), digest);
  out = (char *)malloc(65);
  if (!out)
    return NULL;
  for (i = 0; i < 32; i++) {
    out[i * 2] = sh_hexd[(digest[i] >> 4) & 0xF];
    out[i * 2 + 1] = sh_hexd[digest[i] & 0xF];
  }
  out[64] = '\0';
  return out;
}

/* ---- Entropi OS (fail-explicit; TANPA fallback rand()) ---- */

/* Isi `len` byte dari /dev/urandom; 1 = penuh, 0 = gagal. */
static int nusa_crypto_rand(unsigned char *buf, int64_t len) {
  FILE *f;
  size_t n;
  if (len <= 0)
    return 0;
  f = fopen("/dev/urandom", "rb");
  if (!f)
    return 0;
  n = fread(buf, 1, (size_t)len, f);
  fclose(f);
  return n == (size_t)len;
}

/* Hex lowercase `nbytes` acak OS (malloc 2*n+1; NULL bila n<=0,
 * malloc gagal, atau sumber tak terbaca penuh). */
char *nusa_crypto_rand_hex(int64_t nbytes) {
  unsigned char *raw;
  char *out;
  int64_t i;
  if (nbytes <= 0)
    return NULL;
  raw = (unsigned char *)malloc((size_t)nbytes);
  if (!raw)
    return NULL;
  if (!nusa_crypto_rand(raw, nbytes)) {
    free(raw);
    return NULL;
  }
  out = (char *)malloc((size_t)(2 * nbytes + 1));
  if (!out) {
    free(raw);
    return NULL;
  }
  for (i = 0; i < nbytes; i++) {
    out[i * 2] = sh_hexd[(raw[i] >> 4) & 0xF];
    out[i * 2 + 1] = sh_hexd[raw[i] & 0xF];
  }
  out[2 * nbytes] = '\0';
  free(raw);
  return out;
}
