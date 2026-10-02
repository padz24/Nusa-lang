#pragma once
#include "compiler/ast/AST.h"
#include "compiler/semantic/Scopes.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nusa {

// Semantic analysis v0.6.0: namespaces + lexical scopes.
//
//  - Namespace fungsi (top-level, flat): nama → deklarasi. Duplikat →
//    error. `main` wajib ada tepat satu. Reserved: `print` (statement),
//    `len` (builtin), `puts`/`printf`/`exit` (runtime — cegah tabrakan
//    simbol LLVM).
//  - Namespace nilai: tumpukan scope leksikal (v0.6.0). Tiap Block
//    mendorong scope baru; shadowing antar-scope diizinkan, duplikat
//    se-scope ditolak. Parameter menghuni scope fungsi.
//  - Variabel: dideklarasikan sebelum dipakai (innermost-out).
//  - Panggilan: callee harus fungsi yang dikenal (`main` tak callable).
//  - Tipe (kondisi bool, arity, return) = TypeChecker.

struct Symbol {
  std::string name;
  Location declLoc;
};

class SymbolTable {
public:
  // false + errHint jika nama sudah dideklarasikan.
  bool declare(const std::string &name, Location loc, std::string &errHint);
  std::optional<Symbol> lookup(const std::string &name) const;
  void clear();

private:
  std::unordered_map<std::string, Symbol> symbols_;
};

struct SemanticError {
  Location loc;
  std::string message;
  std::string hint;
  // Panjang underline (caret beruntun) dari loc.column; default 1.
  // Diisi dengan panjang identifier bila loc tepat di identifier
  // (mis. variabel/fungsi/field) — meniru contoh diagnostics master.
  int length = 1;
};

class SemanticAnalyzer {
public:
  std::vector<SemanticError> analyze(const Program &prog);

private:
  void checkFunction(const FunctionDecl &fn);
  void checkBlock(const Block &block);
  void checkExpr(const Expr &expr);
  bool isReservedFunction(const std::string &name) const;

  SymbolTable functions_; // namespace fungsi (top-level, flat)
  SymbolTable structs_;   // namespace struct (top-level, flat, v0.8.0)
  SymbolTable traits_;    // namespace trait (top-level, flat, Phase 22)
  Scopes<Symbol> values_; // namespace nilai (tumpukan scope)
  // Definisi struct per nama (untuk validasi field literal).
  std::unordered_map<std::string, const StructDecl *> structDecls_;
  // Nama alias tipe (Phase 21a): menghuni namespace tipe bersama struct.
  std::unordered_set<std::string> aliasNames_;
  // Nama enum (Phase 26): namespace tipe yang sama dengan struct/alias.
  std::unordered_set<std::string> enumNames_;
  std::unordered_map<std::string, const EnumDecl *> enumDecls_;
  // Varian → enum pemilik (hint ctor tak-qualified di Call).
  std::unordered_map<std::string, std::string> variantToEnum_;
  // Nama konstanta (Phase 70): nilai top-level; namespace nilai
  // bersama fungsi (tabrakan = error, cegah ambiguitas VarRef).
  // Nilai = file deklarasi (privacy `_` cermin fungsi).
  std::unordered_map<std::string, Location> constNames_;
  std::unordered_map<std::string, std::string> constFiles_;
  std::vector<SemanticError> errors_;
  // Batas rekursi checkExpr (v0.9.5): rantai biner dibangun iteratif oleh
  // parser sehingga lolos guard parser — walker ini yang membatasi.
  // Body checkExpr straight-line (aman inc/dec manual).
  static constexpr int kMaxExprDepth = 500;
  int exprDepth_ = 0;
};

} // namespace nusa
