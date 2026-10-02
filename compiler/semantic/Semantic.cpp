#include "compiler/semantic/Semantic.h"
#include "compiler/frontend/Builtin.h" // isBuiltin (Phase 51: Frontend layer)

namespace nusa {

bool SymbolTable::declare(const std::string &name, Location loc,
                          std::string &errHint) {
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    const Location &prev = it->second.declLoc;
    errHint = "previous declaration of '" + name + "' was at " +
              prev.file + ":" + std::to_string(prev.line) + ":" +
              std::to_string(prev.column);
    return false;
  }
  symbols_.emplace(name, Symbol{name, loc});
  return true;
}

std::optional<Symbol> SymbolTable::lookup(const std::string &name) const {
  auto it = symbols_.find(name);
  if (it == symbols_.end())
    return std::nullopt;
  return it->second;
}

void SymbolTable::clear() { symbols_.clear(); }

std::vector<SemanticError> SemanticAnalyzer::analyze(const Program &prog) {
  errors_.clear();
  functions_.clear();
  structs_.clear();
  structDecls_.clear();
  aliasNames_.clear();
  enumNames_.clear();
  enumDecls_.clear();
  constNames_.clear();
  constFiles_.clear();
  variantToEnum_.clear();
  traits_.clear();
  values_.clear();
  // Pass 0: daftarkan semua nama trait (bound `T: Trait` di bawah
  // butuh tabel lengkap; maju/mundur OK).
  for (const auto &td : prog.traits) {
    if (isReservedFunction(td.name)) {
      errors_.push_back(SemanticError{
          td.loc, "cannot declare trait '" + td.name + "'",
          "'" + td.name + "' is reserved by the language/runtime"});
      continue;
    }
    std::string hint;
    if (!traits_.declare(td.name, td.loc, hint)) {
      errors_.push_back(SemanticError{
          td.loc, "duplicate trait '" + td.name + "'", hint});
      continue;
    }
    // Duplikat metode dalam trait.
    std::unordered_set<std::string> seenMethods;
    for (const auto &m : td.methods) {
      if (m.name == "main") {
        errors_.push_back(SemanticError{
            m.loc, "'fn main' cannot be a trait method",
            "methods belong to a type, not the entry point"});
      }
      if (!seenMethods.insert(m.name).second) {
        errors_.push_back(SemanticError{
            m.loc, "duplicate method '" + m.name + "' in trait '" +
                       td.name + "'",
            "use each method name once"});
      }
    }
  }
  // Pass 1: daftarkan semua nama fungsi (dukung panggilan maju/mundur).
  for (const auto &fn : prog.functions) {
    if (isReservedFunction(fn.name)) {
      errors_.push_back(SemanticError{
          fn.loc, "cannot declare function '" + fn.name + "'",
          "'" + fn.name + "' is reserved by the language/runtime"});
      continue;
    }
    std::string hint;
    if (!functions_.declare(fn.name, fn.loc, hint)) {
      errors_.push_back(SemanticError{
          fn.loc, "duplicate function '" + fn.name + "'", hint});
    }
    // Extern C (Phase 32): tanpa tparams, bukan main (entry milik Nusa).
    if (fn.isExtern) {
      if (!fn.tparams.empty()) {
        errors_.push_back(SemanticError{
            fn.loc, "extern function '" + fn.name + "' cannot be generic",
            "generic instantiation needs a Nusa body to specialize"});
      }
      if (fn.name == "main") {
        errors_.push_back(SemanticError{
            fn.loc, "cannot declare 'main' as extern",
            "'main' is the program entry point, defined in Nusa"});
      }
    }
    // Duplikat parameter tipe (Phase 21c).
    std::unordered_set<std::string> seenTp;
    for (const auto &tp : fn.tparams) {
      if (!seenTp.insert(tp).second) {
        errors_.push_back(SemanticError{
            fn.loc, "duplicate type parameter '" + tp + "' in '" + fn.name +
                        "'",
            "use each type parameter name once"});
      }
    }
    // Batasan trait (Phase 22): nama trait harus dikenal.
    for (const auto &tp : fn.tparams) {
      auto bit = fn.tparamBounds.find(tp);
      if (bit == fn.tparamBounds.end())
        continue;
      for (const auto &b : bit->second) {
        if (!traits_.lookup(b)) {
          errors_.push_back(SemanticError{
              fn.loc, "unknown trait '" + b + "' in bound for '" + tp +
                          "' in '" + fn.name + "'",
              "declare 'trait " + b + " { ... }' first"});
        }
      }
    }
  }
  if (!prog.hasMain()) {
    Location loc{prog.file, 1, 1};
    errors_.push_back(SemanticError{
        loc, "no 'fn main' found",
        "every program needs an entry point: fn main() { ... }"});
  }
  // Pass 1b: daftarkan semua nama struct (konstruksi maju/mundur OK).
  // Namespace terpisah dari fungsi (ala Rust): `struct Foo` + `fn Foo`
  // boleh koeksis; pemakaian dibedakan posisi sintaksis (`{` vs `(`).
  for (const auto &sd : prog.structs) {
    if (isReservedFunction(sd.name)) {
      errors_.push_back(SemanticError{
          sd.loc, "cannot declare struct '" + sd.name + "'",
          "'" + sd.name + "' is reserved by the language/runtime"});
      continue;
    }
    std::string hint;
    if (!structs_.declare(sd.name, sd.loc, hint)) {
      errors_.push_back(SemanticError{
          sd.loc, "duplicate struct '" + sd.name + "'", hint});
    } else {
      structDecls_[sd.name] = &sd;
    }
    // Duplikat parameter tipe (Phase 21d; cermin fungsi generik).
    std::unordered_set<std::string> seenTp;
    for (const auto &tp : sd.tparams) {
      if (!seenTp.insert(tp).second) {
        errors_.push_back(SemanticError{
            sd.loc, "duplicate type parameter '" + tp + "' in '" + sd.name +
                        "'",
            "use each type parameter name once"});
      }
    }
    // Batasan trait (Phase 22; cermin fungsi).
    for (const auto &tp : sd.tparams) {
      auto bit = sd.tparamBounds.find(tp);
      if (bit == sd.tparamBounds.end())
        continue;
      for (const auto &b : bit->second) {
        if (!traits_.lookup(b)) {
          errors_.push_back(SemanticError{
              sd.loc, "unknown trait '" + b + "' in bound for '" + tp +
                          "' in '" + sd.name + "'",
              "declare 'trait " + b + " { ... }' first"});
        }
      }
    }
  }
  // Pass 1c: daftarkan nama alias tipe (Phase 21a) — namespace tipe yang
  // sama dengan struct. Target alias TIDAK di-resolve di sini
  // (ranah typechecker).
  for (const auto &ad : prog.aliases) {
    if (isReservedFunction(ad.name)) {
      errors_.push_back(SemanticError{
          ad.loc, "cannot declare type alias '" + ad.name + "'",
          "'" + ad.name + "' is reserved by the language/runtime"});
      continue;
    }
    if (structs_.lookup(ad.name)) {
      std::string what = aliasNames_.count(ad.name)
                             ? "a type alias"
                             : "a struct";
      errors_.push_back(SemanticError{
          ad.loc, "duplicate type '" + ad.name + "'",
          "'" + ad.name + "' is already declared as " + what});
      continue;
    }
    std::string hint;
    structs_.declare(ad.name, ad.loc, hint);
    aliasNames_.insert(ad.name);
  }
  // Pass 1e: daftarkan nama enum (Phase 26) — namespace tipe yang sama
  // dengan struct/alias (ala Rust: satu namespace tipe). Payload TIDAK
  // di-resolve di sini (ranah typechecker, cermin field struct).
  for (const auto &ed : prog.enums) {
    if (isReservedFunction(ed.name)) {
      errors_.push_back(SemanticError{
          ed.loc, "cannot declare enum '" + ed.name + "'",
          "'" + ed.name + "' is reserved by the language/runtime"});
      continue;
    }
    if (structs_.lookup(ed.name)) {
      std::string what = enumNames_.count(ed.name)
                             ? "an enum"
                             : (aliasNames_.count(ed.name) ? "a type alias"
                                                           : "a struct");
      errors_.push_back(SemanticError{
          ed.loc, "duplicate type '" + ed.name + "'",
          "'" + ed.name + "' is already declared as " + what});
      continue;
    }
    std::string hint;
    structs_.declare(ed.name, ed.loc, hint);
    enumNames_.insert(ed.name);
    enumDecls_[ed.name] = &ed;
    // Duplikat parameter tipe (cermin struct generik).
    std::unordered_set<std::string> seenTp;
    for (const auto &tp : ed.tparams) {
      if (!seenTp.insert(tp).second) {
        errors_.push_back(SemanticError{
            ed.loc, "duplicate type parameter '" + tp + "' in '" + ed.name +
                        "'",
            "use each type parameter name once"});
      }
    }
    // Batasan trait (cermin struct).
    for (const auto &tp : ed.tparams) {
      auto bit = ed.tparamBounds.find(tp);
      if (bit == ed.tparamBounds.end())
        continue;
      for (const auto &b : bit->second) {
        if (!traits_.lookup(b)) {
          errors_.push_back(SemanticError{
              ed.loc, "unknown trait '" + b + "' in bound for '" + tp +
                          "' in '" + ed.name + "'",
              "declare 'trait " + b + " { ... }' first"});
        }
      }
    }
    // Duplikat varian se-enum.
    std::unordered_set<std::string> seenV;
    for (const auto &v : ed.variants) {
      if (!seenV.insert(v.name).second) {
        errors_.push_back(SemanticError{
            v.loc, "duplicate variant '" + v.name + "' in enum '" +
                       ed.name + "'",
            "use each variant name once per enum"});
      } else if (!variantToEnum_.count(v.name)) {
        variantToEnum_[v.name] = ed.name;
      }
    }
  }
  // Pass 1d: validasi blok impl (Phase 22) — setelah semua nama
  // (trait/struct/fungsi) terdaftar agar forward reference OK.
  for (const auto &ib : prog.impls) {
    if (!traits_.lookup(ib.trait)) {
      errors_.push_back(SemanticError{
          ib.traitLoc, "unknown trait '" + ib.trait + "'",
          "declare 'trait " + ib.trait + " { ... }' first"});
    }
    std::unordered_set<std::string> seenMethods;
    for (const auto &m : ib.methods) {
      if (m.name == "main") {
        errors_.push_back(SemanticError{
            m.loc, "'fn main' cannot be a trait method",
            "methods belong to a type, not the entry point"});
      }
      if (!seenMethods.insert(m.name).second) {
        errors_.push_back(SemanticError{
            m.loc, "duplicate method '" + m.name + "' in impl of '" +
                       ib.trait + "'",
            "use each method name once per impl block"});
      }
    }
  }
  // Pass 1f: daftarkan nama konstanta (Phase 70) — namespace nilai
  // bersama fungsi (tabrakan = error, cegah ambiguitas VarRef).
  for (const auto &cd : prog.consts) {
    if (isReservedFunction(cd.name)) {
      errors_.push_back(SemanticError{
          cd.loc, "cannot declare constant '" + cd.name + "'",
          "'" + cd.name + "' is reserved by the language/runtime"});
      continue;
    }
    if (functions_.lookup(cd.name)) {
      errors_.push_back(SemanticError{
          cd.loc, "duplicate definition '" + cd.name + "'",
          "'" + cd.name + "' is already declared as a function"});
      continue;
    }
    if (!constNames_.emplace(cd.name, cd.loc).second) {
      errors_.push_back(SemanticError{
          cd.loc, "duplicate constant '" + cd.name + "'",
          "use each constant name once"});
      continue;
    }
    constFiles_[cd.name] = cd.loc.file;
  }
  // Pass 2: periksa setiap body dengan scope fungsi baru
  // (extern dilewati: tanpa body).
  for (const auto &fn : prog.functions) {
    if (fn.isExtern)
      continue;
    checkFunction(fn);
  }
  // Body metode impl (Phase 22): dicek seperti fungsi biasa (`self`
  // hanyalah parameter; tipenya ranah typechecker).
  for (const auto &ib : prog.impls) {
    for (const auto &m : ib.methods)
      checkFunction(m);
  }
  return errors_;
}

bool SemanticAnalyzer::isReservedFunction(const std::string &name) const {
  return name == "print" || name == "puts" || name == "printf" ||
         name == "exit" || // runtime bounds-check (v0.7.0)
         isBuiltin(name);  // len + builtins v0.9.0
}

void SemanticAnalyzer::checkFunction(const FunctionDecl &fn) {
  values_.clear();
  values_.push(); // scope fungsi
  // Parameter menghuni scope fungsi.
  for (const auto &param : fn.params) {
    if (!values_.insert(param.name, Symbol{param.name, param.loc})) {
      auto prev = values_.lookup(param.name);
      std::string hint;
      if (prev) {
        const Location &pl = prev->declLoc;
        hint = "previous declaration of '" + param.name + "' was at " +
               pl.file + ":" + std::to_string(pl.line) + ":" +
               std::to_string(pl.column);
      }
      errors_.push_back(SemanticError{
          param.loc, "duplicate parameter '" + param.name + "'", hint});
    }
  }
  checkBlock(fn.body);
  values_.pop();
}

// Kumpulkan nama binding pola (Phase 25) untuk deklarasi scope arm.
static void collectPatBinds(const Pat &pat,
                            std::vector<std::pair<std::string, Location>> &out) {
  if (const auto *n = std::get_if<Pat::Bind>(&pat.node)) {
    out.emplace_back(n->name, n->loc);
  } else if (const auto *n = std::get_if<Pat::Struct>(&pat.node)) {
    for (const auto &f : n->fields)
      collectPatBinds(*f.pat, out);
  } else if (const auto *n = std::get_if<Pat::Variant>(&pat.node)) {
    for (const auto &a : n->args)
      collectPatBinds(*a, out);
  }
}

void SemanticAnalyzer::checkBlock(const Block &block) {
  values_.push(); // tiap Block = scope baru (shadowing diizinkan)
  for (const auto &stmt : block.stmts) {
    if (std::holds_alternative<LetStmt>(stmt)) {
      const auto &let = std::get<LetStmt>(stmt);
      // Inisialisasi diperiksa DULU: `let x = x;` → x belum terdefinisi.
      checkExpr(*let.value);
      if (!values_.insert(let.name, Symbol{let.name, let.loc})) {
        // Bentrok di scope ini (shadowing antar-scope diizinkan, jadi
        // yang ditemukan lookup pasti entri se-scope yang bentrok).
        std::string hint;
        if (auto prev = values_.lookup(let.name)) {
          const Location &pl = prev->declLoc;
          hint = "previous declaration of '" + let.name + "' was at " +
                 pl.file + ":" + std::to_string(pl.line) + ":" +
                 std::to_string(pl.column);
        }
        errors_.push_back(SemanticError{
            let.loc, "duplicate variable '" + let.name + "'", hint});
      }
    } else if (std::holds_alternative<PrintStmt>(stmt)) {
      checkExpr(*std::get<PrintStmt>(stmt).arg);
    } else if (std::holds_alternative<ReturnStmt>(stmt)) {
      const auto &ret = std::get<ReturnStmt>(stmt);
      if (ret.value)
        checkExpr(*ret.value);
    } else if (std::holds_alternative<ExprStmt>(stmt)) {
      checkExpr(*std::get<ExprStmt>(stmt).expr);
    } else if (std::holds_alternative<IfStmt>(stmt)) {
      const auto &is = std::get<IfStmt>(stmt);
      checkExpr(*is.cond);
      checkBlock(*is.thenBlock);
      if (is.elseBlock)
        checkBlock(*is.elseBlock);
    } else if (std::holds_alternative<WhileStmt>(stmt)) {
      const auto &ws = std::get<WhileStmt>(stmt);
      checkExpr(*ws.cond);
      checkBlock(*ws.body);
    } else if (std::holds_alternative<MatchStmt>(stmt)) {
      // Match (Phase 25): subject + tiap arm di scope sendiri dengan
      // binding pola dideklarasikan (tipe tak perlu di level ini).
      const auto &ms = std::get<MatchStmt>(stmt);
      checkExpr(*ms.subject);
      for (const auto &arm : ms.arms) {
        values_.push();
        std::vector<std::pair<std::string, Location>> binds;
        collectPatBinds(arm.pat, binds);
        for (const auto &bn : binds) {
          if (!values_.insert(bn.first, Symbol{bn.first, bn.second})) {
            errors_.push_back(SemanticError{
                bn.second, "duplicate binding '" + bn.first + "' in pattern",
                "use each bound name once per pattern"});
          }
        }
        // Guard boleh memakai binding pola (`n if n > 0`).
        if (arm.guard)
          checkExpr(*arm.guard);
        checkBlock(*arm.body);
        values_.pop();
      }
    } else {
      // Assignment: menimpa variabel yang HARUS sudah ada (tak mendeclare).
      const auto &as = std::get<AssignStmt>(stmt);
      checkExpr(*as.value);
      if (constNames_.count(as.name)) {
        errors_.push_back(SemanticError{
            as.loc, "cannot assign to constant '" + as.name + "'",
            "constants are immutable; use 'let' for a mutable binding"});
      } else if (!values_.lookup(as.name)) {
        errors_.push_back(SemanticError{
            as.loc, "undefined variable '" + as.name + "'",
            "declare '" + as.name + "' with 'let' before assigning it"});
      }
    }
  }
  values_.pop();
}

void SemanticAnalyzer::checkExpr(const Expr &expr) {
  if (++exprDepth_ > kMaxExprDepth) {
    --exprDepth_;
    errors_.push_back(SemanticError{
        expr.loc(), "expression too deeply nested",
        "simplify this nested expression"});
    return;
  }
  struct Visitor {
    SemanticAnalyzer *self;
    void operator()(const Expr::LiteralInt &) const {}
    void operator()(const Expr::LiteralBool &) const {}
    void operator()(const Expr::LiteralString &) const {}
    void operator()(const Expr::VarRef &n) const {
      if (self->values_.lookup(n.name))
        return; // variabel lokal/parameter
      // Konstanta top-level (Phase 70): nilai disubstitusi typechecker.
      auto cit = self->constNames_.find(n.name);
      if (cit != self->constNames_.end()) {
        if (!n.name.empty() && n.name[0] == '_') {
          auto fit = self->constFiles_.find(n.name);
          if (fit != self->constFiles_.end() && fit->second != n.loc.file) {
            self->errors_.push_back(SemanticError{
                n.loc, "'" + n.name + "' is private to module '" +
                           fit->second + "'",
                "names starting with '_' are module-private",
                static_cast<int>(n.name.size())});
          }
        }
        return;
      }
      // Nama fungsi sebagai value (Phase 21b): function reference.
      if (self->functions_.lookup(n.name)) {
        if (n.name == "main") {
          self->errors_.push_back(SemanticError{
              n.loc, "cannot use 'main' as a value",
              "'main' is the program entry point, not a function value",
              static_cast<int>(n.name.size())});
          return;
        }
        if (!n.name.empty() && n.name[0] == '_') {
          // Privacy modul (v0.8.0): konsisten dengan panggilan.
          auto sym = self->functions_.lookup(n.name);
          if (sym && sym->declLoc.file != n.loc.file) {
            self->errors_.push_back(SemanticError{
                n.loc, "'" + n.name + "' is private to module '" +
                           sym->declLoc.file + "'",
                "names starting with '_' are module-private",
                static_cast<int>(n.name.size())});
          }
        }
        return;
      }
      if (isBuiltin(n.name)) {
        self->errors_.push_back(SemanticError{
            n.loc, "cannot use builtin '" + n.name + "' as a value",
            "builtins are only callable, e.g. " + n.name + "(...)",
            static_cast<int>(n.name.size())});
        return;
      }
      if (n.name == "print") {
        self->errors_.push_back(SemanticError{
            n.loc, "cannot use 'print' as a value",
            "'print' is a statement: print(<expr>)",
            static_cast<int>(n.name.size())});
        return;
      }
      // Nama enum bukan nilai (Phase 26): konstruksi via varian.
      if (self->enumNames_.count(n.name)) {
        self->errors_.push_back(SemanticError{
            n.loc, "cannot use enum '" + n.name + "' as a value",
            "construct a variant: " + n.name + ".Variant(...)",
            static_cast<int>(n.name.size())});
        return;
      }
      self->errors_.push_back(SemanticError{
          n.loc, "undefined variable '" + n.name + "'",
          "declare '" + n.name + "' before using it",
          static_cast<int>(n.name.size())});
    }
    // Panggilan method (Phase 22): receiver + argumen diperiksa;
    // resolusi metode (butuh tipe) ranah typechecker.
    // Konstruktor enum `E.V(...)` (Phase 26): receiver nama-tipe,
    // bukan nilai — argumen saja yang diperiksa di sini.
    void operator()(const Expr::MethodCall &n) const {
      bool enumCtor = false;
      if (const auto *vr = std::get_if<Expr::VarRef>(&n.receiver->node))
        enumCtor = self->enumNames_.count(vr->name) > 0;
      if (!enumCtor)
        self->checkExpr(*n.receiver);
      for (const auto &a : n.args)
        self->checkExpr(*a);
    }
    // Propagasi `?` (Phase 24): operan diperiksa; validasi tipe
    // ranah typechecker.
    void operator()(const Expr::Propagate &n) const {
      self->checkExpr(*n.expr);
    }
    // Lambda `|x| ...` (Phase 27): parameter menghuni scope lambda;
    // body dicek seperti blok fungsi (capture = variabel enclosing,
    // resolved via scope luar; validasi penuh ranah typechecker).
    void operator()(const Expr::Lambda &n) const {
      self->values_.push();
      for (const auto &p : n.params) {
        if (!self->values_.insert(p.name, Symbol{p.name, p.loc})) {
          self->errors_.push_back(SemanticError{
              p.loc, "duplicate parameter '" + p.name + "' in lambda",
              "use each parameter name once"});
        }
      }
      self->checkBlock(*n.body);
      self->values_.pop();
    }
    void operator()(const Expr::Closure &n) const {
      // Tak terjangkau pre-checker (dibuat checker); pertahanan:
      // capture tetap diperiksa.
      for (const auto &c : n.captures)
        self->checkExpr(*c);
    }
    void operator()(const Expr::Call &n) const {      if (isBuiltin(n.callee)) {
        // Builtin: bukan fungsi user; argumen tetap diperiksa,
        // validasi penuh (arity, tipe) di typechecker.
      } else if (self->values_.lookup(n.callee)) {
        // Variabel lokal/parameter (Phase 21b): validasi bahwa nilainya
        // bertipe fungsi + arity di typechecker (tahu tipenya).
      } else if (n.callee == "main") {
        self->errors_.push_back(SemanticError{
            n.loc, "cannot call 'main'",
            "'main' is the program entry point, not a callable function",
            static_cast<int>(n.callee.size())});
      } else if (n.callee == "print") {
        self->errors_.push_back(SemanticError{
            n.loc, "cannot call 'print' as a function",
            "'print' is a statement: print(<expr>)",
            static_cast<int>(n.callee.size())});
      } else if (!self->functions_.lookup(n.callee)) {
        // Varian enum tak-qualified (Phase 26): arahkan ke bentuk penuh.
        auto vit = self->variantToEnum_.find(n.callee);
        if (vit != self->variantToEnum_.end()) {
          self->errors_.push_back(SemanticError{
              n.loc, "undefined function '" + n.callee + "'",
              "'" + n.callee + "' is a variant of enum '" + vit->second +
                  "'; construct it qualified: " + vit->second + "." +
                  n.callee + "(...)",
              static_cast<int>(n.callee.size())});
        } else {
          self->errors_.push_back(SemanticError{
              n.loc, "undefined function '" + n.callee + "'",
              "declare 'fn " + n.callee + "(...)' before calling it",
              static_cast<int>(n.callee.size())});
        }
      } else if (!n.callee.empty() && n.callee[0] == '_') {
        // Privacy modul (v0.8.0): `_`-function antar-file ditolak.
        auto sym = self->functions_.lookup(n.callee);
        if (sym && sym->declLoc.file != n.loc.file) {
          self->errors_.push_back(SemanticError{
              n.loc, "'" + n.callee + "' is private to module '" +
                         sym->declLoc.file + "'",
              "names starting with '_' are module-private",
              static_cast<int>(n.callee.size())});
        }
      }
      for (const auto &a : n.args)
        self->checkExpr(*a);
    }
    void operator()(const Expr::ArrayLit &n) const {
      for (const auto &e : n.elems)
        self->checkExpr(*e);
    }
    void operator()(const Expr::Index &n) const {
      self->checkExpr(*n.base);
      self->checkExpr(*n.index);
    }
    void operator()(const Expr::StructLit &n) const {
      auto declIt = self->structDecls_.find(n.name);
      if (declIt == self->structDecls_.end()) {
        // Enum bukan struct (Phase 26): konstruksi via varian.
        if (self->enumNames_.count(n.name)) {
          self->errors_.push_back(SemanticError{
              n.loc, "cannot construct enum '" + n.name + "' as struct",
              "construct a variant: " + n.name + ".Variant(...)",
              static_cast<int>(n.name.size())});
          for (const auto &f : n.fields)
            self->checkExpr(*f.value);
          return;
        }
        if (self->aliasNames_.count(n.name)) {
          self->errors_.push_back(SemanticError{
              n.loc, "cannot construct type alias '" + n.name + "'",
              "construct the underlying struct type instead",
              static_cast<int>(n.name.size())});
          for (const auto &f : n.fields)
            self->checkExpr(*f.value);
          return;
        }
        self->errors_.push_back(SemanticError{
            n.loc, "undefined struct '" + n.name + "'",
            "declare 'struct " + n.name + " { ... }' before constructing it",
            static_cast<int>(n.name.size())});
        for (const auto &f : n.fields)
          self->checkExpr(*f.value);
        return;
      }
      const StructDecl *decl = declIt->second;
      // Privacy modul (v0.8.0): `_`-struct antar-file ditolak.
      auto symIt = self->structs_.lookup(n.name);
      if (symIt && !n.name.empty() && n.name[0] == '_' &&
          symIt->declLoc.file != n.loc.file) {
        self->errors_.push_back(SemanticError{
            n.loc, "'" + n.name + "' is private to module '" +
                       symIt->declLoc.file + "'",
            "names starting with '_' are module-private",
            static_cast<int>(n.name.size())});
      }
      std::unordered_map<std::string, Location> seen;
      for (const auto &f : n.fields) {
        self->checkExpr(*f.value);
        bool known = false;
        for (const auto &df : decl->fields) {
          if (df.name == f.name) {
            known = true;
            break;
          }
        }
        if (!known) {
          std::string fields;
          for (size_t i = 0; i < decl->fields.size(); i++) {
            if (i)
              fields += ", ";
            fields += decl->fields[i].name;
          }
          self->errors_.push_back(SemanticError{
              f.loc, "unknown field '" + f.name + "' for struct '" + n.name +
                         "'",
              "struct '" + n.name + "' has fields: " + fields,
              static_cast<int>(f.name.size())});
        } else if (!seen.emplace(f.name, f.loc).second) {
          self->errors_.push_back(SemanticError{
              f.loc, "duplicate field '" + f.name + "' for struct '" +
                         n.name + "'",
              "", static_cast<int>(f.name.size())});
        }
      }
      for (const auto &df : decl->fields) {
        bool present = false;
        for (const auto &f : n.fields) {
          if (f.name == df.name) {
            present = true;
            break;
          }
        }
        if (!present) {
          self->errors_.push_back(SemanticError{
              n.loc, "missing field '" + df.name + "' for struct '" +
                         n.name + "'",
              "construction must provide every field",
              static_cast<int>(n.name.size())});
        }
      }
    }
    void operator()(const Expr::FieldAccess &n) const {
      // Keberadaan field butuh tipe base → dicek typechecker.
      // Varian unit `E.V` (Phase 26): base nama-tipe, bukan nilai.
      bool enumCtor = false;
      if (const auto *vr = std::get_if<Expr::VarRef>(&n.base->node))
        enumCtor = self->enumNames_.count(vr->name) > 0;
      if (!enumCtor)
        self->checkExpr(*n.base);
    }
    void operator()(const Expr::Unary &n) const {
      self->checkExpr(*n.operand);
    }
    void operator()(const Expr::Binary &n) const {
      self->checkExpr(*n.lhs);
      self->checkExpr(*n.rhs);
    }
  };
  std::visit(Visitor{this}, expr.node);
  --exprDepth_;
}

} // namespace nusa
