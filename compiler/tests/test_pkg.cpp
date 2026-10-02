// Unit test package manager v1.11.0: manifest, semver, lock, sha.
#include "compiler/pkg/Pkg.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace nusa::pkg;
namespace fs = std::filesystem;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static std::string tmpdir() {
  char tmpl[] = "/tmp/nusapkXXXXXX";
  if (!mkdtemp(tmpl))
    return "";
  return tmpl;
}

static void write(const std::string &path, const std::string &data) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << data;
}

// Fwd (definisi di bawah main agar diff kecil).
static int testSatisfiesReq();
static int testCapabilities();
static int testPackageDirSafe();
static int testLockCompilerPin();

static int testManifestOk() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  write(d + "/nusa.toml",
        "[package]\nname = \"myapp\"\nversion = \"0.1.0\"\n"
        "compiler = \"1.11.0\"\nfeatures = [\"net\"]\n"
        "\n[dependencies]\nfoo = { path = \"../foo\" }\n"
        "bar = \"1.2.3\"\n");
  Manifest m;
  std::string err;
  CHECK(parseManifest(d + "/nusa.toml", m, err));
  CHECK(m.name == "myapp");
  CHECK(m.version == "0.1.0");
  CHECK(m.compiler == "1.11.0");
  CHECK(m.features.size() == 1 && m.features[0] == "net");
  CHECK(m.deps.size() == 2);
  CHECK(m.deps[0].name == "foo" && m.deps[0].isPath());
  CHECK(m.deps[1].name == "bar" && !m.deps[1].isPath() &&
        m.deps[1].version == "1.2.3");
  std::cout << "testManifestOk OK\n";
  return 0;
}

static int testManifestBad() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  Manifest m;
  std::string err;
  write(d + "/nusa.toml", "[package]\nversion = \"0.1.0\"\n");
  CHECK(!parseManifest(d + "/nusa.toml", m, err));
  write(d + "/nusa.toml", "[package]\nname = \"x\"\nversion = \"1.2\"\n");
  CHECK(!parseManifest(d + "/nusa.toml", m, err));
  write(d + "/nusa.toml",
        "[package]\nname = \"x\"\nversion = \"0.1.0\"\n"
        "[dependencies]\nfoo = { path = \"a\", version = \"1\" }\n");
  CHECK(!parseManifest(d + "/nusa.toml", m, err));
  std::cout << "testManifestBad OK\n";
  return 0;
}

static int testSemver() {
  int a, b, c;
  std::string pre;
  CHECK(parseSemver("1.11.0", a, b, c, pre) && a == 1 && b == 11 && c == 0 &&
        pre.empty());
  CHECK(parseSemver("0.1.0-alpha.1", a, b, c, pre) && pre == "alpha.1");
  CHECK(!parseSemver("1.2", a, b, c, pre));
  CHECK(!parseSemver("x.y.z", a, b, c, pre));
  CHECK(cmpSemver("1.10.0", "1.9.0") > 0);
  CHECK(cmpSemver("1.9.0", "1.10.0") < 0);
  CHECK(cmpSemver("1.10.0", "1.10.0") == 0);
  CHECK(cmpSemver("2.0.0-alpha", "2.0.0") < 0);
  CHECK(satisfiesMin("1.11.0", "1.10.0"));
  CHECK(!satisfiesMin("1.9.0", "1.10.0"));
  std::cout << "testSemver OK\n";
  return 0;
}

static int testSha() {
  // Vektor uji SHA-256 ("abc").
  CHECK(sha256Hex("abc") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  std::cout << "testSha OK\n";
  return 0;
}

static int testLockRoundtrip() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  std::vector<LockPkg> pkgs = {{"b", "0.2.0", "../b", "", "sha256:00"},
                               {"a", "0.1.0", "../a", "", "sha256:ff"}};
  std::string err;
  CHECK(writeLock(d + "/nusa.lock", pkgs, err));
  std::vector<LockPkg> back;
  CHECK(parseLock(d + "/nusa.lock", back, err));
  CHECK(back.size() == 2);
  CHECK(back[0].name == "a"); // terurut
  CHECK(back[1].name == "b");
  CHECK(back[0].checksum == "sha256:ff");
  std::cout << "testLockRoundtrip OK\n";
  return 0;
}

static int testDirChecksumStable() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  write(d + "/nusa.toml", "[package]\nname = \"x\"\nversion = \"0.1.0\"\n");
  write(d + "/a.ns", "fn main() { }\n");
  std::string c1 = dirChecksum(d);
  std::string c2 = dirChecksum(d);
  CHECK(c1 == c2);
  CHECK(c1.rfind("sha256:", 0) == 0 && c1.size() == 7 + 64);
  write(d + "/a.ns", "fn main() { print(1); }\n");
  CHECK(dirChecksum(d) != c1);
  std::cout << "testDirChecksumStable OK\n";
  return 0;
}

int main() {
  if (testManifestOk()) return 1;
  if (testManifestBad()) return 1;
  if (testSemver()) return 1;
  if (testSha()) return 1;
  if (testLockRoundtrip()) return 1;
  if (testDirChecksumStable()) return 1;
  if (testSatisfiesReq()) return 1;
  if (testCapabilities()) return 1;
  if (testPackageDirSafe()) return 1;
  if (testLockCompilerPin()) return 1;
  std::cout << "ALL PKG TESTS PASSED\n";
  return 0;
}

static int testSatisfiesReq() {
  CHECK(satisfiesReq("1.2.3", "1.2.3"));
  CHECK(!satisfiesReq("1.2.4", "1.2.3"));
  CHECK(satisfiesReq("1.2.3", "^1.2.0"));
  CHECK(satisfiesReq("1.9.9", "^1.2.0"));
  CHECK(!satisfiesReq("2.0.0", "^1.2.0"));
  CHECK(!satisfiesReq("1.1.9", "^1.2.0"));
  CHECK(satisfiesReq("0.1.5", "^0.1.0"));
  CHECK(!satisfiesReq("0.2.0", "^0.1.0"));
  CHECK(satisfiesReq("0.0.3", "^0.0.3"));
  CHECK(!satisfiesReq("0.0.4", "^0.0.3"));
  CHECK(satisfiesReq("9.9.9", ""));
  CHECK(!satisfiesReq("bogus", "^1.0.0"));
  std::cout << "testSatisfiesReq OK\n";
  return 0;
}

// Phase 59 (v2.10.0): [capabilities] parse + round-trip + summary.
static int testCapabilities() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  Manifest m;
  std::string err;
  // Default: semua false bila bagian absen.
  write(d + "/nusa.toml",
        "[package]\nname = \"a\"\nversion = \"0.1.0\"\n");
  CHECK(parseManifest(d + "/nusa.toml", m, err));
  CHECK(!m.caps.filesystem && !m.caps.network && !m.caps.process &&
        !m.caps.environment && !m.caps.native);
  CHECK(capabilitiesSummary(m) == "(none)");
  // Semua true + round-trip tulis-baca.
  write(d + "/nusa.toml",
        "[package]\nname = \"a\"\nversion = \"0.1.0\"\n"
        "\n[capabilities]\nfilesystem = true\nnetwork = true\n"
        "process = false\nenvironment = false\nnative = true\n");
  CHECK(parseManifest(d + "/nusa.toml", m, err));
  CHECK(m.caps.filesystem && m.caps.network && !m.caps.process &&
        !m.caps.environment && m.caps.native);
  CHECK(capabilitiesSummary(m) == "filesystem network native");
  CHECK(writeManifest(d + "/out.toml", m, err));
  Manifest back;
  CHECK(parseManifest(d + "/out.toml", back, err));
  CHECK(back.caps.filesystem && back.caps.network && back.caps.native);
  CHECK(!back.caps.process && !back.caps.environment);
  // Tanpa cap true: bagian tak ditulis (tanpa churn).
  write(d + "/nusa.toml",
        "[package]\nname = \"a\"\nversion = \"0.1.0\"\n");
  Manifest fresh;
  CHECK(parseManifest(d + "/nusa.toml", fresh, err));
  CHECK(writeManifest(d + "/out2.toml", fresh, err));
  {
    std::ifstream f(d + "/out2.toml");
    std::string all((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    CHECK(all.find("[capabilities]") == std::string::npos);
  }
  // Tipe salah + key asing ditolak.
  write(d + "/nusa.toml",
        "[package]\nname = \"a\"\nversion = \"0.1.0\"\n"
        "\n[capabilities]\nnetwork = \"yes\"\n");
  CHECK(!parseManifest(d + "/nusa.toml", m, err));
  write(d + "/nusa.toml",
        "[package]\nname = \"a\"\nversion = \"0.1.0\"\n"
        "\n[capabilities]\nnetwrok = true\n");
  CHECK(!parseManifest(d + "/nusa.toml", m, err));
  std::cout << "testCapabilities OK\n";
  return 0;
}

// Phase 59 (v2.10.0): validator isi paket pasca-unpack.
static int testPackageDirSafe() {
  std::string bad;
  std::string d = tmpdir();
  CHECK(!d.empty());
  write(d + "/nusa.toml", "[package]\n");
  write(d + "/a.ns", "fn main() {}\n");
  CHECK(packageDirSafe(d, bad));
  // Subdirektori ditolak.
  fs::create_directories(d + "/sub");
  CHECK(!packageDirSafe(d, bad));
  fs::remove_all(d + "/sub");
  CHECK(packageDirSafe(d, bad));
  // Symlink ditolak.
  fs::create_symlink(d + "/a.ns", d + "/link.ns");
  CHECK(!packageDirSafe(d, bad));
  fs::remove(d + "/link.ns");
  CHECK(packageDirSafe(d, bad));
  // Dotfile ditolak.
  write(d + "/.hidden", "x");
  CHECK(!packageDirSafe(d, bad));
  fs::remove(d + "/.hidden");
  CHECK(packageDirSafe(d, bad));
  std::cout << "testPackageDirSafe OK\n";
  return 0;
}

// Phase 61 (v2.12.0): pin compiler [meta] di lock.
static int testLockCompilerPin() {
  std::string d = tmpdir();
  CHECK(!d.empty());
  std::vector<LockPkg> pkgs = {{"a", "0.1.0", "../a", "", "sha256:ff"}};
  std::string err;
  CHECK(writeLock(d + "/nusa.lock", pkgs, err));
  std::vector<LockPkg> back;
  std::string pin;
  CHECK(parseLockFull(d + "/nusa.lock", back, &pin, err));
  CHECK(pin == compilerVersion());
  CHECK(back.size() == 1 && back[0].name == "a");
  // Lock lama tanpa [meta] → pin kosong (basi).
  write(d + "/old.lock",
        "# GENERATED\n\n[[package]]\nname = \"a\"\nversion = \"0.1.0\"\n"
        "path = \"../a\"\nchecksum = \"sha256:ff\"\n");
  std::vector<LockPkg> back2;
  std::string pin2 = "sentinel";
  CHECK(parseLock(d + "/old.lock", back2, err));
  CHECK(parseLockFull(d + "/old.lock", back2, &pin2, err));
  CHECK(pin2.empty());
  std::cout << "testLockCompilerPin OK\n";
  return 0;
}
