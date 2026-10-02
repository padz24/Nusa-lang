// Sandbox eksekusi tak-terpercaya (Phase 42, docs/playground.md).
//
// Struktur proses (tanpa threads, tanpa exceptions):
//   supervisor (runSandboxed)
//     └─ worker [pgid sendiri]: build via Driver::runBuild, lalu
//          └─ runner: rlimits + chdir(tmp) + clearenv + /dev/null stdin,
//             stdout+stderr → pipe → worker (cap) → file hasil.
//   supervisor menunggu worker dengan timeout total; kadaluarsa →
//   killpg + verdict "timeout".
//
// Catatan keamanan jujur: TANPA namespace/user-isolation (tak tersedia
// di Termux) — lihat docs/playground.md untuk batasnya. RLIMIT_NPROC
// SENGAJA tak dipakai: di Android ia menghitung SEMUA thread UID
// (satu sesi Termux) sehingga bisa membunuh runner yang sah.
#include "compiler/driver/Sandbox.h"
#include "compiler/driver/Driver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace nusa {
namespace sandbox {
namespace {

// Budget build di dalam timeout total supervisor (detik).
constexpr int kBuildBudgetSec = 120;
// Poll supervisor/worker (mikrodetik).
constexpr useconds_t kPollUsec = 50000;

void writeAll(int fd, const char *p, size_t n) {
  while (n > 0) {
    ssize_t w = write(fd, p, n);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    p += w;
    n -= (size_t)w;
  }
}

// VmRSS (KB) proses via /proc/<pid>/status. -1 bila tak terbaca
// (proses sudah mati / proc tak tersedia).
long readVmRssKb(pid_t pid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
  FILE *f = fopen(path, "r");
  if (!f)
    return -1;
  char line[256];
  long rss = -1;
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      rss = atol(line + 6); // angka dalam kB
      break;
    }
  }
  fclose(f);
  return rss;
}

// Anak runner: terapkan batas, isolasi, lalu exec. Tak pernah kembali.
// inFd >= 0 = stdin dari berkas input (plugin); < 0 = /dev/null (biasa).
void runnerChild(const std::string &exe, const std::string &workdir,
                 int outFd, int inFd, const SandboxLimits &lim) {
  // Grup sendiri agar supervisor bisa killpg bila worker ikut mati.
  setpgid(0, 0);
  // TANPA RLIMIT_AS (lihat Sandbox.h): Bionic butuh reservasi VA
  // raksasa untuk CFI shadow. Memori dibatasi via polling VmRSS
  // di worker (verdict "mem-limit").
  // Batas CPU sedikit di atas wall-timeout (cadangan bila wall gagal).
  if (lim.timeoutSec > 0) {
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = (rlim_t)(lim.timeoutSec + 5);
    setrlimit(RLIMIT_CPU, &rl);
  }
  // Tanpa core dump.
  {
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rl);
  }
  if (!workdir.empty())
    (void)chdir(workdir.c_str());
  // Lingkungan dikosongkan total (PATH pun tidak diwariskan).
  clearenv();
  // stdin = berkas input (plugin) atau /dev/null (biasa — tak pernah
  // gantung menunggu ketikan).
  if (inFd >= 0) {
    dup2(inFd, STDIN_FILENO);
    if (inFd != STDIN_FILENO)
      close(inFd);
  } else {
    int nullIn = open("/dev/null", O_RDONLY);
    if (nullIn >= 0) {
      dup2(nullIn, STDIN_FILENO);
      if (nullIn != STDIN_FILENO)
        close(nullIn);
    }
  }
  dup2(outFd, STDOUT_FILENO);
  dup2(outFd, STDERR_FILENO);
  if (outFd != STDOUT_FILENO && outFd != STDERR_FILENO)
    close(outFd);
  execl(exe.c_str(), exe.c_str(), nullptr);
  _exit(127); // exec gagal
}

// Worker: build + jalankan runner + tulis file hasil. Return via _exit.
// stdinData kosong = stdin /dev/null (perilaku lama persis).
void workerMain(const std::string &nsPath, const std::string &workdir,
                const std::string &resPath, const SandboxLimits &lim,
                const std::string &stdinData) {
  setpgid(0, 0);
  std::string verdict = "build-fail";
  std::string output;
  int code = 1;
  std::string exe = workdir + "/prog";

  // 1. Build (compiler tepercaya atas sumber tak-terpercaya; pipeline
  // Nusa menjamin tanpa-hang: guard rekursi + error cap — spec §14).
  // Stderr build perlu ditangkap: alihkan via file (popen di runBuild
  // menulis ke stderr proses ini).
  std::string buildLog = workdir + "/build.log";
  int savedErr = dup(STDERR_FILENO);
  int logFd = open(buildLog.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  Driver driver;
  int buildRc = 1;
  if (logFd >= 0) {
    std::fflush(nullptr);
    dup2(logFd, STDERR_FILENO);
    // Stdout build ("Built ...") ikut ke log agar hasil bersih.
    int savedOut = dup(STDOUT_FILENO);
    dup2(logFd, STDOUT_FILENO);
    buildRc = driver.runBuild(nsPath, exe);
    std::fflush(nullptr);
    dup2(savedOut, STDOUT_FILENO);
    close(savedOut);
    dup2(savedErr, STDERR_FILENO);
    close(logFd);
  } else {
    buildRc = driver.runBuild(nsPath, exe);
  }
  close(savedErr);
  if (buildRc != 0) {
    verdict = "build-fail";
    code = buildRc;
    FILE *lf = fopen(buildLog.c_str(), "rb");
    if (lf) {
      char buf[4096];
      size_t n;
      while ((n = fread(buf, 1, sizeof(buf), lf)) > 0)
        output.append(buf, n);
      fclose(lf);
    }
  } else {
    // 2. Runner di bawah pipe + timeout wall worker.
    int pipefd[2];
    if (pipe(pipefd) != 0) {
      verdict = "signal";
      output = "sandbox: pipe failed";
    } else {
      // Berkas stdin untuk runner (plugin); -1 = /dev/null.
      int inFd = -1;
      std::string inPath = workdir + "/stdin.bin";
      if (!stdinData.empty()) {
        FILE *wf = fopen(inPath.c_str(), "wb");
        if (wf) {
          size_t off = 0;
          while (off < stdinData.size()) {
            size_t w =
                fwrite(stdinData.data() + off, 1, stdinData.size() - off, wf);
            if (w == 0)
              break;
            off += w;
          }
          fclose(wf);
          inFd = open(inPath.c_str(), O_RDONLY);
        }
      }
      std::fflush(nullptr);
      pid_t rp = fork();
      if (rp < 0) {
        verdict = "signal";
        output = "sandbox: fork failed";
        if (inFd >= 0)
          close(inFd);
        close(pipefd[0]);
        close(pipefd[1]);
      } else if (rp == 0) {
        close(pipefd[0]);
        runnerChild(exe, workdir, pipefd[1], inFd, lim);
        _exit(127);
      } else {
        if (inFd >= 0)
          close(inFd); // milik runner; EOF dari berkas (reguler)
        close(pipefd[1]);
        size_t cap =
            lim.outputKb > 0 ? (size_t)lim.outputKb * 1024UL : (1UL << 30);
        bool over = false;
        char buf[4096];
        // Baca sampai EOF / cap; timeout dipoll bersamaan via
        // non-blocking + waitpid WNOHANG.
        int fl = fcntl(pipefd[0], F_GETFL, 0);
        fcntl(pipefd[0], F_SETFL, fl | O_NONBLOCK);
        int elapsed = 0; // dalam satuan poll
        int maxPoll =
            lim.timeoutSec > 0 ? (lim.timeoutSec * 1000000) / kPollUsec : -1;
        int status = 0;
        pid_t done = 0;
        std::string acc;
        long memKb = lim.memoryMb > 0 ? lim.memoryMb * 1024L : -1;
        bool memOver = false;
        while (true) {
          ssize_t r = read(pipefd[0], buf, sizeof(buf));
          if (r > 0) {
            if (acc.size() + (size_t)r > cap) {
              acc.append(buf, cap - acc.size());
              over = true;
              break;
            }
            acc.append(buf, r);
          } else if (r == 0) {
            done = waitpid(rp, &status, 0);
            break;
          } else if (errno != EAGAIN && errno != EINTR) {
            break;
          }
          done = waitpid(rp, &status, WNOHANG);
          if (done == rp)
            break;
          if (done < 0 && errno != EINTR)
            break;
          // Penegakan memori via polling (pengganti RLIMIT_AS —
          // lihat Sandbox.h). Granularitas 50ms; OOM-killer kernel
          // backstop untuk spike antar-poll.
          if (memKb > 0 && readVmRssKb(rp) > memKb) {
            memOver = true;
            break;
          }
          usleep(kPollUsec);
          if (maxPoll >= 0 && ++elapsed >= maxPoll)
            break; // timeout wall
          if (r < 0)
            continue;
        }
        if (memOver) {
          verdict = "mem-limit";
          code = 137;
          kill(rp, SIGKILL);
          waitpid(rp, &status, 0);
        } else if (over ||
                   (done != rp && maxPoll >= 0 && elapsed >= maxPoll &&
                    waitpid(rp, &status, WNOHANG) != rp)) {
          if (over) {
            verdict = "output-limit";
            code = 1;
          } else {
            verdict = "timeout";
            code = 124;
          }
          kill(rp, SIGKILL);
          waitpid(rp, &status, 0);
        } else {
          if (done != rp)
            waitpid(rp, &status, 0);
          if (WIFEXITED(status)) {
            verdict = "ok";
            code = WEXITSTATUS(status);
          } else if (WIFSIGNALED(status)) {
            verdict = "signal";
            code = WTERMSIG(status);
          }
        }
        // Kuras sisa pipe pasca-kill agar tak ada zombie-output.
        if (verdict == "ok" || verdict == "signal") {
          // EOF sudah tercapai via read==0 di atas untuk kasus normal.
        }
        output = acc;
        close(pipefd[0]);
      }
    }
  }

  FILE *rf = fopen(resPath.c_str(), "wb");
  if (rf) {
    fprintf(rf, "%s\n%d\n", verdict.c_str(), code);
    if (!output.empty())
      fwrite(output.data(), 1, output.size(), rf);
    fclose(rf);
  }
  _exit(0);
}

} // namespace

static SandboxResult runSandboxedImpl(const std::string &nsPath,
                                        const SandboxLimits &lim,
                                        const std::string &stdinData) {
  SandboxResult res;
  // Workdir terisolasi untuk exe + build.log + hasil.
  char tmpl[] = "/tmp/nusa-sandbox-XXXXXX";
  char *wd = mkdtemp(tmpl);
  if (!wd) {
    res.verdict = "signal";
    res.output = "sandbox: mkdtemp failed";
    return res;
  }
  std::string workdir(wd);
  std::string resPath = workdir + "/result.bin";

  std::fflush(nullptr);
  pid_t wp = fork();
  if (wp < 0) {
    res.verdict = "signal";
    res.output = "sandbox: fork failed";
    return res;
  }
  if (wp == 0) {
    workerMain(nsPath, workdir, resPath, lim, stdinData);
    _exit(1);
  }
  // Supervisor: timeout total = run + budget build.
  int totalSec = (lim.timeoutSec > 0 ? lim.timeoutSec : 10) + kBuildBudgetSec;
  int elapsed = 0;
  int maxPoll = (totalSec * 1000000) / kPollUsec;
  int status = 0;
  pid_t done = 0;
  while (elapsed++ < maxPoll) {
    done = waitpid(wp, &status, WNOHANG);
    if (done == wp || (done < 0 && errno != EINTR))
      break;
    usleep(kPollUsec);
  }
  if (done != wp) {
    killpg(wp, SIGKILL); // worker + runner (satu grup)
    waitpid(wp, &status, 0);
    res.verdict = "timeout";
    res.code = 124;
    res.output = "sandbox: total time exceeded (build or run hung)";
    return res;
  }
  if (WIFSIGNALED(status)) {
    res.verdict = "signal";
    res.code = WTERMSIG(status);
    res.output = "sandbox: worker killed by signal";
    return res;
  }
  // Baca file hasil.
  FILE *rf = fopen(resPath.c_str(), "rb");
  if (!rf) {
    res.verdict = "signal";
    res.output = "sandbox: missing result (worker failed)";
    return res;
  }
  char line[256];
  if (!fgets(line, sizeof(line), rf)) {
    fclose(rf);
    res.verdict = "signal";
    res.output = "sandbox: corrupt result";
    return res;
  }
  line[strcspn(line, "\r\n")] = '\0';
  res.verdict = line;
  if (fgets(line, sizeof(line), rf))
    res.code = atoi(line);
  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), rf)) > 0)
    res.output.append(buf, n);
  fclose(rf);
  // Bersihkan workdir (best-effort; isi hanya artefak sandbox).
  {
    std::string cmd = std::string("rm -rf '") + workdir + "'";
    // workdir dari mkdtemp: tanpa kutip tunggal — aman dikutip.
    (void)system(cmd.c_str());
  }
  return res;
}

SandboxResult runSandboxed(const std::string &nsPath,
                           const SandboxLimits &lim) {
  return runSandboxedImpl(nsPath, lim, "");
}

SandboxResult runSandboxedInput(const std::string &nsPath,
                                const SandboxLimits &lim,
                                const std::string &stdinData) {
  return runSandboxedImpl(nsPath, lim, stdinData);
}

} // namespace sandbox
} // namespace nusa
