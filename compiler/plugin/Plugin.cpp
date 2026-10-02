// Plugin Nusa v1 — implementasi milestone 1 (Phase 112).
// Hanya C++ standar + lapisan sandbox. Parser JSON subset mandiri
// (tanpa refactor LSP/pkg): objek datar + array-string + skipValue
// robust. Tanpa exceptions (gaya repo).
#include "compiler/plugin/Plugin.h"

#include "compiler/driver/Driver.h"
#include "compiler/driver/Sandbox.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace nusa {
namespace plugin {
namespace {

struct P {
  const std::string &s;
  size_t i = 0;
  std::string err;
  void skip() {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
      i++;
  }
  bool expect(char c, const char *what) {
    skip();
    if (i >= s.size() || s[i] != c) {
      err = std::string("expected ") + what;
      return false;
    }
    i++;
    return true;
  }
  bool parseString(std::string &out) {
    skip();
    if (i >= s.size() || s[i] != '"') {
      err = "expected string";
      return false;
    }
    i++;
    while (i < s.size() && s[i] != '"') {
      if (s[i] == '\\' && i + 1 < s.size()) {
        char e = s[i + 1];
        if (e == 'n')
          out.push_back('\n');
        else if (e == 't')
          out.push_back('\t');
        else if (e == 'r')
          out.push_back('\r');
        else
          out.push_back(e); // \" \\ \/ \u.. disederhanakan setia
        i += 2;
      } else {
        out.push_back(s[i++]);
      }
    }
    if (i >= s.size()) {
      err = "unterminated string";
      return false;
    }
    i++;
    return true;
  }
  // Lewati satu nilai JSON arbitrer (objek/array/string/angka/huruf).
  bool skipValue() {
    skip();
    if (i >= s.size()) {
      err = "unexpected end";
      return false;
    }
    char c = s[i];
    if (c == '"') {
      std::string tmp;
      return parseString(tmp);
    }
    if (c == '{') {
      i++;
      skip();
      if (i < s.size() && s[i] == '}') {
        i++;
        return true;
      }
      for (;;) {
        std::string k;
        if (!parseString(k))
          return false;
        if (!expect(':', "':'"))
          return false;
        if (!skipValue())
          return false;
        skip();
        if (i >= s.size()) {
          err = "unterminated object";
          return false;
        }
        if (s[i] == ',') {
          i++;
          continue;
        }
        if (s[i] == '}') {
          i++;
          return true;
        }
        err = "expected ',' or '}'";
        return false;
      }
    }
    if (c == '[') {
      i++;
      skip();
      if (i < s.size() && s[i] == ']') {
        i++;
        return true;
      }
      for (;;) {
        if (!skipValue())
          return false;
        skip();
        if (i >= s.size()) {
          err = "unterminated array";
          return false;
        }
        if (s[i] == ',') {
          i++;
          continue;
        }
        if (s[i] == ']') {
          i++;
          return true;
        }
        err = "expected ',' or ']'";
        return false;
      }
    }
    // Angka/literal: hingga pembatas struktural/whitespace.
    while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' &&
           s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r')
      i++;
    return true;
  }
};

} // namespace

bool parseManifest(const std::string &text, PluginManifest &out,
                   std::string &err) {
  P p{text};
  if (!p.expect('{', "'{'")) {
    err = "manifest: " + p.err;
    return false;
  }
  p.skip();
  out = PluginManifest();
  if (p.i < p.s.size() && p.s[p.i] == '}') {
    p.i++;
    out.present = true;
    return true;
  }
  for (;;) {
    std::string key;
    if (!p.parseString(key)) {
      err = "manifest: " + p.err;
      return false;
    }
    if (!p.expect(':', "':'")) {
      err = "manifest: " + p.err;
      return false;
    }
    if (key == "name" || key == "version") {
      std::string v;
      if (!p.parseString(v)) {
        err = "manifest: '" + key + "' must be a string";
        return false;
      }
      if (key == "name")
        out.name = v;
      else
        out.version = v;
    } else if (key == "ops") {
      p.skip();
      if (p.i >= p.s.size() || p.s[p.i] != '[') {
        err = "manifest: 'ops' must be an array of strings";
        return false;
      }
      p.i++;
      p.skip();
      if (p.i < p.s.size() && p.s[p.i] == ']') {
        p.i++;
      } else {
        for (;;) {
          std::string op;
          if (!p.parseString(op)) {
            err = "manifest: 'ops' must be an array of strings";
            return false;
          }
          out.ops.push_back(op);
          p.skip();
          if (p.i >= p.s.size()) {
            err = "manifest: unterminated 'ops'";
            return false;
          }
          if (p.s[p.i] == ',') {
            p.i++;
            continue;
          }
          if (p.s[p.i] == ']') {
            p.i++;
            break;
          }
          err = "manifest: expected ',' or ']'";
          return false;
        }
      }
    } else {
      if (!p.skipValue()) {
        err = "manifest: " + p.err;
        return false;
      }
    }
    p.skip();
    if (p.i >= p.s.size()) {
      err = "manifest: unterminated object";
      return false;
    }
    if (p.s[p.i] == ',') {
      p.i++;
      continue;
    }
    if (p.s[p.i] == '}') {
      p.i++;
      break;
    }
    err = "manifest: expected ',' or '}'";
    return false;
  }
  p.skip();
  if (p.i != p.s.size()) {
    err = "manifest: trailing content";
    return false;
  }
  out.present = true;
  return true;
}

bool extractOp(const std::string &request, std::string &op,
               std::string &err) {
  op.clear(); // parseString append (kontrak internal)
  P p{request};
  if (!p.expect('{', "'{'")) {
    err = "request must be a JSON object";
    return false;
  }
  for (;;) {
    p.skip();
    if (p.i >= p.s.size()) {
      err = "request: unterminated object";
      return false;
    }
    if (p.s[p.i] == '}') {
      err = "request needs string \"op\"";
      return false;
    }
    std::string key;
    if (!p.parseString(key)) {
      err = std::string("request: ") + p.err;
      return false;
    }
    if (!p.expect(':', "':'")) {
      err = std::string("request: ") + p.err;
      return false;
    }
    if (key == "op") {
      p.skip();
      if (!p.parseString(op)) {
        err = "request needs string \"op\"";
        return false;
      }
      return true; // sisa request milik plugin (tak divalidasi host)
    }
    if (!p.skipValue()) {
      err = std::string("request: ") + p.err;
      return false;
    }
    p.skip();
    if (p.i < p.s.size() && p.s[p.i] == ',') {
      p.i++;
      continue;
    }
    break;
  }
  err = "request needs string \"op\"";
  return false;
}

PluginResult runPlugin(const std::string &nsPath,
                       const PluginManifest &manifest,
                       const PluginLimits &lim, const std::string &request) {
  PluginResult r;
  if (manifest.present) {
    std::string op;
    std::string oerr;
    if (!extractOp(request, op, oerr)) {
      r.verdict = "refused";
      r.output = std::string("plugin: ") + oerr;
      return r;
    }
    bool allowed = false;
    for (const auto &a : manifest.ops) {
      if (a == op) {
        allowed = true;
        break;
      }
    }
    if (!allowed) {
      r.verdict = "refused";
      r.output = "plugin: op '" + op + "' not in manifest";
      return r;
    }
  }
  sandbox::SandboxLimits slim;
  slim.timeoutSec = lim.timeoutSec;
  slim.memoryMb = lim.memoryMb;
  slim.outputKb = lim.outputKb;
  std::string fed = request;
  if (fed.empty() || fed.back() != '\n')
    fed.push_back('\n'); // builtin input() membaca baris
  sandbox::SandboxResult s = sandbox::runSandboxedInput(nsPath, slim, fed);
  r.verdict = s.verdict;
  r.code = s.code;
  r.output = s.output;
  return r;
}

} // namespace plugin

int Driver::runPlugin(const std::string &nsPath,
                      const std::string &manifestPath, int timeoutSec,
                      long memoryMb, long outputKb,
                      const std::string &request) {
  plugin::PluginManifest manifest;
  if (!manifestPath.empty()) {
    bool ok = false;
    std::string source;
    // Baca utuh (biner-aman; manifest kecil).
    std::ifstream f(manifestPath, std::ios::binary);
    if (f) {
      std::ostringstream ss;
      ss << f.rdbuf();
      source = ss.str();
      ok = true;
    }
    if (!ok) {
      std::fprintf(stderr, "error: cannot read manifest '%s'\n",
                   manifestPath.c_str());
      return 1;
    }
    std::string merr;
    if (!plugin::parseManifest(source, manifest, merr)) {
      std::fprintf(stderr, "error: %s\n", merr.c_str());
      return 1;
    }
  }
  if (request.empty()) {
    std::fprintf(stderr, "error: empty request (pipe one JSON object, "
                         "e.g. echo '{\"op\":\"add\",\"a\":1}' | nusa "
                         "plugin run p.ns)\n");
    return 1;
  }
  plugin::PluginLimits lim;
  lim.timeoutSec = timeoutSec;
  lim.memoryMb = memoryMb;
  lim.outputKb = outputKb;
  plugin::PluginResult r =
      plugin::runPlugin(nsPath, manifest, lim, request);
  if (!r.output.empty())
    std::fwrite(r.output.data(), 1, r.output.size(), stdout);
  std::fprintf(stderr, "\n[plugin: verdict=%s code=%d]\n", r.verdict.c_str(),
               r.code);
  return (r.verdict == "ok" && r.code == 0) ? 0 : 1;
}

} // namespace nusa
