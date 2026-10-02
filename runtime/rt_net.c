/* Nusa Runtime 2.0 — Networking: TCP client + server loopback (dipindah verbatim dari nusa_rt.c, Phase 55). */
/* Satu translation unit via nusa_rt.c; JANGAN dikompilasi terpisah. */

/* ---------- TCP (stdlib 2.0) ----------
 * Socket = fd (i64); -1 = gagal. `recv` mengembalikan buffer heap
 * (program-lifetime, cermin string runtime); "" saat EOF/error.
 */

int64_t nusa_tcp_connect(const char *host, int64_t port) {
  if (!host || port <= 0 || port > 65535)
    return -1;
  char service[16];
  snprintf(service, sizeof(service), "%lld", (long long)port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *list = NULL;
  if (getaddrinfo(host, service, &hints, &list) != 0)
    return -1;
  int fd = -1;
  for (struct addrinfo *ai = list; ai; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(list);
  return (int64_t)fd;
}

int64_t nusa_tcp_send(int64_t fd, const char *data) {
  if (fd < 0 || !data)
    return 0;
  size_t len = strlen(data);
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send((int)fd, data + sent, len - sent, 0);
    if (n <= 0)
      return 0;
    sent += (size_t)n;
  }
  return 1;
}

char *nusa_tcp_recv(int64_t fd, int64_t maxlen) {
  static const char empty_str[1] = {0};
  if (fd < 0 || maxlen <= 0)
    return (char *)empty_str;
  if (maxlen > 1048576)
    maxlen = 1048576; /* batas kewarasan 1 MiB per recv */
  char *buf = (char *)malloc((size_t)maxlen + 1);
  if (!buf)
    return (char *)empty_str;
  ssize_t n = recv((int)fd, buf, (size_t)maxlen, 0);
  if (n <= 0) {
    free(buf);
    return (char *)empty_str;
  }
  buf[n] = 0;
  return buf;
}

void nusa_tcp_close(int64_t fd) {
  if (fd >= 0)
    close((int)fd);
}

/* ---------- TCP listen/accept (2.0) ----------
 * Server loopback-only (127.0.0.1) by-design — cermin filosofi
 * playground (docs/playground.md): default aman; publikasi via
 * reverse-proxy di depan. fd listen = i64; -1 = gagal. */

int64_t nusa_tcp_listen(int64_t port) {
  if (port <= 0 || port > 65535)
    return -1;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)port);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
      listen(fd, 8) != 0) {
    close(fd);
    return -1;
  }
  return (int64_t)fd;
}

int64_t nusa_tcp_accept(int64_t fd) {
  if (fd < 0)
    return -1;
  int c = accept((int)fd, NULL, NULL);
  return (int64_t)c;
}
