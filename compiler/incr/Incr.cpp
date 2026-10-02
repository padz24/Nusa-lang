// Incremental compilation (Phase 35). Lihat Incr.h.
#include "compiler/incr/Incr.h"
#include "compiler/driver/Modules.h"
#include "compiler/pkg/Pkg.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace nusa {
namespace incr {

std::string shaFile(const std::string &path) {
  std::string data;
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  data = ss.str();
  if (f.bad())
    return "";
  return "sha256:" + pkg::sha256Hex(data);
}

std::string shaCompiler() {
  char buf[4096];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0)
    return "unknown:no-proc-exe";
  buf[n] = '\0';
  std::string h = shaFile(buf);
  if (h.empty())
    return "unknown:unreadable-exe";
  return h;
}

std::string cacheDirFor(const std::string &outputPath) {
  size_t slash = outputPath.find_last_of("/\\");
  std::string dir =
      (slash == std::string::npos) ? "." : outputPath.substr(0, slash);
  if (dir.empty())
    dir = ".";
  return dir + "/.nusa-cache";
}

std::string fingerprintPath(const std::string &cacheDir) {
  return cacheDir + "/fingerprint";
}

std::vector<ModuleNode> buildGraph(const std::vector<LoadedModule> &modules) {
  std::vector<ModuleNode> out;
  for (const auto &m : modules) {
    ModuleNode node;
    node.path = m.path;
    node.display = m.display;
    node.sha = shaFile(m.path);
    if (node.sha.empty())
      node.sha = "missing:" + m.path;
    for (const auto &im : m.program.imports) {
      std::string name = im.module;
      const std::string prefix = "std.";
      if (name.size() > prefix.size() &&
          name.compare(0, prefix.size(), prefix) == 0)
        name = name.substr(prefix.size());
      node.imports.push_back(name);
    }
    out.push_back(node);
  }
  // Edge tak disimpan eksplisit (best-effort via basename saat lapor);
  // fingerprint memakai set terurut di bawah (sameInputs).
  return out;
}

bool sameInputs(const Fingerprint &a, const Fingerprint &b) {
  if (a.compiler != b.compiler || a.target != b.target ||
      a.opt != b.opt || a.emit != b.emit || a.sysroot != b.sysroot ||
      a.links != b.links || a.modules.size() != b.modules.size())
    return false;
  std::vector<std::pair<std::string, std::string>> x, y;
  for (const auto &m : a.modules)
    x.emplace_back(m.path, m.sha);
  for (const auto &m : b.modules)
    y.emplace_back(m.path, m.sha);
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());
  return x == y;
}

bool sameFlags(const Fingerprint &a, const Fingerprint &b) {
  return a.compiler == b.compiler && a.target == b.target &&
         a.opt == b.opt && a.emit == b.emit && a.sysroot == b.sysroot &&
         a.debug == b.debug && a.links == b.links && a.pgo == b.pgo &&
         a.out == b.out;
}

std::vector<std::string> changedModules(const Fingerprint &oldFp,
                                        const Fingerprint &cur) {
  std::vector<std::pair<std::string, std::string>> x, y;
  for (const auto &m : oldFp.modules)
    x.emplace_back(m.path, m.sha);
  for (const auto &m : cur.modules)
    y.emplace_back(m.path, m.sha);
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());
  std::vector<std::string> changed;
  size_t i = 0, j = 0;
  auto disp = [&](const std::string &path) {
    for (const auto &m : cur.modules) {
      if (m.path == path)
        return m.display;
    }
    for (const auto &m : oldFp.modules) {
      if (m.path == path)
        return m.display;
    }
    return path;
  };
  while (i < x.size() || j < y.size()) {
    if (j == y.size() || (i < x.size() && x[i].first < y[j].first)) {
      changed.push_back("-" + disp(x[i].first)); // hilang
      i++;
    } else if (i == x.size() || y[j].first < x[i].first) {
      changed.push_back("+" + disp(y[j].first)); // baru
      j++;
    } else {
      if (x[i].second != y[j].second)
        changed.push_back("~" + disp(y[j].first)); // berubah isi
      i++;
      j++;
    }
  }
  return changed;
}

bool readFingerprint(const std::string &path, Fingerprint &out,
                     std::string &err) {
  std::ifstream f(path);
  if (!f) {
    err = "no fingerprint";
    return false;
  }
  Fingerprint fp;
  std::string line;
  std::string section;
  auto need = [&](bool cond, const std::string &what) {
    if (!cond)
      err = "bad fingerprint (" + what + ")";
    return cond;
  };
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
      section = line.substr(1, line.size() - 2);
      continue;
    }
    size_t eq = line.find('=');
    if (eq == std::string::npos) {
      err = "bad fingerprint (line)";
      return false;
    }
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    // Unescape sederhana: \\ \n \t \" (cermin writer di bawah).
    std::string u;
    for (size_t i = 0; i < val.size(); i++) {
      if (val[i] == '\\' && i + 1 < val.size()) {
        char e = val[i + 1];
        if (e == 'n')
          u.push_back('\n');
        else if (e == 't')
          u.push_back('\t');
        else
          u.push_back(e);
        i++;
      } else {
        u.push_back(val[i]);
      }
    }
    if (section == "build") {
      if (key == "compiler")
        fp.compiler = u;
      else if (key == "target")
        fp.target = u;
      else if (key == "opt")
        fp.opt = u;
      else if (key == "emit")
        fp.emit = u;
      else if (key == "sysroot")
        fp.sysroot = u;
      else if (key == "debug")
        fp.debug = (u == "1");
      else if (key == "pgo")
        fp.pgo = u;
      else if (key == "out")
        fp.out = u;
      else if (key == "ir")
        fp.ir = u;
      else if (key == "link")
        fp.links.push_back(u);
      else
        return need(false, "key");
    } else if (section == "modules") {
      fp.modules.push_back(ModuleNode{key, key, u, {}});
    } else {
      return need(false, "section");
    }
  }
  if (!need(!fp.compiler.empty(), "compiler"))
    return false;
  out = fp;
  return true;
}

bool writeFingerprint(const std::string &path, const Fingerprint &fp,
                      std::string &err) {
  auto esc = [](const std::string &s) {
    std::string o;
    for (char c : s) {
      if (c == '\\')
        o += "\\\\";
      else if (c == '\n')
        o += "\\n";
      else if (c == '\t')
        o += "\\t";
      else
        o.push_back(c);
    }
    return o;
  };
  std::string o = "# Nusa incremental fingerprint (GENERATED, do not edit).\n";
  o += "[build]\n";
  o += "compiler=" + esc(fp.compiler) + "\n";
  o += "target=" + esc(fp.target) + "\n";
  o += "opt=" + esc(fp.opt) + "\n";
  o += "emit=" + esc(fp.emit) + "\n";
  o += "sysroot=" + esc(fp.sysroot) + "\n";
  o += "debug=";
  o += fp.debug ? "1\n" : "0\n";
  o += "pgo=" + esc(fp.pgo) + "\n";
  o += "out=" + esc(fp.out) + "\n";
  o += "ir=" + esc(fp.ir) + "\n";
  for (const auto &l : fp.links)
    o += "link=" + esc(l) + "\n";
  o += "[modules]\n";
  std::vector<ModuleNode> sorted = fp.modules;
  std::sort(sorted.begin(), sorted.end(),
            [](const ModuleNode &a, const ModuleNode &b) {
              return a.path < b.path;
            });
  for (const auto &m : sorted)
    o += esc(m.path) + "=" + esc(m.sha) + "\n";
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    err = "cannot write '" + path + "'";
    return false;
  }
  f << o;
  if (!f) {
    err = "cannot write '" + path + "'";
    return false;
  }
  return true;
}

} // namespace incr
} // namespace nusa
