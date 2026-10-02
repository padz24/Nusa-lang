// Test runner v0.9.0 (`nusa test`): temukan `*_test.ns`, build + jalankan
// tiap file dengan batas waktu, laporkan PASS/FAIL + ringkasan.
//
// Konvensi: file test berakhiran `_test.ns` dan mendefinisikan `fn main()`.
// PASS = executable exit 0 (gagal build, assert, OOB, timeout = FAIL).
// Batas 10 detik per test (fork + alarm) agar loop nyasar tak menggantung.
#include "compiler/driver/Driver.h"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace nusa {
namespace {

constexpr int kTestTimeoutSec = 10;

bool endsWith(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isDir(const std::string &path) {
  DIR *d = opendir(path.c_str());
  if (!d)
    return false;
  closedir(d);
  return true;
}

// Kumpulkan *_test.ns dari path (file langsung atau direktori, flat).
void collect(const std::string &path, std::vector<std::string> &out) {
  if (isDir(path)) {
    DIR *d = opendir(path.c_str());
    if (!d)
      return;
    std::vector<std::string> names;
    while (dirent *e = readdir(d)) {
      std::string n(e->d_name);
      if (endsWith(n, "_test.ns"))
        names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const auto &n : names)
      out.push_back(path + "/" + n);
    return;
  }
  out.push_back(path);
}

// Jalankan executable dengan timeout. Return: 0 PASS, -2 TIMEOUT,
// selain itu exit code anak (127 = gagal exec).
constexpr int kTimeoutVerdict = -2;

int runWithTimeout(const std::string &exe) {
  // WAJIB flush sebelum fork: buffer stdout/stderr milik orang tua ikut
  // tersalin ke anak dan akan ter-flush dua kali (duplikat output).
  std::fflush(nullptr);
  pid_t pid = fork();
  if (pid < 0)
    return 1;
  if (pid == 0) {
    alarm(kTestTimeoutSec);
    // Bungkam output program uji; stdin = /dev/null agar `input()`
    // deterministik (EOF) dan test tak pernah gantung menunggu ketikan.
    FILE *nullOut = freopen("/dev/null", "w", stdout);
    FILE *nullErr = freopen("/dev/null", "w", stderr);
    (void)nullOut;
    (void)nullErr;
    int nullIn = open("/dev/null", O_RDONLY);
    if (nullIn >= 0) {
      dup2(nullIn, STDIN_FILENO);
      if (nullIn != STDIN_FILENO)
        close(nullIn);
    }
    execl(exe.c_str(), exe.c_str(), nullptr);
    _exit(127); // execl gagal
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
  }
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM)
    return kTimeoutVerdict;
  if (WIFSIGNALED(status))
    return 128 + WTERMSIG(status);
  return 1;
}

} // namespace

int Driver::runTest(const std::vector<std::string> &paths) {
  std::vector<std::string> seeds = paths;
  if (seeds.empty())
    seeds.push_back(".");
  std::vector<std::string> files;
  for (const auto &p : seeds)
    collect(p, files);
  if (files.empty()) {
    std::fprintf(stderr, "error: no test files (`*_test.ns`) found\n");
    return 1;
  }
  int passed = 0, failed = 0;
  Driver driver;
  for (const auto &f : files) {
    if (!endsWith(f, ".ns")) {
      std::printf("FAIL %s (not a .ns file)\n", f.c_str());
      failed++;
      continue;
    }
    std::string exe = f + ".test.out";
    // Tutup stdout/stderr test agar output runner bersih; kegagalan
    // tetap terlihat via status. (Output test tampil bila FAIL? — tidak;
    // PASS/FAIL + ringkasan cukup untuk v0.9.0.)
    if (driver.runBuild(f, exe) != 0) {
      std::printf("FAIL %s (build failed)\n", f.c_str());
      failed++;
      std::remove(exe.c_str());
      continue;
    }
    // Bungkam output program saat uji; eksekusi selalu via fork+alarm
    // agar mandiri (tanpa dependensi `timeout(1)`) dan anti-gantung.
    // Alihkan stdout/stderr anak ke /dev/null di dalam runWithTimeout.
    int verdict = runWithTimeout(exe[0] == '/' || exe.find('/') !=
                                                      std::string::npos
                                     ? exe
                                     : "./" + exe);
    std::remove(exe.c_str());
    if (verdict == 0) {
      std::printf("PASS %s\n", f.c_str());
      passed++;
    } else if (verdict == kTimeoutVerdict) {
      std::printf("FAIL %s (timeout after %ds)\n", f.c_str(),
                  kTestTimeoutSec);
      failed++;
    } else {
      std::printf("FAIL %s (exit %d)\n", f.c_str(), verdict);
      failed++;
    }
  }
  std::printf("%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}

} // namespace nusa
