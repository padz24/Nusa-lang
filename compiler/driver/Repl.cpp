// REPL Nusa — implementasi milestone 1 (Phase 106, AOT-loop).
// Tiap baris: transkrip sesi -> session.ns (fn main) -> runBuild
// (cache inkremental) -> eksekusi -> teruskan stdout. Baris gagal
// TIDAK masuk sesi (diagnostik compiler tampil apa adanya).
#include "compiler/driver/Repl.h"

#include "compiler/driver/Driver.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace nusa {
namespace repl {

std::string renderSession(const std::vector<std::string> &lines) {
  std::string src = "fn main() {\n";
  for (const auto &l : lines) {
    src += l;
    src += "\n";
  }
  src += "}\n";
  return src;
}

int scanDepth(const std::string &text) {
  int depth = 0;
  bool inStr = false;
  for (size_t i = 0; i < text.size(); i++) {
    char c = text[i];
    if (inStr) {
      if (c == '\\' && i + 1 < text.size()) {
        i++; // escape: lewati berikut
      } else if (c == '"') {
        inStr = false;
      }
      continue;
    }
    if (c == '"') {
      inStr = true;
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      break; // sisa baris = komentar
    } else if (c == '{' || c == '(' || c == '[') {
      depth++;
    } else if (c == '}' || c == ')' || c == ']') {
      depth--;
    }
  }
  return depth;
}

static std::string trimRight(const std::string &s) {
  size_t n = s.find_last_not_of(" \t\r\n");
  return (n == std::string::npos) ? "" : s.substr(0, n + 1);
}

std::string wrapLine(const std::string &line) {
  std::string t = trimRight(line);
  // Statement .ns apa adanya: akhir ';' (let/print/assign/call) atau
  // '{'/'}' (blok if/while/match dibuka/ditutup tanpa ';').
  if (!t.empty() && (t.back() == ';' || t.back() == '{' || t.back() == '}'))
    return line;
  return "print(" + line + ");";
}

} // namespace repl

int Driver::runRepl() {
  // Direktori sesi privat (mkdtemp; dibersihkan saat keluar).
  char dirTpl[] = "/tmp/nusa-repl-XXXXXX";
  char *dir = mkdtemp(dirTpl);
  if (!dir) {
    std::fprintf(stderr, "error: cannot create REPL session dir\n");
    return 1;
  }
  std::string sessionNs = std::string(dir) + "/session.ns";
  std::string sessionExe = std::string(dir) + "/session";
  std::vector<std::string> lines;
  std::string pending; // akumulator prompt lanjutan
  bool tty = isatty(STDIN_FILENO);

  Driver driver;
  for (;;) {
    if (tty)
      std::printf(pending.empty() ? "nusa> " : "... ");
    std::fflush(stdout);
    std::string input;
    if (!std::getline(std::cin, input)) {
      std::printf("\n");
      break; // EOF (Ctrl-D / stdin habis) = keluar bersih
    }
    if (!pending.empty())
      pending += "\n";
    pending += input;
    if (repl::scanDepth(pending) > 0)
      continue; // blok belum seimbang: minta lanjutan
    std::string line = pending;
    pending.clear();

    std::string t = line;
    size_t s = t.find_first_not_of(" \t\r\n");
    std::string head = (s == std::string::npos) ? "" : t.substr(s);
    if (head.empty())
      continue;
    if (head[0] == ':') {
      if (head == ":quit" || head == ":exit")
        break;
      if (head == ":reset") {
        lines.clear();
        std::printf("session reset\n");
        continue;
      }
      if (head == ":help") {
        std::printf("commands: :help :reset :quit\n"
                    "statements end with ';', '{' or '}' (e.g. let x = 10;)\n"
                    "bare expressions are printed (e.g. x + 20)\n");
        continue;
      }
      std::fprintf(stderr, "error: unknown repl command '%s'\n",
                   head.c_str());
      continue;
    }

    std::vector<std::string> trial = lines;
    trial.push_back(repl::wrapLine(line));
    {
      FILE *f = std::fopen(sessionNs.c_str(), "w");
      if (!f) {
        std::fprintf(stderr, "error: cannot write REPL session\n");
        return 1;
      }
      std::string src = repl::renderSession(trial);
      std::fwrite(src.data(), 1, src.size(), f);
      std::fclose(f);
    }
    int rc = driver.runBuild(sessionNs, sessionExe);
    if (rc != 0) {
      // Diagnostik sudah di stderr; sesi tak teracuni.
      continue;
    }
    std::string cmd = sessionExe; // tanpa spasi (mkdtemp); tetap aman
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
      std::fprintf(stderr, "error: cannot run REPL program\n");
      continue;
    }
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), pipe)) > 0)
      std::fwrite(buf, 1, n, stdout);
    std::fflush(stdout);
    int prc = pclose(pipe);
    if (prc != 0)
      std::fprintf(stderr, "warning: program exited %d\n", prc);
    lines.swap(trial); // sukses -> jadikan permanen
  }
  std::remove(sessionExe.c_str());
  std::string ll = sessionNs + ".ll";
  std::remove(ll.c_str());
  std::remove(sessionNs.c_str());
  {
    // Cache inkremental sesi (.nusa-cache) + direktori: buang total.
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
  return 0;
}

} // namespace nusa
