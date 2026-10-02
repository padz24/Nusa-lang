/* Shim TLS client untuk FFI Nusa (paket referensi ekosistem).
 *
 * `extern fn` hanya mengenal string/i64/bool: koneksi TLS hidup di
 * tabel slot (handle = i64); hasil baca ditulis ke file lalu dibaca
 * via file_read (pola packages/sqlite). OpenSSL sistem (link
 * -lssl -lcrypto); verifikasi peer + hostname SELALU aktif
 * (tanpa mode insecure — kegagalan verifikasi = -1).
 */
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define NTLS_MAX 16
static int ntls_fd[NTLS_MAX];
static SSL *ntls_ssl[NTLS_MAX];
static int ntls_used[NTLS_MAX] = {0};

static int ntls_tcp(const char *host, long long port) {
  struct addrinfo hints, *res, *rp;
  char ps[32];
  int fd = -1;
  if (!host)
    return -1;
  snprintf(ps, sizeof(ps), "%lld", port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, ps, &hints, &res) != 0)
    return -1;
  for (rp = res; rp; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

static int ntls_free_slot(void) {
  int i;
  for (i = 0; i < NTLS_MAX; i++)
    if (!ntls_used[i])
      return i;
  return -1;
}

static void ntls_drop(int i) {
  if (ntls_ssl[i]) {
    SSL_shutdown(ntls_ssl[i]);
    SSL_free(ntls_ssl[i]);
    ntls_ssl[i] = 0;
  }
  if (ntls_fd[i] >= 0) {
    close(ntls_fd[i]);
    ntls_fd[i] = -1;
  }
  ntls_used[i] = 0;
}

/* Dial TLS + verifikasi penuh; cafile "" = path default sistem.
 * Id slot atau -1. */
long long ntls_dial(const char *host, long long port, const char *cafile) {
  SSL_CTX *ctx;
  SSL *ssl;
  int fd, i;
  if (!host)
    return -1;
  i = ntls_free_slot();
  if (i < 0)
    return -1;
  fd = ntls_tcp(host, port);
  if (fd < 0)
    return -1;
  ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    close(fd);
    return -1;
  }
  if (cafile && cafile[0]) {
    if (SSL_CTX_load_verify_file(ctx, cafile) != 1) {
      SSL_CTX_free(ctx);
      close(fd);
      return -1;
    }
  } else {
    if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
      SSL_CTX_free(ctx);
      close(fd);
      return -1;
    }
  }
  ssl = SSL_new(ctx);
  SSL_CTX_free(ctx);
  if (!ssl) {
    close(fd);
    return -1;
  }
  SSL_set1_host(ssl, host);
  SSL_set_verify(ssl, SSL_VERIFY_PEER, 0);
  SSL_set_fd(ssl, fd);
  if (SSL_connect(ssl) != 1 || SSL_get_verify_result(ssl) != X509_V_OK) {
    SSL_free(ssl);
    close(fd);
    return -1;
  }
  ntls_fd[i] = fd;
  ntls_ssl[i] = ssl;
  ntls_used[i] = 1;
  return (long long)i;
}

static int ntls_ok(long long id) {
  return id >= 0 && id < NTLS_MAX && ntls_used[(int)id];
}

/* Kirim semua byte; jumlah terkirim atau -1. */
long long ntls_send(long long id, const char *data) {
  size_t len, off = 0;
  int w;
  if (!ntls_ok(id) || !data)
    return -1;
  len = strlen(data);
  while (off < len) {
    w = SSL_write(ntls_ssl[(int)id], data + off, (int)(len - off));
    if (w <= 0)
      return -1;
    off += (size_t)w;
  }
  return (long long)off;
}

/* Terima sampai EOF ke `path`; jumlah byte atau -1. */
long long ntls_recv_dump(long long id, const char *path) {
  FILE *f;
  long long n = 0;
  int r;
  char buf[4096];
  if (!ntls_ok(id) || !path)
    return -1;
  f = fopen(path, "w");
  if (!f)
    return -1;
  while ((r = SSL_read(ntls_ssl[(int)id], buf, sizeof(buf))) > 0) {
    if (fwrite(buf, 1, (size_t)r, f) != (size_t)r) {
      fclose(f);
      return -1;
    }
    n += r;
  }
  fclose(f);
  return r == 0 ? n : -1;
}

/* Tutup koneksi; 0 ok, -1 id buruk. */
long long ntls_close(long long id) {
  if (!ntls_ok(id))
    return -1;
  ntls_drop((int)id);
  return 0;
}

/* Satu-shot: dial + kirim + terima-sampai-EOF + tutup.
 * Byte diterima atau -1 (pola respons request/response). */
long long ntls_request(const char *host, long long port, const char *cafile,
                       const char *request, const char *path) {
  long long id, s, n;
  if (!request || !path)
    return -1;
  id = ntls_dial(host, port, cafile);
  if (id < 0)
    return -1;
  s = ntls_send(id, request);
  n = s < 0 ? -1 : ntls_recv_dump(id, path);
  ntls_close(id);
  return n;
}
