/* Nusa Runtime 2.0 — Alokator (Phase 123, v3.24.0).
 *
 * SATU-SATUNYA titik alokasi heap program Nusa: codegen TIDAK PERNAH
 * memanggil malloc langsung (selalu `nusa_alloc`; `nusa_free` untuk
 * simetri masa depan — codegen hari ini tak membebaskan apa pun:
 * heap program-lifetime by-design).
 *
 * Definisi WEAK: target/platform MENIMPA dengan definisi kuat via
 * --link (alokator kustom: arena, pool, penghitung, fail-fast uji)
 * TANPA mengubah compiler. Default = malloc/free libc.
 * Bagian dari TU agregator (jangan kompilasi terpisah).
 */

#ifndef NUSA_RT_ALLOC_H
#define NUSA_RT_ALLOC_H

#include <stddef.h>
#include <stdlib.h>

__attribute__((weak)) void *nusa_alloc(size_t n) { return malloc(n); }
__attribute__((weak)) void nusa_free(void *p) { free(p); }

#endif /* NUSA_RT_ALLOC_H */
