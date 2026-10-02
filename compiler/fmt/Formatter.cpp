#include "compiler/fmt/Formatter.h"
#include "compiler/lexer/Lexer.h"
#include <unordered_set>

namespace nusa {
namespace {

bool isCloser(TokenKind k) {
  return k == TokenKind::RParen || k == TokenKind::RBracket ||
         k == TokenKind::RBrace || k == TokenKind::Semi ||
         k == TokenKind::Comma || k == TokenKind::Colon ||
         k == TokenKind::Dot || k == TokenKind::Eof;
}

bool isOpener(TokenKind k) {
  return k == TokenKind::LParen || k == TokenKind::LBracket;
}

bool isOperator(TokenKind k) {
  return k == TokenKind::Plus || k == TokenKind::Minus ||
         k == TokenKind::Star || k == TokenKind::Slash ||
         k == TokenKind::Assign || k == TokenKind::Eq ||
         k == TokenKind::NotEq || k == TokenKind::Lt || k == TokenKind::Gt ||
         k == TokenKind::LtEq || k == TokenKind::GtEq ||
         k == TokenKind::Arrow;
}

std::string escapeString(const std::string &s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\t': out += "\\t"; break;
    case '\r': out += "\\r"; break;
    case '\0': out += "\\0"; break;
    default: out.push_back(static_cast<char>(c)); break;
    }
  }
  out += "\"";
  return out;
}

std::string spellToken(const Token &t) {
  if (t.kind == TokenKind::StringLit)
    return escapeString(t.lexeme);
  return t.lexeme;
}

} // namespace

FormatResult formatSource(const std::string &file, const std::string &source) {
  Lexer lexer(file, source);
  std::vector<Token> tokens = lexer.tokenize();
  if (!lexer.errors().empty()) {
    FormatResult r;
    r.ok = false;
    r.error = lexer.errors().front();
    return r;
  }
  const std::vector<Comment> &comments = lexer.comments();
  size_t ci = 0; // komentar berikutnya

  // Tanda `<...>` generik (Phase 21c/21d): `fn f<T>(...)`, `Point<i64>` —
  // direkat tanpa spasi. Pindai seimbang dari `Ident <`: isi hanya token
  // tipe (termasuk nested `<>`, `[]`, `fn()`); terminator `>` harus
  // diikuti kelanjutan posisi-tipe (`(` `)` `,` `;` `=` `]` `->` `{`).
  // Di kode VALID, bentuk ini tak pernah muncul sebagai perbandingan
  // (`a<b` tak diikuti pola tersebut; turbofish tak ada).
  auto kindAt = [&](size_t k) {
    return (k < tokens.size()) ? tokens[k].kind : TokenKind::Eof;
  };
  auto isTypeTok = [&](TokenKind k) {
    return k == TokenKind::Ident || k == TokenKind::I64 ||
           k == TokenKind::Bool || k == TokenKind::String ||
           k == TokenKind::Void || k == TokenKind::Fn ||
           k == TokenKind::Comma || k == TokenKind::LBracket ||
           k == TokenKind::RBracket || k == TokenKind::LParen ||
           k == TokenKind::RParen || k == TokenKind::Lt ||
           k == TokenKind::Gt || k == TokenKind::Arrow;
  };
  auto isTypeCont = [&](TokenKind k) {
    return k == TokenKind::LParen || k == TokenKind::RParen ||
           k == TokenKind::Comma || k == TokenKind::Semi ||
           k == TokenKind::Assign || k == TokenKind::RBracket ||
           k == TokenKind::Arrow || k == TokenKind::LBrace;
  };
  std::unordered_set<size_t> genericBrackets;
  for (size_t i = 0; i + 4 < tokens.size(); i++) {
    if (tokens[i].kind != TokenKind::Ident ||
        tokens[i + 1].kind != TokenKind::Lt)
      continue;
    size_t j = i + 2;
    int depth = 1;
    bool ok = false;
    while (isTypeTok(kindAt(j))) {
      if (kindAt(j) == TokenKind::Lt) {
        depth++;
      } else if (kindAt(j) == TokenKind::Gt) {
        depth--;
        if (depth == 0) {
          ok = (j > i + 2) && isTypeCont(kindAt(j + 1));
          break;
        }
      }
      j++;
    }
    if (!ok)
      continue;
    for (size_t k = i + 1; k <= j; k++) {
      if (tokens[k].kind == TokenKind::Lt ||
          tokens[k].kind == TokenKind::Gt)
        genericBrackets.insert(k);
    }
  }

  std::string out;
  int indent = 0;
  bool lineStart = true; // di awal baris (butuh indent)
  auto emitIndent = [&]() {
    if (lineStart) {
      out.append(static_cast<size_t>(indent) * 4, ' ');
      lineStart = false;
    }
  };
  auto newline = [&]() {
    out.push_back('\n');
    lineStart = true;
  };
  // Komentar own-line sebelum baris asli `line` (eksklusif).
  // Phase 57 (2.8.0): `///` dokumentasi dipertahankan (flag doc
  // lexer) — dulu terdegradasi menjadi `//` oleh formatter.
  auto flushCommentsBefore = [&](int line) {
    while (ci < comments.size() && comments[ci].line < line) {
      emitIndent();
      out += comments[ci].doc ? "///" : "//";
      out += comments[ci].text;
      newline();
      ci++;
    }
  };
  // Komentar trailing di baris asli `line` (maks 1 per baris).
  auto flushTrailing = [&](int line) {
    while (ci < comments.size() && comments[ci].line == line) {
      out += comments[ci].doc ? " ///" : " //";
      out += comments[ci].text;
      ci++;
    }
  };

  for (size_t i = 0; i < tokens.size(); i++) {
    const Token &t = tokens[i];
    if (t.kind == TokenKind::Eof)
      break;
    TokenKind next =
        (i + 1 < tokens.size()) ? tokens[i + 1].kind : TokenKind::Eof;
    flushCommentsBefore(t.loc.line);

    if (t.kind == TokenKind::LBrace) {
      emitIndent();
      // `{` tak pernah di awal baris kecuali kasus aneh — beri spasi
      // hanya bila bukan awal baris.
      out += lineStart ? "{" : " {";
      flushTrailing(t.loc.line);
      newline();
      indent++;
      continue;
    }
    if (t.kind == TokenKind::RBrace) {
      indent = indent > 0 ? indent - 1 : 0;
      // Hindari baris kosong ganda: bila sudah di awal baris, langsung.
      if (!lineStart)
        newline();
      emitIndent();
      out += "}";
      flushTrailing(t.loc.line);
      if (next == TokenKind::Else) {
        out += " ";
        lineStart = false;
      } else if (next == TokenKind::Semi) {
        // `};` — biarkan `;` yang menutup baris.
        lineStart = false;
      } else {
        newline();
      }
      continue;
    }
    if (t.kind == TokenKind::Semi) {
      emitIndent();
      out += ";";
      flushTrailing(t.loc.line);
      newline();
      continue;
    }
    emitIndent();
    if ((t.kind == TokenKind::Lt || t.kind == TokenKind::Gt) &&
        genericBrackets.count(i)) {
      // Rekatkan bracket generik (`f<T>`, `T>`); spasi di sekitar koma
      // di dalam sudah ditangani cabang Comma/Ident biasa.
      while (!out.empty() && out.back() == ' ')
        out.pop_back();
      out += spellToken(t);
      continue;
    }
    if (isOperator(t.kind)) {
      // Unary `-`/`+` menempel ke operand (konteks = token sebelumnya:
      // awal/operator/opener/`,`/`:`/`return`/`{`); biner dikelilingi spasi.
      // Konteks sebelumnya selalu diakhiri spasi atau opener, jadi unary
      // cukup di-emit tanpa spasi tambahan (idempoten via cleanup).
      TokenKind prevKind =
          (i > 0) ? tokens[i - 1].kind : TokenKind::Eof;
      bool unary = (t.kind == TokenKind::Minus || t.kind == TokenKind::Plus) &&
                   (prevKind == TokenKind::Eof || isOperator(prevKind) ||
                    isOpener(prevKind) || prevKind == TokenKind::Comma ||
                    prevKind == TokenKind::Colon ||
                    prevKind == TokenKind::Return ||
                    prevKind == TokenKind::LBrace);
      if (unary) {
        out += spellToken(t);
      } else {
        out += " " + spellToken(t) + " ";
      }
      // Spasi ganda potensial ("a  +  b") dirapikan di akhir.
      continue;
    }
    if (t.kind == TokenKind::Comma) {
      // Koma pemisah arm `match` → akhiri baris (deteksi: `=>` di depan
      // pada kedalaman 0 sebelum `;`/`}`/`)`/`]`/EOF menutup konteks).
      // Koma biasa (argumen, array, struct) tak tersentuh.
      bool armSep = false;
      {
        int depth = 0;
        for (size_t j = i + 1; j < tokens.size(); j++) {
          TokenKind k = tokens[j].kind;
          if (k == TokenKind::LParen || k == TokenKind::LBracket ||
              k == TokenKind::LBrace) {
            depth++;
            continue;
          }
          if (k == TokenKind::RParen || k == TokenKind::RBracket ||
              k == TokenKind::RBrace || k == TokenKind::Semi ||
              k == TokenKind::Eof) {
            if (depth == 0)
              break; // keluar konteks → bukan pemisah arm
            depth--;
            continue;
          }
          if (k == TokenKind::MatchArrow && depth == 0) {
            armSep = true;
            break;
          }
        }
      }
      // Koma trailing sebelum `}` penutup match (`_, }`) tak terlihat
      // forward-scan: fallback backward (di dalam arm bila `=>` temu
      // pada kedalaman 0 sebelum pembuka tak-berpasangan/`;`).
      if (!armSep && next == TokenKind::RBrace) {
        int depth = 0;
        for (size_t j = i; j-- > 0;) {
          TokenKind k = tokens[j].kind;
          if (k == TokenKind::RBrace || k == TokenKind::RParen ||
              k == TokenKind::RBracket) {
            depth++;
            continue;
          }
          if (k == TokenKind::LBrace || k == TokenKind::LParen ||
              k == TokenKind::LBracket) {
            if (depth == 0)
              break;
            depth--;
            continue;
          }
          if (k == TokenKind::Semi && depth == 0)
            break;
          if (k == TokenKind::MatchArrow && depth == 0) {
            armSep = true;
            break;
          }
        }
      }
      if (armSep) {
        // Tempel ke baris sebelumnya: hapus indent + newline yang telanjur
        // di-emit emitIndent() (atau spasi ekor bila mid-line).
        while (!out.empty() && out.back() == ' ')
          out.pop_back();
        if (!out.empty() && out.back() == '\n')
          out.pop_back();
        out += ",";
        flushTrailing(t.loc.line);
        newline();
        continue;
      }
      out += ", ";
      continue;
    }
    if (t.kind == TokenKind::Colon) {
      out += ": ";
      continue;
    }
    if (t.kind == TokenKind::Dot) {
      out += ".";
      continue;
    }
    if (t.kind == TokenKind::Question) {
      // Propagasi `?` menempel (cermin Dot; lihat menempel-next).
      out += "?";
      continue;
    }
    if (t.kind == TokenKind::Pipe) {
      // Bar lambda `|x: i64| ...` (Phase 27): buka menempel ke dalam
      // (`|x`), tutup menempel ke pola (`x|`) + spasi sesudah.
      TokenKind prevKind =
          (i > 0) ? tokens[i - 1].kind : TokenKind::Eof;
      bool prevIsOpener =
          (i == 0) || prevKind == TokenKind::Assign ||
          isOperator(prevKind) || isOpener(prevKind) ||
          prevKind == TokenKind::Comma || prevKind == TokenKind::Colon ||
          prevKind == TokenKind::Return || prevKind == TokenKind::LBrace ||
          prevKind == TokenKind::Semi || prevKind == TokenKind::Pipe ||
          prevKind == TokenKind::MatchArrow;
      while (!out.empty() && out.back() == ' ')
        out.pop_back();
      // Spasi sebelum `|` buka (kecuali pasangan `||` menempel).
      // Phase 50 (2.1.0): sesudah `(`/`[` menempel tanpa spasi
      // (`(|x|`, bukan `( |x|`) — konsisten dengan call/indexing.
      if (!lineStart && !out.empty() && out.back() != '\n' && prevIsOpener &&
          prevKind != TokenKind::Pipe && prevKind != TokenKind::LParen &&
          prevKind != TokenKind::LBracket)
        out += " ";
      out += "|";
      // Tanpa spasi sesudah hanya untuk `|` buka (`|x`, `||`);
      // `|` tutup (`x|`, `||` kedua) butuh spasi.
      if (!(prevIsOpener && prevKind != TokenKind::Pipe &&
            next != TokenKind::Pipe))
        out += " ";
      continue;
    }
    if (t.kind == TokenKind::LParen || t.kind == TokenKind::LBracket) {
      // `[` literal array butuh spasi sesudah operator/koma/colon/return
      // (`= [1]`, `: [i64]`); `[` indexing/argumen menempel (`a[0]`, `([1])`).
      // `(` grouping butuh spasi yang sama (`a - (b)`, `= (a)`); call
      // menempel (`f(`). Phase 100 (3.1-foundation): `(` dulu selalu
      // menempel sehingga `a - (b)` menjadi `a -(b)` — kini cermin `[`.
      bool keepSpace = false;
      if (t.kind == TokenKind::LBracket && i > 0) {
        TokenKind pk = tokens[i - 1].kind;
        keepSpace = isOperator(pk) || pk == TokenKind::Comma ||
                    pk == TokenKind::Colon || pk == TokenKind::Return;
      } else if (t.kind == TokenKind::LParen && i > 0) {
        TokenKind pk = tokens[i - 1].kind;
        // `(` grouping sesudah operator ARITMETIKA/`=`/`, `/`:`/`return`
        // beri spasi (`a - (b)`, `= (a)`); call menempel (`f(`).
        // Phase 100 (3.1-foundation): `(` dulu selalu menempel sehingga
        // `a - (b)` menjadi `a -(b)` — kini cermin `[` literal (v1.1.0).
        // SENGAJA dikecualikan: `<`/`>`/`<=`/`>=`/`==`/`!=`/`->` tetap
        // menempel (status quo 3.0.0) — `>` di sini ambigu dengan penutup
        // generik berjarak (`f < T: Show >(v)`, tutorials/04) yang
        // pemindai `genericBrackets` tak kenali (isi `:`); memberi spasi
        // akan merusak `e2e_ecosystem`. Perbandingan + grouping
        // (`a < (b)` → `a <(b)`) tetap quirk warisan, bukan regresi.
        if (pk == TokenKind::Plus || pk == TokenKind::Minus ||
            pk == TokenKind::Star || pk == TokenKind::Slash ||
            pk == TokenKind::Assign || pk == TokenKind::Comma ||
            pk == TokenKind::Colon || pk == TokenKind::Return) {
          keepSpace = true;
          // Unary `-`/`+` sebelum `(` tetap menempel (`-(a)`), bukan
          // biner (`a - (b)`): bedakan via konteks token sebelum operator.
          if ((pk == TokenKind::Minus || pk == TokenKind::Plus) && i >= 2) {
            TokenKind ppk = tokens[i - 2].kind;
            if (ppk == TokenKind::Eof || isOperator(ppk) ||
                isOpener(ppk) || ppk == TokenKind::Comma ||
                ppk == TokenKind::Colon || ppk == TokenKind::Return ||
                ppk == TokenKind::LBrace)
              keepSpace = false;
          }
        }
      }
      if (keepSpace) {
        if (!out.empty() && out.back() != ' ' && out.back() != '\n')
          out += " ";
      } else {
        // Hapus spasi sebelum opener yang mungkin tertinggal.
        while (!out.empty() && out.back() == ' ')
          out.pop_back();
      }
      out += spellToken(t);
      continue;
    }
    if (isCloser(t.kind)) {
      while (!out.empty() && out.back() == ' ')
        out.pop_back();
      out += spellToken(t);
      if (t.kind == TokenKind::RParen || t.kind == TokenKind::RBracket) {
        if (!isCloser(next) && next != TokenKind::Question &&
            next != TokenKind::Eof)
          out += " ";
      }
      continue;
    }
    // Ident/keyword/literal: spasi pemisah kecuali sesudah opener/awal
    // atau unary `-`/`+` atau bracket generik `<`/`>`. (Operator biner
    // selalu diakhiri spasi oleh cabangnya, jadi back '-'/' +' pasti
    // berasal dari unary.)
    if (!lineStart && !out.empty() && out.back() != ' ' && out.back() != '\n' &&
        out.back() != '(' && out.back() != '[' && out.back() != '.' &&
        out.back() != '-' && out.back() != '+' && out.back() != '<' &&
        out.back() != '>' && out.back() != '|')
      out += " ";
    out += spellToken(t);
    if (next == TokenKind::LParen || next == TokenKind::LBracket ||
        next == TokenKind::Dot || next == TokenKind::Question) {
      // menempel, tanpa spasi
    } else if (next == TokenKind::Gt && genericBrackets.count(i + 1)) {
      // menempel ke `>` generik (`T>`)
    } else if (!isCloser(next) && next != TokenKind::Eof) {
      out += " ";
    }
  }
  // Komentar sisa di akhir file.
  flushCommentsBefore(1 << 30);

  // Rapikan: spasi ganda → tunggal (kecuali indentasi awal baris),
  // spasi ekor, baris kosong ganda. Sadar-string (Phase 125): isi
  // literal "..." disalin verbatim — collapse global pernah merusak
  // `"  hi"` menjadi `" hi"` (konten program, bukan gaya).
  // Aman: string Nusa satu-baris (escape 2-char), jadi `\n` selalu
  // di luar string; strip ekor tak pernah menyentuh isi string
  // (baris invalid unterminated ditolak lexer sebelum fmt).
  std::string clean;
  clean.reserve(out.size());
  bool atLineStart = true;
  bool inStr = false;
  size_t col = 0;
  for (size_t i = 0; i < out.size(); i++) {
    char c = out[i];
    if (inStr) {
      clean.push_back(c);
      col++;
      if (c == '\\' && i + 1 < out.size()) {
        clean.push_back(out[++i]);
        col++;
      } else if (c == '"') {
        inStr = false;
      }
      continue;
    }
    if (c == '"' || (c == '/' && i + 1 < out.size() && out[i + 1] == '/')) {
      // Literal string ATAU komentar `//`: salin verbatim (isi
      // komentar = dokumentasi; collapse merusak `///` tersejajarkan
      // dan mengubah keluaran `nusa doc`).
      if (c == '"')
        inStr = true;
      atLineStart = false;
      if (c == '/') {
        while (i < out.size() && out[i] != '\n') {
          clean.push_back(out[i]);
          i++;
          col++;
        }
        if (i < out.size())
          i--; // biar i++ for mendarat di '\n' (diproses normal)
        continue;
      }
      clean.push_back(c);
      col++;
      continue;
    }
    if (c == '\n') {
      // Buang spasi ekor.
      while (!clean.empty() && clean.back() == ' ')
        clean.pop_back();
      clean.push_back('\n');
      atLineStart = true;
      col = 0;
      continue;
    }
    if (c == ' ' && atLineStart) {
      clean.push_back(c); // indentasi
      col++;
      continue;
    }
    atLineStart = false;
    if (c == ' ' && col > 0 && !clean.empty() && clean.back() == ' ')
      continue; // spasi ganda
    clean.push_back(c);
    col++;
  }
  // Gabung baris kosong beruntun (maks 1 baris kosong).
  std::string final;
  final.reserve(clean.size());
  int blankRun = 0;
  bool freshLine = true;
  for (char c : clean) {
    if (c == '\n') {
      if (freshLine) {
        blankRun++;
        if (blankRun <= 1)
          final.push_back(c);
      } else {
        final.push_back(c);
        blankRun = 0;
      }
      freshLine = true;
    } else {
      final.push_back(c);
      freshLine = false;
    }
  }
  // Tepat satu newline di akhir.
  while (!final.empty() && final.back() == '\n')
    final.pop_back();
  final.push_back('\n');

  FormatResult r;
  r.text = final;
  return r;
}

} // namespace nusa
