// Project templates `nusa new` (Phase 43, docs/ecosystem.md).
//
// Empat template tertanam (sumber-tunggal, tanpa path runtime):
// hello-world, cli-app, library, server. Setiap hasil scaffold
// WAJIB build+run hijau + fmt bersih (dikunci e2e_new.sh).
#include "compiler/pkg/Pkg.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace nusa {
namespace pkg {
namespace {

namespace fs = std::filesystem;

bool validPkgName(const std::string &n) {
  if (n.empty())
    return false;
  if (!(n[0] == '_' || (n[0] >= 'A' && n[0] <= 'Z') ||
        (n[0] >= 'a' && n[0] <= 'z')))
    return false;
  for (char c : n) {
    if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9')))
      return false;
  }
  return true;
}

std::string subst(std::string t, const std::string &name) {
  std::string o;
  for (size_t i = 0; i < t.size(); i++) {
    if (t.compare(i, 8, "{{NAME}}") == 0) {
      o += name;
      i += 7;
    } else {
      o.push_back(t[i]);
    }
  }
  return o;
}

struct TplFile {
  const char *path;
  const char *text;
};

// Catatan: contoh .ns di sini WAJIB lolos `nusa fmt --check`
// (gaya repo) dan WAJIB build+run (e2e_new.sh).

const TplFile kHello[] = {
    {"nusa.toml",
     "[package]\nname = \"{{NAME}}\"\nversion = \"0.1.0\"\n"
     "compiler = \"__NUSA_VERSION__\"\n"
     "description = \"Hello-world Nusa\"\n"
     "\n[capabilities]\nfilesystem = false\nnetwork = false\nprocess = false\nenvironment = false\nnative = false\n"},
    {"main.ns",
     "fn main() {\n    print(\"Hello, {{NAME}}!\");\n}\n"},
    {"README.md",
     "# {{NAME}}\n\n```sh\nnusa pkg build\n./{{NAME}}\n```\n"},
};

const TplFile kCli[] = {
    {"nusa.toml",
     "[package]\nname = \"{{NAME}}\"\nversion = \"0.1.0\"\n"
     "compiler = \"__NUSA_VERSION__\"\n"
     "description = \"CLI app Nusa\"\n"
     "\n[capabilities]\nfilesystem = false\nnetwork = false\nprocess = true\nenvironment = false\nnative = false\n"},
    {"main.ns",
     "fn usage() {\n"
     "    print(\"usage: {{NAME}} <name>\");\n"
     "}\n"
     "fn main() {\n"
     "    if process_argc() < 2 {\n"
     "        usage();\n"
     "    } else {\n"
     "        print(str_concat(\"Hello, \", process_argv(1)));\n"
     "    }\n"
     "}\n"},
    {"README.md",
     "# {{NAME}}\n\n```sh\nnusa pkg build\n./{{NAME}} Nusa\n```\n"},
};

const TplFile kLib[] = {
    {"nusa.toml",
     "[package]\nname = \"{{NAME}}\"\nversion = \"0.1.0\"\n"
     "compiler = \"__NUSA_VERSION__\"\n"
     "description = \"Library Nusa\"\n"
     "\n[capabilities]\nfilesystem = false\nnetwork = false\nprocess = false\nenvironment = false\nnative = false\n"},
    {"calc.ns",
     "fn add(a: i64, b: i64) -> i64 {\n"
     "    return a + b;\n"
     "}\n"
     "fn fact(n: i64) -> i64 {\n"
     "    let r = 1;\n"
     "    let i = 2;\n"
     "    while i <= n {\n"
     "        r = r * i;\n"
     "        i = i + 1;\n"
     "    }\n"
     "    return r;\n"
     "}\n"},
    {"calc_test.ns",
     "import calc;\n"
     "fn main() {\n"
     "    assert(add(2, 3) == 5);\n"
     "    assert(fact(5) == 120);\n"
     "    assert(fact(0) == 1);\n"
     "}\n"},
    {"README.md",
     "# {{NAME}}\n\n```sh\nnusa test\n```\n"},
};

const TplFile kServer[] = {
    {"nusa.toml",
     "[package]\nname = \"{{NAME}}\"\nversion = \"0.1.0\"\n"
     "compiler = \"__NUSA_VERSION__\"\n"
     "description = \"Server loopback Nusa\"\n"
     "\n[capabilities]\nfilesystem = false\nnetwork = true\nprocess = false\nenvironment = false\nnative = false\n"},
    {"handler.ns",
     "fn handle(path: string) -> string {\n"
     "    match path {\n"
     "        \"/\" => {\n"
     "            return \"Hello, Nusa!\";\n"
     "        },\n"
     "        _ => {\n"
     "            return \"not found\";\n"
     "        },\n"
     "    }\n"
     "}\n"},
    {"main.ns",
     "// Server loopback: handler murni (handler.ns, teruji di\n"
     "// server_test.ns) + accept SATU koneksi lalu keluar\n"
     "// (model serve-once: deterministik + teruji; loop abadi\n"
     "// menunggu executor — lihat RFC-0004).\n"
     "import handler;\n"
     "fn main() {\n"
     "    if process_argc() < 2 {\n"
     "        print(\"usage: {{NAME}} <port>\");\n"
     "        return;\n"
     "    }\n"
     "    let srv = tcp_listen(parse_int(process_argv(1)));\n"
     "    if srv < 0 {\n"
     "        print(\"listen failed\");\n"
     "        return;\n"
     "    }\n"
     "    print(\"serving\");\n"
     "    let c = tcp_accept(srv);\n"
     "    if c < 0 {\n"
     "        print(\"accept failed\");\n"
     "        return;\n"
     "    }\n"
     "    let req = tcp_recv(c, 256);\n"
     "    tcp_send(c, handle(req));\n"
     "    tcp_close(c);\n"
     "    tcp_close(srv);\n"
     "}\n"},
    {"server_test.ns",
     "import handler;\n"
     "fn main() {\n"
     "    assert(str_len(handle(\"/\")) == 12);\n"
     "    assert(str_len(handle(\"/x\")) == 9);\n"
     "}\n"},
    {"README.md",
     "# {{NAME}}\n\nServer loopback serve-once: `handle` murni + teruji\n"
     "(`nusa test`). Menjalankan: `./{{NAME}} <port>` lalu hubungi\n"
     "satu koneksi TCP (loop abadi menunggu executor: RFC-0004).\n"},
};

struct Template {
  const char *name;
  const TplFile *files;
  size_t count;
  const char *entry; // file untuk build/run smoke
};

const Template kTemplates[] = {
    {"hello-world", kHello, sizeof(kHello) / sizeof(kHello[0]), "main.ns"},
    {"cli-app", kCli, sizeof(kCli) / sizeof(kCli[0]), "main.ns"},
    {"library", kLib, sizeof(kLib) / sizeof(kLib[0]), ""},
    {"server", kServer, sizeof(kServer) / sizeof(kServer[0]), "main.ns"},
};

bool writeText(const std::string &path, const std::string &data,
               std::string &err) {
  std::ofstream o(path, std::ios::binary | std::ios::trunc);
  if (!o) {
    err = "cannot write '" + path + "'";
    return false;
  }
  o << data;
  o.close();
  if (!o) {
    err = "cannot write '" + path + "'";
    return false;
  }
  return true;
}

} // namespace

int runNew(const std::vector<std::string> &args) {
  std::string tplName, name, dirFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--list") {
      std::printf("templates:\n");
      for (const auto &t : kTemplates)
        std::printf("  %s\n", t.name);
      return 0;
    } else if (args[i] == "--dir" && i + 1 < args.size()) {
      dirFlag = args[++i];
    } else if (!args[i].empty() && args[i][0] == '-') {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    } else if (tplName.empty()) {
      tplName = args[i];
    } else if (name.empty()) {
      name = args[i];
    } else {
      std::fprintf(stderr, "error: unexpected argument '%s'\n",
                   args[i].c_str());
      return 1;
    }
  }
  if (tplName.empty()) {
    std::fprintf(stderr,
                 "error: 'nusa new' needs a template "
                 "(see 'nusa new --list')\n");
    return 1;
  }
  const Template *tpl = nullptr;
  for (const auto &t : kTemplates) {
    if (tplName == t.name) {
      tpl = &t;
      break;
    }
  }
  if (!tpl) {
    std::fprintf(stderr, "error: unknown template '%s'\n", tplName.c_str());
    std::fprintf(stderr, "help: see 'nusa new --list'\n");
    return 1;
  }
  if (name.empty()) {
    name = tplName;
    for (auto &c : name) {
      if (c == '-')
        c = '_';
    }
  }
  if (!validPkgName(name)) {
    std::fprintf(stderr,
                 "error: invalid package name '%s' (want "
                 "[A-Za-z_][A-Za-z0-9_]*)\n",
                 name.c_str());
    return 1;
  }
  std::string dir = dirFlag.empty() ? name : dirFlag;
  std::error_code ec;
  bool exists = fs::exists(dir, ec);
  if (!ec && exists && fs::is_directory(dir, ec) && !ec) {
    bool empty = fs::directory_iterator(dir, ec) == fs::directory_iterator();
    if (ec || !empty) {
      std::fprintf(stderr, "error: directory '%s' already exists and is "
                           "not empty\n",
                   dir.c_str());
      return 1;
    }
  }
  fs::create_directories(dir, ec);
  if (ec) {
    std::fprintf(stderr, "error: cannot create '%s'\n", dir.c_str());
    return 1;
  }
  for (size_t i = 0; i < tpl->count; i++) {
    std::string text = subst(tpl->files[i].text, name);
    // Versi compiler minimum = compiler yang men-scaffold.
    const char *ver = "__NUSA_VERSION__";
    size_t p = text.find(ver);
    if (p != std::string::npos)
      text.replace(p, std::string(ver).size(), compilerVersion());
    std::string err;
    std::string path = (fs::path(dir) / tpl->files[i].path).string();
    if (fs::is_regular_file(path, ec) && !ec) {
      std::fprintf(stderr, "error: '%s' already exists\n", path.c_str());
      return 1;
    }
    if (!writeText(path, text, err)) {
      std::fprintf(stderr, "error: %s\n", err.c_str());
      return 1;
    }
  }
  std::printf("Created %s '%s' in '%s/'\n", tpl->name, name.c_str(),
              dir.c_str());
  return 0;
}

} // namespace pkg
} // namespace nusa
