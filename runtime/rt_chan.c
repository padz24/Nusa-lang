/* Nusa Runtime 2.0 — Channels: bertag i64/bool/string (dipindah verbatim dari nusa_rt.c, Phase 55). */
/* Satu translation unit via nusa_rt.c; JANGAN dikompilasi terpisah. */

/* ---------- channel bertag (2.0: i64/bool/string) ---------- */
/* Tag payload: 0 = i64, 1 = bool (0/1), 2 = string (pointer).
 * Tag dicatat di handle saat create dan diverifikasi di send/recv:
 * salah tipe = pesan bersih + exit(1) (cermin OOB/div-0), BUKAN
 * garbage diam. chan_new/send/recv lama = tag 0 (kompatibel). */

typedef struct nusa_node {
  int64_t value;
  struct nusa_node *next;
} nusa_node_t;

typedef struct {
  pthread_mutex_t mu;
  pthread_cond_t cond;
  nusa_node_t *head;
  nusa_node_t *tail;
  int64_t tag;
  int64_t size; /* Phase 56 (2.7.0): panjang antre O(1) untuk chan_len */
  int64_t cap;  /* Phase 118 (3.19.0): 0 = tak-terbatas; >0 = bounded */
} nusa_chan_t;

static void nusa_chan_die(const char *what) {
  fprintf(stderr, "runtime error: channel type mismatch in %s\n", what);
  fprintf(stderr,
          "help: use chan_new/chan_send/chan_recv for i64, "
          "chan_new_bool/chan_send_bool/chan_recv_bool for bool, "
          "chan_new_str/chan_send_str/chan_recv_str for string\n");
  exit(1);
}

static nusa_chan_t *nusa_chan_check(int64_t handle, int64_t want,
                                    const char *what) {
  nusa_chan_t *c = (nusa_chan_t *)handle;
  if (!c || c->tag != want)
    nusa_chan_die(what);
  return c;
}

int64_t nusa_chan_new_tagged(int64_t tag) {
  nusa_chan_t *c = (nusa_chan_t *)malloc(sizeof(nusa_chan_t));
  if (!c)
    return 0;
  if (pthread_mutex_init(&c->mu, NULL) != 0) {
    free(c);
    return 0;
  }
  if (pthread_cond_init(&c->cond, NULL) != 0) {
    pthread_mutex_destroy(&c->mu);
    free(c);
    return 0;
  }
  c->head = NULL;
  c->tail = NULL;
  c->tag = (tag >= 0 && tag <= 2) ? tag : 0;
  c->size = 0;
  c->cap = 0; /* tak-terbatas (kompatibel pra-3.19) */
  nusa_hadd((int64_t)c);
  return (int64_t)c;
}

int64_t nusa_chan_new(void) { return nusa_chan_new_tagged(0); }

/* Phase 118 (3.19.0): channel bounded untuk backpressure.
 * cap <= 0 = error fail-fast (bukan rendezvous implisit, bukan
 * tak-terbatas-diam — cermin penolakan ambiguitas S1/Phase 87). */
int64_t nusa_chan_new_bounded(int64_t cap) {
  if (cap <= 0) {
    fprintf(stderr, "runtime error: channel capacity must be positive\n");
    fprintf(stderr, "help: use chan_new() for unbounded, or capacity >= 1\n");
    exit(1);
  }
  nusa_chan_t *c = (nusa_chan_t *)malloc(sizeof(nusa_chan_t));
  if (!c)
    return 0;
  if (pthread_mutex_init(&c->mu, NULL) != 0) {
    free(c);
    return 0;
  }
  if (pthread_cond_init(&c->cond, NULL) != 0) {
    pthread_mutex_destroy(&c->mu);
    free(c);
    return 0;
  }
  c->head = NULL;
  c->tail = NULL;
  c->tag = 0; /* bounded = i64 (bertag = follow-up) */
  c->size = 0;
  c->cap = cap;
  nusa_hadd((int64_t)c);
  return (int64_t)c;
}

void nusa_chan_send_tagged(int64_t handle, int64_t tag, int64_t value) {
  nusa_hcheck(handle, "chan_send");
  nusa_chan_t *c = nusa_chan_check(handle, tag, "chan_send");
  nusa_node_t *node = (nusa_node_t *)malloc(sizeof(nusa_node_t));
  if (!node)
    return; /* OOM: kirim hilang (v1; didokumentasikan). */
  node->value = value;
  node->next = NULL;
  pthread_mutex_lock(&c->mu);
  if (c->tail) {
    c->tail->next = node;
    c->tail = node;
  } else {
    c->head = node;
    c->tail = node;
  }
  c->size++;
  pthread_cond_signal(&c->cond);
  pthread_mutex_unlock(&c->mu);
}

void nusa_chan_send(int64_t handle, int64_t value) {
  nusa_chan_send_tagged(handle, 0, value);
}

/* Phase 118 (3.19.0): kirim non-blocking untuk backpressure.
 * Penuh (bounded) = 0 (tak menunggu); tak-terbatas = selalu 1.
 * OOM node = 0 (cermin drop-diam chan_send). Tag dipaksa 0
 * (bertag = follow-up; mismatch = pesan bersih via chan_check). */
int64_t nusa_chan_try_send(int64_t handle, int64_t value) {
  nusa_hcheck(handle, "chan_try_send");
  nusa_chan_t *c = nusa_chan_check(handle, 0, "chan_try_send");
  pthread_mutex_lock(&c->mu);
  if (c->cap > 0 && c->size >= c->cap) {
    pthread_mutex_unlock(&c->mu);
    return 0;
  }
  pthread_mutex_unlock(&c->mu);
  nusa_node_t *node = (nusa_node_t *)malloc(sizeof(nusa_node_t));
  if (!node)
    return 0;
  node->value = value;
  node->next = NULL;
  pthread_mutex_lock(&c->mu);
  /* Cek ulang di bawah lock (penuh antar-celah di atas). */
  if (c->cap > 0 && c->size >= c->cap) {
    pthread_mutex_unlock(&c->mu);
    free(node);
    return 0;
  }
  if (c->tail) {
    c->tail->next = node;
    c->tail = node;
  } else {
    c->head = node;
    c->tail = node;
  }
  c->size++;
  pthread_cond_signal(&c->cond);
  pthread_mutex_unlock(&c->mu);
  return 1;
}

int64_t nusa_chan_recv_tagged(int64_t handle, int64_t tag) {
  nusa_hcheck(handle, "chan_recv");
  nusa_chan_t *c = nusa_chan_check(handle, tag, "chan_recv");
  pthread_mutex_lock(&c->mu);
  while (!c->head)
    pthread_cond_wait(&c->cond, &c->mu);
  nusa_node_t *node = c->head;
  c->head = node->next;
  if (!c->head)
    c->tail = NULL;
  c->size--;
  pthread_mutex_unlock(&c->mu);
  int64_t value = node->value;
  free(node);
  return value;
}

int64_t nusa_chan_recv(int64_t handle) {
  return nusa_chan_recv_tagged(handle, 0);
}

/* Phase 56 (2.7.0): panjang antre sesaat, non-blocking. Nilai benar
 * pada instant pembacaan (di bawah mutex); siap berubah segera —
 * cermin semantik `mutex_try_lock`. Handle basi → trap registry. */
int64_t nusa_chan_len(int64_t handle) {
  nusa_hcheck(handle, "chan_len");
  nusa_chan_t *c = (nusa_chan_t *)handle;
  pthread_mutex_lock(&c->mu);
  int64_t n = c->size;
  pthread_mutex_unlock(&c->mu);
  return n;
}

void nusa_chan_free(int64_t handle) {
  nusa_chan_t *c = (nusa_chan_t *)handle;
  if (!nusa_hremove(handle))
    nusa_hdie("chan_free");
  pthread_mutex_lock(&c->mu);
  nusa_node_t *node = c->head;
  while (node) {
    nusa_node_t *next = node->next;
    free(node);
    node = next;
  }
  pthread_mutex_unlock(&c->mu);
  pthread_mutex_destroy(&c->mu);
  pthread_cond_destroy(&c->cond);
  free(c);
}
