// Playground HTTP loopback + perintah sandbox (Phase 42).
// Arsitektur master: Browser → Playground → Compiler Service →
// Sandbox → Nusa Program. Di sini "Compiler Service" = compiler ini
// sendiri; "Sandbox" = compiler/driver/Sandbox.cpp.
//
// Keamanan: hanya 127.0.0.1; cap request/body; eksekusi SELALU via
// sandbox (timeout + rlimit + tmpdir + env scrub). Tanpa auth karena
// loopback-only (deployment publik = reverse-proxy + auth di depan;
// lihat docs/playground.md).
#include "compiler/driver/Driver.h"
#include "compiler/driver/Sandbox.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace nusa {
namespace {

// Cap HTTP.
constexpr size_t kMaxHeader = 16384;
constexpr size_t kMaxBody = 65536; // sumber .ns (program wajar < 64KB)

const char kPage[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<title>Nusa Playground</title></head><body>"
    "<h1>Nusa Playground (local sandbox)</h1>"
    "<textarea id=s rows=12 cols=60>fn main() {\n"
    "    print(\"Hello, Nusa!\");\n}\n</textarea><br>"
    "<button onclick=\"run()\">Run</button><pre id=o></pre>"
    "<script>async function run(){const r=await fetch('/api/run',"
    "{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({source:document.getElementById('s').value})});"
    "document.getElementById('o').textContent=await r.text();}</script>"
    "</body></html>";

// Escape string untuk JSON (" + \ + kontrol).
std::string jsonEscape(const std::string &s) {
  std::string o;
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      o += "\\\"";
      break;
    case '\\':
      o += "\\\\";
      break;
    case '\n':
      o += "\\n";
      break;
    case '\r':
      o += "\\r";
      break;
    case '\t':
      o += "\\t";
      break;
    default:
      if (c < 0x20) {
        char b[8];
        snprintf(b, sizeof(b), "\\u%04x", c);
        o += b;
      } else {
        o.push_back((char)c);
      }
    }
  }
  return o;
}

// Ekstrak nilai kunci "source" (JSON string) dari body SECARA
// MINIMAL: cari "source", lalu parse string JSON setelah ':'.
// False bila bentuk tak dikenali (bukan parser JSON umum).
bool extractSource(const std::string &body, std::string &out) {
  size_t k = body.find("\"source\"");
  if (k == std::string::npos)
    return false;
  size_t c = body.find(':', k + 8);
  if (c == std::string::npos)
    return false;
  size_t q = body.find('"', c + 1);
  if (q == std::string::npos)
    return false;
  std::string v;
  for (size_t i = q + 1; i < body.size(); i++) {
    char ch = body[i];
    if (ch == '"')
      break;
    if (ch == '\\' && i + 1 < body.size()) {
      char e = body[++i];
      switch (e) {
      case 'n':
        v.push_back('\n');
        break;
      case 'r':
        v.push_back('\r');
        break;
      case 't':
        v.push_back('\t');
        break;
      case 'u': // \uXXXX — ambil ASCII saja, sisanya '?'
        if (i + 4 < body.size()) {
          unsigned cp = 0;
          bool hex = true;
          for (int d = 1; d <= 4; d++) {
            char h = body[i + d];
            cp <<= 4;
            if (h >= '0' && h <= '9')
              cp += (unsigned)(h - '0');
            else if (h >= 'a' && h <= 'f')
              cp += (unsigned)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
              cp += (unsigned)(h - 'A' + 10);
            else
              hex = false;
          }
          v.push_back((hex && cp < 128) ? (char)cp : '?');
          i += 4;
        } else {
          v.push_back('?');
        }
        break;
      default:
        v.push_back(e);
        break;
      }
    } else {
      v.push_back(ch);
    }
    if (v.size() > kMaxBody)
      return false;
  }
  out = v;
  return true;
}

void sendAll(int fd, const std::string &s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t w = send(fd, s.data() + off, s.size() - off, 0);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    off += (size_t)w;
  }
}

void respond(int fd, int code, const char *phrase, const std::string &ctype,
             const std::string &body) {
  char head[256];
  snprintf(head, sizeof(head),
           "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
           "Connection: close\r\n\r\n",
           code, phrase, ctype.c_str(), body.size());
  std::string resp(head);
  resp += body;
  sendAll(fd, resp);
}

// Baca request lengkap (header + body Content-Length). False bila
// melewati cap / koneksi rusak.
bool readRequest(int fd, std::string &method, std::string &path,
                 std::string &body) {
  std::string acc;
  char buf[4096];
  size_t headerEnd = std::string::npos;
  size_t contentLen = 0;
  bool haveLen = false;
  while (true) {
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r <= 0)
      return false;
    acc.append(buf, r);
    if (acc.size() > kMaxHeader + kMaxBody)
      return false;
    if (headerEnd == std::string::npos) {
      size_t e = acc.find("\r\n\r\n");
      if (e == std::string::npos) {
        if (acc.size() > kMaxHeader)
          return false;
        continue;
      }
      headerEnd = e + 4;
      // Baris request.
      size_t sp1 = acc.find(' ');
      size_t sp2 = acc.find(' ', sp1 + 1);
      size_t eol = acc.find("\r\n");
      if (sp1 == std::string::npos || sp2 == std::string::npos ||
          eol == std::string::npos || sp2 > eol)
        return false;
      method = acc.substr(0, sp1);
      path = acc.substr(sp1 + 1, sp2 - sp1 - 1);
      // Content-Length (case-insensitive sederhana).
      std::string low = acc.substr(0, headerEnd);
      for (auto &ch : low)
        ch = (char)tolower((unsigned char)ch);
      size_t cl = low.find("content-length:");
      if (cl != std::string::npos && cl < headerEnd) {
        size_t v = cl + 15;
        while (v < low.size() && (low[v] == ' ' || low[v] == '\t'))
          v++;
        contentLen = (size_t)strtoul(low.c_str() + v, nullptr, 10);
        haveLen = true;
        if (contentLen > kMaxBody)
          return false;
      }
    }
    if (headerEnd != std::string::npos &&
        (!haveLen || acc.size() >= headerEnd + contentLen))
      break;
  }
  body = haveLen ? acc.substr(headerEnd, contentLen) : "";
  return true;
}

} // namespace

int Driver::runSandbox(const std::string &inputPath, int timeoutSec,
                       long memoryMb, long outputKb) {
  sandbox::SandboxLimits lim;
  lim.timeoutSec = timeoutSec;
  lim.memoryMb = memoryMb;
  lim.outputKb = outputKb;
  sandbox::SandboxResult r = sandbox::runSandboxed(inputPath, lim);
  if (!r.output.empty())
    std::fwrite(r.output.data(), 1, r.output.size(), stdout);
  std::printf("\n[sandbox: verdict=%s code=%d]\n", r.verdict.c_str(), r.code);
  return (r.verdict == "ok" && r.code == 0) ? 0 : 1;
}

int Driver::runPlayground(int port, int timeoutSec, long memoryMb,
                          long outputKb) {
  // Abaikan SIGPIPE (client menutup duluan) — server tetap hidup.
  signal(SIGPIPE, SIG_IGN);
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) {
    std::fprintf(stderr, "error: playground socket failed\n");
    return 1;
  }
  int one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // loopback SAJA
  addr.sin_port = htons((uint16_t)port);
  if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    std::fprintf(stderr, "error: playground bind 127.0.0.1:%d failed: %s\n",
                 port, strerror(errno));
    close(srv);
    return 1;
  }
  if (port == 0) {
    socklen_t len = sizeof(addr);
    if (getsockname(srv, (struct sockaddr *)&addr, &len) == 0)
      port = ntohs(addr.sin_port);
  }
  if (listen(srv, 8) != 0) {
    std::fprintf(stderr, "error: playground listen failed\n");
    close(srv);
    return 1;
  }
  std::printf("playground: listening on 127.0.0.1:%d (sandbox: %ds, %ldMB)\n",
              port, timeoutSec, memoryMb);
  std::fflush(stdout);

  sandbox::SandboxLimits lim;
  lim.timeoutSec = timeoutSec;
  lim.memoryMb = memoryMb;
  lim.outputKb = outputKb;
  int reqNo = 0;
  while (true) {
    int fd = accept(srv, nullptr, nullptr);
    if (fd < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    reqNo++;
    std::string method, path, body;
    if (!readRequest(fd, method, path, body)) {
      respond(fd, 413, "Payload Too Large", "text/plain",
              "request too large or malformed\n");
      close(fd);
      continue;
    }
    if (method == "GET" && path == "/") {
      respond(fd, 200, "OK", "text/html; charset=utf-8", kPage);
    } else if (method == "POST" && path == "/api/run") {
      std::string source;
      if (!extractSource(body, source) || source.empty()) {
        respond(fd, 400, "Bad Request", "application/json",
                "{\"ok\":false,\"verdict\":\"bad-request\","
                "\"output\":\"need JSON {\\\"source\\\": \\\"...\\\"}\"}");
      } else {
        // Tulis sumber ke file sementara, lalu via sandbox.
        char tmpl[] = "/tmp/nusa-play-XXXXXX.ns";
        int tf = mkstemps(tmpl, 3);
        std::string srcPath;
        if (tf >= 0) {
          size_t off = 0;
          while (off < source.size()) {
            ssize_t w =
                write(tf, source.data() + off, source.size() - off);
            if (w < 0) {
              if (errno == EINTR)
                continue;
              break;
            }
            off += (size_t)w;
          }
          close(tf);
          srcPath = tmpl;
        }
        if (srcPath.empty()) {
          respond(fd, 500, "Error", "application/json",
                  "{\"ok\":false,\"verdict\":\"signal\","
                  "\"output\":\"temp file failed\"}");
        } else {
          sandbox::SandboxResult r =
              sandbox::runSandboxed(srcPath, lim);
          unlink(srcPath.c_str());
          std::string jb =
              std::string("{\"ok\":") + (r.verdict == "ok" && r.code == 0
                                             ? "true"
                                             : "false") +
              ",\"verdict\":\"" + jsonEscape(r.verdict) + "\",\"output\":\"" +
              jsonEscape(r.output) + "\"}";
          respond(fd, 200, "OK", "application/json", jb);
        }
      }
    } else {
      respond(fd, 404, "Not Found", "text/plain", "not found\n");
    }
    close(fd);
    (void)reqNo;
  }
  close(srv);
  return 0;
}

} // namespace nusa
