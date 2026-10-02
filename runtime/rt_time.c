/* Nusa Runtime 2.0 — Time & entropy: clock/urandom (dipindah verbatim dari nusa_rt.c, Phase 55). */
/* Satu translation unit via nusa_rt.c; JANGAN dikompilasi terpisah. */

/* ---------- waktu (stdlib 2.0) ---------- */

int64_t nusa_time_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    return 0;
  return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000L);
}

int64_t nusa_time_now_s(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    return 0;
  return (int64_t)ts.tv_sec;
}

/* ---------- entropi (stdlib 2.0) ---------- */

int64_t nusa_random_u64(void) {
  uint64_t v = 0;
  /* /dev/urandom: portabel Bionic/glibc tanpa feature-test macro. */
  FILE *f = fopen("/dev/urandom", "rb");
  if (f) {
    size_t n = fread(&v, 1, sizeof(v), f);
    fclose(f);
    if (n == sizeof(v))
      return (int64_t)v;
  }
  /* Fallback: rand() dua paruh (bukan kriptografis; didokumentasikan). */
  v = ((uint64_t)(uint32_t)rand() << 32) | (uint64_t)(uint32_t)rand();
  return (int64_t)v;
}
