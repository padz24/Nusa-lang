/* Nusa Runtime 2.0 — Threads: spawn/join/sleep (dipindah verbatim dari nusa_rt.c, Phase 55). */
/* Satu translation unit via nusa_rt.c; JANGAN dikompilasi terpisah. */

/* ---------- thread ---------- */

typedef struct {
  pthread_t tid;
} nusa_thread_t;

int64_t nusa_thread_spawn(void *entry, void *env) {
  nusa_thread_t *h =
      (nusa_thread_t *)malloc(sizeof(nusa_thread_t));
  if (!h)
    return 0;
  /* entry: trampolin Nusa `(ptr) -> i64`; signature pthread
   * `(void*) -> void*` ABI-kompatibel (LP64): cast eksplisit. */
  void *(*start)(void *) = (void *(*)(void *))entry;
  if (pthread_create(&h->tid, NULL, start, env) != 0) {
    free(h);
    return 0;
  }
  nusa_hadd((int64_t)h);
  return (int64_t)h;
}

int64_t nusa_thread_join(int64_t handle) {
  nusa_thread_t *h = (nusa_thread_t *)handle;
  /* Klaim DULU (atomik di bawah mutex registry): ganda/basi/nol DAN
   * kalah-balapan double-join konkuren ditolak di sini — SEBELUM
   * pthread_join yang memblokir. Pola sama dengan *_free (Phase 54);
   * join lama check-dulu-hapus-belakangan = jendela TOCTOU
   * (double-free + nilai 0 diam-diam, terbukti Phase 116).
   * Pemenang klaim melanjutkan; yang kalah exit(1) fail-fast
   * (konsisten: salinan tak memperpanjang lifetime). */
  if (!nusa_hremove(handle))
    nusa_hdie("thread_join");
  void *retval = NULL;
  if (pthread_join(h->tid, &retval) != 0) {
    free(h);
    return 0;
  }
  free(h);
  /* retval sejatinya i64 hasil trampolin (di x0); baca kembali. */
  return (int64_t)retval;
}

/* ---------- sleep ---------- */

void nusa_sleep_ms(int64_t ms) {
  if (ms <= 0)
    return;
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000);
  ts.tv_nsec = (long)((ms % 1000) * 1000000L);
  nanosleep(&ts, NULL);
}
