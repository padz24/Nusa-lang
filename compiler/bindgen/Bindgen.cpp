// Bindgen C subset (Phase 66, milestone 1). Tokenizer + parser
// prototipe mandiri; hanya header standar C++. Pemetaan = tabel
// docs/ffi.md; yang keluar → skip + warning (bukan fatal).
#include "compiler/bindgen/Bindgen.h"

#include <cctype>

namespace nusa {
namespace bindgen {
namespace {

// ---- Token ----

enum class K { Ident, Star, LParen, RParen, LBrace, RBrace, Comma, Semi, End };

struct Tok {
  K kind = K::End;
  std::string text;
};

static bool isIdentStart(char c) {
  return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool isIdentChar(char c) {
  return isIdentStart(c) || (c >= '0' && c <= '9');
}

// Strip komentar + baris `#`; hasilkan token bermakna.
static std::vector<Tok> lex(const std::string &src) {
  std::vector<Tok> out;
  size_t i = 0, n = src.size();
  bool lineStart = true;
  while (i < n) {
    char c = src[i];
    if (c == '\n') {
      lineStart = true;
      i++;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      i++;
      continue;
    }
    // Baris preprocessor (`#` pertama di baris): buang sebaris.
    if (c == '#' && lineStart) {
      while (i < n && src[i] != '\n')
        i++;
      continue;
    }
    lineStart = false;
    if (c == '/' && i + 1 < n && src[i + 1] == '/') {
      while (i < n && src[i] != '\n')
        i++;
      continue;
    }
    if (c == '/' && i + 1 < n && src[i + 1] == '*') {
      i += 2;
      while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/'))
        i++;
      i += 2;
      continue;
    }
    if (c == '"' || c == '\'') {
      // Literal: lewati (isi tak relevan untuk prototipe).
      char q = c;
      i++;
      while (i < n && src[i] != q) {
        if (src[i] == '\\')
          i++;
        i++;
      }
      i++;
      continue;
    }
    if (isIdentStart(c)) {
      size_t j = i;
      while (j < n && isIdentChar(src[j]))
        j++;
      out.push_back(Tok{K::Ident, src.substr(i, j - i)});
      i = j;
      continue;
    }
    switch (c) {
    case '*': out.push_back(Tok{K::Star, "*"}); break;
    case '(': out.push_back(Tok{K::LParen, "("}); break;
    case ')': out.push_back(Tok{K::RParen, ")"}); break;
    case '{': out.push_back(Tok{K::LBrace, "{"}); break;
    case '}': out.push_back(Tok{K::RBrace, "}"}); break;
    case ',': out.push_back(Tok{K::Comma, ","}); break;
    case ';': out.push_back(Tok{K::Semi, ";"}); break;
    default:
      // Angka, `.`, `=`, `[`, `]`, dsb.: tak bermakna di level
      // prototipe (inisialisasi/array dilewati via skip seimbang).
      out.push_back(Tok{K::End, std::string(1, c)});
      break;
    }
    i++;
  }
  return out;
}

// ---- Klasifikasi tipe ----

// Nusa type hasil peta, atau "" bila tak terpetakan.
static std::string mapType(const std::vector<std::string> &words, int stars,
                           bool isReturn, std::string &why) {
  auto has = [&](const char *w) {
    for (const auto &x : words) {
      if (x == w)
        return true;
    }
    return false;
  };
  // Normalisasi dulu: buang qualifier, lalu peta typedef builtin.
  // (Urutan penting: `const char *` harus dikenali sebagai char.)
  std::vector<std::string> ws;
  for (const auto &x : words) {
    if (x == "const" || x == "volatile")
      continue;
    if (x == "size_t" || x == "ssize_t" || x == "intptr_t" ||
        x == "uintptr_t" || x == "int64_t" || x == "uint64_t")
      ws.push_back("long");
    else if (x == "int32_t" || x == "uint32_t")
      ws.push_back("int");
    else if (x == "int16_t" || x == "uint16_t")
      ws.push_back("short");
    else if (x == "int8_t" || x == "uint8_t")
      ws.push_back("char");
    else
      ws.push_back(x);
  }
  bool isVoid = ws.size() == 1 && ws[0] == "void";
  bool isBool = ws.size() == 1 && (ws[0] == "_Bool" || ws[0] == "bool");
  bool isChar = ws.size() == 1 && ws[0] == "char";
  bool isInt = false;
  if (!isVoid && !isBool && stars <= 1) {
    // Integer: hanya kata dari himpunan {signed,unsigned,short,int,
    // long,char} dengan minimal satu kata ukuran/nama. Polos
    // `unsigned`/`signed` = int (aturan C).
    bool named = false, okw = true, signOnly = false;
    for (const auto &x : ws) {
      if (x == "signed" || x == "unsigned") {
        signOnly = true;
        continue;
      }
      if (x == "short" || x == "int" || x == "long" || x == "char") {
        named = true;
        continue;
      }
      okw = false;
    }
    isInt = okw && (named || signOnly) && !ws.empty();
  }
  if (stars > 1) {
    why = "multiple pointers";
    return "";
  }
  if (stars == 1) {
    if (isChar) {
      if (isReturn) {
        why = "string return (lifetime tak diketahui)";
        return "";
      }
      return "string";
    }
    why = "pointer type";
    return "";
  }
  if (isVoid) {
    if (isReturn)
      return "void";
    why = "void parameter";
    return "";
  }
  if (isBool)
    return "bool";
  if (isInt)
    return "i64";
  if (isChar)
    return "i64";
  why = "unsupported type";
  return "";
}

struct Param {
  std::string name;
  std::string type; // Nusa ("i64"/"bool"/"string")
};

struct Func {
  std::string name;
  std::string ret; // Nusa ("i64"/"bool"/"void")
  std::vector<Param> params;
  bool skip = false;
  std::string why;
};

} // namespace

BindResult generateBindings(const std::string &file,
                            const std::string &source) {
  (void)file;
  BindResult r;
  std::vector<Tok> toks = lex(source);
  std::vector<Func> funcs;
  std::vector<std::string> warnings;
  size_t i = 0, n = toks.size();

  auto skipBalanced = [&](K open, K close, size_t &k) {
    int depth = 0;
    while (k < n) {
      if (toks[k].kind == open)
        depth++;
      else if (toks[k].kind == close) {
        depth--;
        if (depth == 0) {
          k++;
          return;
        }
      }
      k++;
    }
  };

  while (i < n) {
    // Lewati token non-identifier (simbol nyasar level atas).
    if (toks[i].kind != K::Ident) {
      if (toks[i].kind == K::LBrace)
        skipBalanced(K::LBrace, K::RBrace, i);
      else
        i++;
      continue;
    }
    // Kumpulkan kata tipe + `*` hingga identifier-nama + `(`.
    std::vector<std::string> words;
    int stars = 0;
    size_t j = i;
    std::string fname;
    bool maybeFunc = false;
    while (j < n) {
      if (toks[j].kind == K::Ident) {
        // Nama bila token berikut `(`; bila tidak, bagian tipe
        // (atau keyword seperti struct/typedef → bukan fungsi).
        if (j + 1 < n && toks[j + 1].kind == K::LParen) {
          fname = toks[j].text;
          maybeFunc = true;
          j++; // di `(`
          break;
        }
        words.push_back(toks[j].text);
        j++;
      } else if (toks[j].kind == K::Star) {
        stars++;
        j++;
      } else if (toks[j].kind == K::LParen && !words.empty() &&
                 (words.back() == "__attribute__" ||
                  words.back() == "__declspec")) {
        // Atribut di posisi tipe (jarang): lewati seimbang.
        int depth = 0;
        while (j < n) {
          if (toks[j].kind == K::LParen)
            depth++;
          else if (toks[j].kind == K::RParen) {
            depth--;
            if (depth == 0) {
              j++;
              break;
            }
          }
          j++;
        }
        words.pop_back();
      } else {
        break;
      }
    }
    if (!maybeFunc) {
      // Bukan prototipe: lewati hingga `;` atau blok seimbang
      // (typedef/struct/union/enum/variabel). Pengecualian:
      // `extern "C" {` — lewati `{`-nya SAJA agar prototipe di
      // dalamnya tetap terparse.
      bool externBlock = !words.empty() && words[0] == "extern";
      while (j < n && toks[j].kind != K::Semi &&
             toks[j].kind != K::LBrace)
        j++;
      if (j < n && toks[j].kind == K::LBrace) {
        if (externBlock) {
          j++; // masuk blok; `}` penutup ditangani sebagai token lepas
        } else {
          skipBalanced(K::LBrace, K::RBrace, j);
        }
      } else if (j < n) {
        j++;
      }
      i = (j > i) ? j : i + 1;
      continue;
    }
    // j di `(`: parse daftar parameter hingga `)` seimbang.
    j++; // lewati `(`
    std::vector<std::vector<Tok>> rawParams;
    std::vector<Tok> cur;
    int depth = 1;
    while (j < n && depth > 0) {
      if (toks[j].kind == K::LParen)
        depth++;
      else if (toks[j].kind == K::RParen) {
        depth--;
        if (depth == 0) {
          j++;
          break;
        }
      }
      if (depth == 1 && toks[j].kind == K::Comma) {
        rawParams.push_back(cur);
        cur.clear();
      } else {
        cur.push_back(toks[j]);
      }
      j++;
    }
    if (depth != 0)
      break; // tak seimbang: hentikan (input terpotong)
    // Parameter kosong `()` atau `(void)` = nol param.
    bool emptyParams = rawParams.empty() && cur.empty();
    if (!emptyParams)
      rawParams.push_back(cur);
    // Sesudah `)`: atribut opsional, lalu `;` (deklarasi) atau
    // `{` (definisi → lewati) atau akhir.
    while (j < n && toks[j].kind == K::Ident &&
           (toks[j].text == "__attribute__" ||
            toks[j].text == "__cdecl" || toks[j].text == "__stdcall")) {
      j++;
      if (j < n && toks[j].kind == K::LParen)
        skipBalanced(K::LParen, K::RParen, j);
    }
    bool isProto = (j < n && toks[j].kind == K::Semi);
    bool isDef = (j < n && toks[j].kind == K::LBrace);
    if (isDef)
      skipBalanced(K::LBrace, K::RBrace, j);
    else if (isProto)
      j++;
    // Klasifikasi (definisi yang dilewati tak diklasifikasi).
    Func f;
    f.name = fname;
    if (isDef) {
      i = j;
      continue; // definisi (static inline): diam-diam dilewati
    }
    if (!isProto) {
      i = j;
      continue; // bukan deklarasi lengkap
    }
    // `extern "C"` sebagai "fungsi" semu: abaikan.
    if (fname == "extern") {
      i = j;
      continue;
    }
    // Awalan `extern` pada deklarasi tunggal (`extern "C" void f();`).
    if (!words.empty() && words[0] == "extern")
      words.erase(words.begin());
    std::string why;
    f.ret = mapType(words, stars, true, why);
    if (f.ret.empty()) {
      f.skip = true;
      f.why = why;
    } else {
      int pno = 0;
      for (auto &rp : rawParams) {
        // Param kosong (dari koma ganda) → tolak fungsi.
        // `(void)` tunggal → nol param.
        std::vector<std::string> pwords;
        int pstars = 0;
        std::string pname;
        for (size_t k = 0; k < rp.size(); k++) {
          if (rp[k].kind == K::Ident &&
              (k + 1 == rp.size() || rp[k + 1].kind == K::Comma)) {
            // Identifier terakhir = kandidat nama.
            pname = rp[k].text;
          } else if (rp[k].kind == K::Ident) {
            pwords.push_back(rp[k].text);
          } else if (rp[k].kind == K::Star) {
            pstars++;
          } else {
            pwords.push_back("?");
          }
        }
        if (pwords.empty() && pname.empty())
          continue; // koma ganda: abaikan slot
        if (pwords.empty() && pname == "void" && rawParams.size() == 1 &&
            pstars == 0) {
          continue; // `(void)` = nol param
        }
        std::string pname2 = pname.empty()
                                 ? ("p" + std::to_string(pno))
                                 : pname;
        // `void x` (bernama) atau `?` (simbol dalam) → tolak.
        std::string pwhy;
        std::string pt = mapType(pwords, pstars, false, pwhy);
        if (pt.empty() ||
            (pwords.size() == 1 && pwords[0] == "void")) {
          f.skip = true;
          f.why = pwhy.empty() ? "bad parameter" : pwhy;
          break;
        }
        // Nama `void` tanpa tipe lain = posisi `(void)` yang salah
        // letak → tolak.
        if (pname == "void") {
          f.skip = true;
          f.why = "bad parameter";
          break;
        }
        f.params.push_back(Param{pname2, pt});
        pno++;
      }
    }
    if (f.skip)
      warnings.push_back("skip " + f.name + ": " + f.why);
    else
      funcs.push_back(f);
    i = j;
  }

  if (funcs.empty()) {
    r.ok = false;
    r.error = "no bindable functions found";
    return r;
  }
  std::string out;
  for (const auto &f : funcs) {
    out += "extern fn " + f.name + "(";
    for (size_t k = 0; k < f.params.size(); k++) {
      if (k)
        out += ", ";
      out += f.params[k].name + ": " + f.params[k].type;
    }
    out += ")";
    if (f.ret != "void")
      out += " -> " + f.ret;
    out += ";\n";
  }
  r.ok = true;
  r.code = out;
  r.warnings = warnings;
  return r;
}

} // namespace bindgen
} // namespace nusa
