/* Nusa Runtime 2.0 — Synchronization: mutex + atomic (dipindah verbatim dari nusa_rt.c, Phase 55). */
/* Satu translation unit via nusa_rt.c; JANGAN dikompilasi terpisah. */

/* ---------- mutex ---------- */

typedef struct {
  pthread_mutex_t mu;
} nusa_mutex_t;

int64_t nusa_mutex_new(void) {
  nusa_mutex_t *m = (nusa_mutex_t *)malloc(sizeof(nusa_mutex_t));
  if (!m)
    return 0;
  if (pthread_mutex_init(&m->mu, NULL) != 0) {
    free(m);
    return 0;
  }
  nusa_hadd((int64_t)m);
  return (int64_t)m;
}

void nusa_mutex_lock(int64_t handle) {
  nusa_mutex_t *m = (nusa_mutex_t *)handle;
  nusa_hcheck(handle, "mutex_lock");
  pthread_mutex_lock(&m->mu);
}

void nusa_mutex_unlock(int64_t handle) {
  nusa_mutex_t *m = (nusa_mutex_t *)handle;
  nusa_hcheck(handle, "mutex_unlock");
  pthread_mutex_unlock(&m->mu);
}

/* 1 bila terkunci sekarang, 0 bila sibuk. */
int64_t nusa_mutex_try_lock(int64_t handle) {
  nusa_mutex_t *m = (nusa_mutex_t *)handle;
  nusa_hcheck(handle, "mutex_try_lock");
  return pthread_mutex_trylock(&m->mu) == 0 ? 1 : 0;
}

void nusa_mutex_free(int64_t handle) {
  nusa_mutex_t *m = (nusa_mutex_t *)handle;
  if (!nusa_hremove(handle))
    nusa_hdie("mutex_free");
  pthread_mutex_destroy(&m->mu);
  free(m);
}

/* ---------- atomic i64 ---------- */

typedef struct {
  int64_t value;
} nusa_atomic_t;

int64_t nusa_atomic_new(int64_t value) {
  nusa_atomic_t *a = (nusa_atomic_t *)malloc(sizeof(nusa_atomic_t));
  if (!a)
    return 0;
  __atomic_store_n(&a->value, value, __ATOMIC_SEQ_CST);
  nusa_hadd((int64_t)a);
  return (int64_t)a;
}

int64_t nusa_atomic_get(int64_t handle) {
  nusa_atomic_t *a = (nusa_atomic_t *)handle;
  nusa_hcheck(handle, "atomic_get");
  return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST);
}

void nusa_atomic_set(int64_t handle, int64_t value) {
  nusa_atomic_t *a = (nusa_atomic_t *)handle;
  nusa_hcheck(handle, "atomic_set");
  __atomic_store_n(&a->value, value, __ATOMIC_SEQ_CST);
}

/* Mengembalikan nilai BARU (old + delta). */
int64_t nusa_atomic_add(int64_t handle, int64_t delta) {
  nusa_atomic_t *a = (nusa_atomic_t *)handle;
  nusa_hcheck(handle, "atomic_add");
  return __atomic_add_fetch(&a->value, delta, __ATOMIC_SEQ_CST);
}

void nusa_atomic_free(int64_t handle) {
  if (!nusa_hremove(handle))
    nusa_hdie("atomic_free");
  free((nusa_atomic_t *)handle);
}
