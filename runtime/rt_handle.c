/* Nusa Runtime 2.0 — Shared: registry handle hidup (dipindah verbatim dari nusa_rt.c, Phase 55). */
/* Satu translation unit via nusa_rt.c; JANGAN dikompilasi terpisah. */

/* ---------- registry handle hidup (Phase 54, v2.5.0) ----------
 * Handle = pointer heap sebagai i64 yang bebas dicopy di sisi Nusa;
 * join/free tanpa invalidasi salinan = UAF/double-free (terbukti ASan).
 * Registry ini membuat misuse SEQUENSIAL fail-fast (pesan + exit 1,
 * konsisten OOB/div-0) alih-alih UB. Residual jujur: ABA reuse alamat
 * dan free-while-borrowed konkuren tetap UB (lih. docs/mem-2.5.md).
 */
static pthread_mutex_t nusa_hmu = PTHREAD_MUTEX_INITIALIZER;
static int64_t *nusa_hlive = NULL;
static size_t nusa_hn = 0;
static size_t nusa_hcap = 0;

static void nusa_hdie(const char *what) {
  fprintf(stderr, "runtime error: invalid handle in %s\n", what);
  fprintf(stderr,
          "help: each handle must be used while live and freed/joined "
          "exactly once (copies do not extend lifetime)\n");
  exit(1);
}

static void nusa_hadd(int64_t h) {
  if (!h)
    return;
  pthread_mutex_lock(&nusa_hmu);
  if (nusa_hn == nusa_hcap) {
    size_t ncap = nusa_hcap ? nusa_hcap * 2 : 16;
    int64_t *nl = (int64_t *)realloc(nusa_hlive, ncap * sizeof(int64_t));
    if (!nl) {
      /* Fail-closed: tanpa pelacakan, handle tak boleh hidup. */
      pthread_mutex_unlock(&nusa_hmu);
      fprintf(stderr, "runtime error: out of memory (handle registry)\n");
      exit(1);
    }
    nusa_hlive = nl;
    nusa_hcap = ncap;
  }
  nusa_hlive[nusa_hn++] = h;
  pthread_mutex_unlock(&nusa_hmu);
}

/* 1 bila terdaftar, 0 bila tidak (ganda/basi/nol). */
static int nusa_hhas(int64_t h) {
  size_t i;
  if (!h)
    return 0;
  pthread_mutex_lock(&nusa_hmu);
  for (i = 0; i < nusa_hn; i++) {
    if (nusa_hlive[i] == h) {
      pthread_mutex_unlock(&nusa_hmu);
      return 1;
    }
  }
  pthread_mutex_unlock(&nusa_hmu);
  return 0;
}

static void nusa_hcheck(int64_t h, const char *what) {
  if (!nusa_hhas(h))
    nusa_hdie(what);
}

/* Keluarkan dari registry; 1 = ada, 0 = ganda/basi/nol. */
static int nusa_hremove(int64_t h) {
  size_t i;
  int found = 0;
  if (!h)
    return 0;
  pthread_mutex_lock(&nusa_hmu);
  for (i = 0; i < nusa_hn; i++) {
    if (!found && nusa_hlive[i] == h) {
      found = 1;
    }
    if (found && i + 1 < nusa_hn)
      nusa_hlive[i] = nusa_hlive[i + 1];
  }
  if (found)
    nusa_hn--;
  pthread_mutex_unlock(&nusa_hmu);
  return found;
}
