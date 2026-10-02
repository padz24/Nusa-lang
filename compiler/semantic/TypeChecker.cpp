#include "compiler/semantic/TypeChecker.h"
#include "compiler/frontend/Builtin.h" // isBuiltin (Phase 51)
#include <algorithm>

namespace nusa {

bool TypeChecker::check(Program &prog) {
  errors_.clear();
  env_.clear();
  sigs_.clear();
  structs_.clear();
  structTemplates_.clear();
  structBad_.clear();
  structInstDecls_.clear();
  structInstNames_.clear();
  structBadInst_.clear();
  instantiating_.clear();
  aliases_.clear();
  aliasBad_.clear();
  resolving_.clear();
  genericSigs_.clear();
  instances_.clear();
  instantiated_.clear();
  instDepth_ = 0;
  traitSigs_.clear();
  methodImpls_.clear();
  typeMethods_.clear();
  typeTraits_.clear();
  pendingMethods_.clear();
  matchTmp_ = 0;
  enums_.clear();
  externFns_.clear();
  lambdaFuncs_.clear();
  lambdaStructs_.clear();
  lambdaSeq_ = 0;
  collectLambdaRet_ = false;
  lambdaRetActuals_.clear();
  structNames_.clear();
  funcNames_.clear();
  mainFuncType_ = Type::func({}, Type::void_());
  // Pass 0: target alias mentah (Phase 21a) — sebelum struct agar field
  // struct boleh memakai alias, dan sebelum signature agar param/return
  // boleh memakai alias. Forward reference OK (resolve lazy).
  for (const auto &ad : prog.aliases) {
    if (aliases_.count(ad.name)) {
      errorAt(ad.loc, "duplicate type alias '" + ad.name + "'",
              "previous alias '" + ad.name + "' is kept");
      continue;
    }
    aliases_[ad.name] = ad.target;
  }
  // Alias vs struct (pertahanan bila semantic dilewati — pemakaian normal
  // sudah ditolak di sana): nama yang juga dipakai struct = duplikat.
  // (Dicek ke prog.structs agar struct kosong valid pun terdeteksi.)
  for (const auto &ad : prog.aliases) {
    bool isStruct = false;
    for (const auto &sd : prog.structs)
      if (sd.name == ad.name) {
        isStruct = true;
        break;
      }
    if (isStruct && aliases_.count(ad.name)) {
      errorAt(ad.loc, "duplicate type '" + ad.name + "'",
              "'" + ad.name + "' is already declared as a struct");
      aliases_.erase(ad.name);
    }
  }
  // Pass 1a0: enum → hidden struct (Phase 26). Disisipkan di AWAL
  // prog.structs sebelum pass 1a agar field struct bisa memakai enum
  // (generik) tanpa peduli urutan deklarasi; seluruh mesin struct
  // (template, layout, trait, print) berlaku. Template di-erase di
  // akhir seperti struct generik biasa (codegen membaca instansiasi).
  {
    std::vector<StructDecl> hidden;
    for (const auto &ed : prog.enums) {
      StructDecl hd;
      if (processEnumDecl(ed, prog, hd))
        hidden.push_back(std::move(hd));
    }
    for (auto it = hidden.rbegin(); it != hidden.rend(); ++it)
      prog.structs.insert(prog.structs.begin(), std::move(*it));
  }
  // Pass 1a: definisi struct (field bisa forward-reference antar-struct
  // karena layout di-resolve per nama, bukan posisi).
  for (auto &sd : prog.structs) {
    structNames_.insert(sd.name);
    if (!sd.tparams.empty()) {
      // Template struct generik (Phase 21d): daftarkan + konversi field
      // (tparam-named → Param); layout dibangun per-instance.
      registerStructTemplate(sd);
      continue;
    }
    StructInfo info;
    bool ok = true;
    for (auto &f : sd.fields) {
      for (const auto &pr : info.fields) {
        if (pr.first == f.name) {
          errorAt(f.loc, "duplicate field '" + f.name + "' in struct '" +
                             sd.name + "'");
          ok = false;
        }
      }
      Type ft = resolveType(f.type, f.loc);
      if (ft.isError()) {
        ok = false;
        continue;
      }
      f.type = mapAppToMangled(ft); // kanonikalisasi in-place (lihat check() di header)
      if (ft.kind == TypeKind::Void) {
        errorAt(f.loc, "struct field '" + f.name + "' cannot have type 'void'");
        ok = false;
        continue;
      }
      if (ft.kind == TypeKind::Array) {
        // Ukuran array bukan bagian tipe → layout struct tak bisa statis.
        errorAt(f.loc, "array fields are not supported",
                "store the array in a local variable, not in a struct");
        ok = false;
        continue;
      }
      if (ft.kind == TypeKind::Func) {
        errorAt(f.loc, "function fields are not supported",
                "store the function in a local variable, not in a struct");
        ok = false;
        continue;
      }
      info.fields.emplace_back(f.name, ft);
    }
    if (ok)
      structs_[sd.name] = std::move(info);
    else if (!structs_.count(sd.name))
      structs_[sd.name] = StructInfo{}; // cegah error beruntun
  }
  // Validasi eager semua alias (Phase 21a): target tak dikenal, siklus,
  // dan rantai dalam DITOLAK walau alias tak pernah dipakai (konsisten
  // dengan field struct yang selalu divalidasi). Gagal di sini → masuk
  // aliasBad_ sehingga situs pakai tetap diam (tanpa error ganda).
  // (Sesudah pass 1a agar forward reference ke struct dikenali.)
  for (const auto &ad : prog.aliases) {
    if (!aliases_.count(ad.name))
      continue; // duplikat/tabrakan — sudah dilaporkan
    (void)resolveType(Type::named(ad.name), ad.loc);
  }
  // Pass 1c: signature trait (Phase 22; butuh structs/alias siap).
  // Dijalankan SEBELUM pass 1b agar bounds `T: Trait` tervalidasi
  // saat signature generik dibangun.
  for (const auto &td : prog.traits)
    buildTraitSig(td);
  // Pass 1b: signatures (rekursi & forward reference bisa dicek di pass 2).
  for (auto &fn : prog.functions) {
    funcNames_.insert(fn.name);
    if (fn.isExtern) {
      // FFI C (Phase 32): signature terbatas + tanpa body.
      checkExternSig(fn);
      continue;
    }
    if (!fn.tparams.empty()) {
      // Fungsi generik (Phase 21c): signature template; body dicek
      // per-instance saat instansiasi, bukan di sini.
      buildGenericSig(fn);
      continue;
    }
    Signature sig;
    bool sigOk = true;
    for (auto &param : fn.params) {
      Type pt = resolveType(param.type, param.loc);
      if (pt.isError()) {
        sigOk = false;
        sig.params.push_back(Type::error());
        continue;
      }
      param.type = mapAppToMangled(pt); // kanonikalisasi in-place
      if (pt.kind == TypeKind::Void) {
        errorAt(param.loc, "parameter '" + param.name +
                               "' cannot have type 'void'",
                "use 'i64', 'bool', or 'string'");
        sigOk = false;
      }
      if (pt.kind == TypeKind::Array) {
        // v0.7.0 minimal: array hanya sebagai nilai lokal (literal,
        // indexing, len). Parameter/return array = slices (mendatang).
        errorAt(param.loc, "array parameters are not supported",
                "move the array logic inside the function");
        sigOk = false;
      }
      sig.params.push_back(pt);
    }
    Type rt = resolveType(fn.retType, fn.loc);
    if (rt.isError()) {
      sigOk = false;
    } else {
      fn.retType = mapAppToMangled(rt); // kanonikalisasi in-place
      if (rt.kind == TypeKind::Array) {
        errorAt(fn.loc, "array return types are not supported",
                "return an element or the length instead");
        sigOk = false;
      }
    }
    sig.ret = rt;
    if (!sigOk)
      sig.ret = Type::error();
    sigs_[fn.name] = sig;
  }
  auto mainIt = sigs_.find("main");
  if (mainIt != sigs_.end())
    mainFuncType_ = Type::func({}, mainIt->second.ret);
  // Pass 1d: blok impl (Phase 22) → decl mangled ke pendingMethods_.
  // (Tetap terpisah dari prog.functions selama analisis agar pointer
  // template (GenericSig::tmpl) tak dangling oleh realloc.)
  for (const auto &ib : prog.impls)
    processImplBlock(ib);
  // Pass 1e2: konstanta waktu-kompilasi (Phase 70, RFC-0009 slice 1):
  // evaluasi + substitusi desugar SEBELUM pass 2 (match desugar,
  // monomorphisasi, lambda-lift melihat literal, bukan VarRef).
  processConsts(prog);
  // Pass 2: bodies (template generik dilewati — dicek per-instance).
  // Termasuk metode impl yang dipindah (tubuh dicek dengan self).
  // Extern dilewati (tanpa body).
  for (auto &fn : prog.functions) {
    if (!fn.tparams.empty())
      continue;
    if (fn.isExtern)
      continue;
    checkFunction(fn);
  }
  for (auto &md : pendingMethods_)
    checkFunction(md);
  // Monomorphisasi (Phase 21c): instance konkret masuk program untuk
  // codegen; template dibuang (tak pernah callable langsung — semua
  // panggilan diinferensi + ditulis ulang saat pass 2).
  // (Semua append DI SINI, usai analisis, agar pointer template stabil.
  // Lihat komentar pendingMethods_.)
  for (auto &md : pendingMethods_)
    prog.functions.push_back(std::move(md));
  pendingMethods_.clear();
  for (auto &inst : instances_)
    prog.functions.push_back(std::move(inst));
  instances_.clear();
  prog.functions.erase(
      std::remove_if(prog.functions.begin(), prog.functions.end(),
                     [](const FunctionDecl &fn) {
                       return !fn.tparams.empty();
                     }),
      prog.functions.end());
  // Monomorphisasi struct (Phase 21d): cermin fungsi di atas.
  for (auto &sd : structInstDecls_)
    prog.structs.push_back(std::move(sd));
  structInstDecls_.clear();
  // Lift lambda (Phase 27): fungsi + struct env hidden menyusul ke prog
  // (concrete; lolos erase template di bawah).
  for (auto &fn : lambdaFuncs_)
    prog.functions.push_back(std::move(fn));
  lambdaFuncs_.clear();
  for (auto &sd : lambdaStructs_)
    prog.structs.push_back(std::move(sd));
  lambdaStructs_.clear();
  prog.structs.erase(
      std::remove_if(prog.structs.begin(), prog.structs.end(),
                     [](const StructDecl &sd) {
                       return !sd.tparams.empty();
                     }),
      prog.structs.end());
  return errors_.empty();
}

// Resolve tipe bernama (`Person`) ke definisi; tipe lain apa adanya
// (termasuk rekursi ke elemen array). Error bila tak dikenal.
// Alias `type` (Phase 21a) di-resolve transparan ke targetnya
// (mendukung forward reference; siklus/rantai dalam ditolak).
Type TypeChecker::resolveType(const Type &ty, const Location &loc) {
  if (ty.kind == TypeKind::Struct && ty.sname &&
      aliases_.count(*ty.sname)) {
    const std::string &name = *ty.sname;
    // Resolusi alias ini sudah gagal sebelumnya (error dilaporkan di
    // situs pertama) — diam agar tidak ada error beruntun.
    if (aliasBad_.count(name))
      return Type::error();
    auto hit = std::find(resolving_.begin(), resolving_.end(), name);
    if (hit != resolving_.end()) {
      std::string chain;
      for (auto it = hit; it != resolving_.end(); ++it)
        chain += (chain.empty() ? "" : " -> ") + *it;
      chain += " -> " + name;
      errorAt(loc, "circular type alias '" + name + "'",
              "circular chain: " + chain);
      aliasBad_.insert(name);
      return Type::error();
    }
    if (resolving_.size() >= kMaxAliasDepth) {
      errorAt(loc, "type alias chain too deep (over 100 aliases)",
              "an alias chain this deep is almost certainly a design error");
      aliasBad_.insert(name);
      return Type::error();
    }
    resolving_.push_back(name);
    Type r = resolveType(aliases_[name], loc);
    resolving_.pop_back();
    if (r.isError())
      aliasBad_.insert(name);
    return r;
  }
  if (ty.kind == TypeKind::Struct) {
    if (!ty.sname)
      return Type::error(); // defensif; parser selalu mengisi nama
    const std::string &name = *ty.sname;
    // Template struct generik (Phase 21d)?
    auto tmplIt = structTemplates_.find(name);
    if (tmplIt != structTemplates_.end()) {
      if (structBad_.count(name))
        return Type::error(); // sudah dilaporkan; diam
      const StructDecl *tmpl = tmplIt->second;
      // Resolve argumen (alias ikut ter-resolve; Param lolos).
      std::vector<Type> rargs;
      for (const auto &a : ty.sargs) {
        Type r = resolveType(a, loc);
        if (r.isError())
          return Type::error();
        rargs.push_back(r);
      }
      if (rargs.empty()) {
        errorAt(loc, "generic struct '" + name +
                         "' used without type arguments (expects " +
                         std::to_string(tmpl->tparams.size()) + ")",
                "write '" + name + "<...>' with concrete types");
        return Type::error();
      }
      if (rargs.size() != tmpl->tparams.size()) {
        errorAt(loc, "wrong number of type arguments for '" + name +
                         "': expected " +
                         std::to_string(tmpl->tparams.size()) + ", found " +
                         std::to_string(rargs.size()),
                "");
        return Type::error();
      }
      // Rigid (mengandung Param, mis. di signature generik)? Kembalikan
      // apa adanya — substitusi terjadi saat instansiasi pemakai.
      for (const auto &r : rargs) {
        if (containsParam(r))
          return Type::applied(name, rargs);
      }
      return instantiateStruct(name, rargs, loc);
    }
    if (!ty.sargs.empty()) {
      errorAt(loc, "struct '" + name + "' is not generic",
              "remove '<...>' after '" + name + "'");
      return Type::error();
    }
    if (!structs_.count(name)) {
      errorAt(loc, "unknown type '" + name + "'",
              "declare 'struct ...' or 'type ...', or use i64, bool, "
              "string, void, [T]");
      return Type::error();
    }
    return ty;
  }
  if (ty.kind == TypeKind::Array && ty.elem) {
    Type e = resolveType(*ty.elem, loc);
    if (e.isError())
      return Type::error();
    return Type::array(e);
  }
  return ty;
}

void TypeChecker::errorAt(const Location &loc, const std::string &msg,
                          const std::string &hint, int length) {
  errors_.push_back(SemanticError{loc, msg, hint, length});
}

void TypeChecker::checkFunction(FunctionDecl &fn) {
  // Simpan state pemanggil (Phase 21c): body instance generik dicek
  // BERSARANG di dalam checkFunction lain (via instantiateGeneric).
  // Tanpa ini currentRet_/currentFunc_/env_ pemanggil tertimpa instance.
  std::string savedFunc = currentFunc_;
  Type savedRet = currentRet_;
  Scopes<Type> savedEnv = env_;
  env_.clear();
  env_.push(); // scope fungsi
  currentFunc_ = fn.name;
  auto sigIt = sigs_.find(fn.name);
  currentRet_ = (sigIt == sigs_.end()) ? Type::error() : sigIt->second.ret;
  // Pakai tipe parameter TERESOLUSI dari signature (bukan mentah).
  if (sigIt != sigs_.end()) {
    for (size_t i = 0; i < fn.params.size() && i < sigIt->second.params.size();
         i++) {
      env_.set(fn.params[i].name, sigIt->second.params[i]);
    }
  } else {
    for (const auto &param : fn.params)
      env_.set(param.name, resolveType(param.type, param.loc));
  }

  checkBlock(fn.body);

  // Fungsi non-void harus SELALU me-return (aturan all-paths v0.6.0).
  if (currentRet_.kind != TypeKind::Void && !currentRet_.isError() &&
      !blockAlwaysReturns(fn.body)) {
    errorAt(fn.loc, "function '" + fn.name + "' must return a value of " +
                        "type '" + currentRet_.name() + "' on every path",
            "end the body (and each 'if'/'else' branch) with "
            "'return <expr>'");
  }
  currentFunc_ = std::move(savedFunc);
  currentRet_ = std::move(savedRet);
  env_ = std::move(savedEnv);
}

// True bila block PASTI keluar via `return` bernilai.
bool TypeChecker::blockAlwaysReturns(const Block &block) {
  if (block.stmts.empty())
    return false;
  const auto &last = block.stmts.back();
  if (std::holds_alternative<ReturnStmt>(last))
    return std::get<ReturnStmt>(last).value != nullptr;
  if (std::holds_alternative<IfStmt>(last)) {
    const auto &is = std::get<IfStmt>(last);
    // `if true` transparan (Phase 25: ekor desugar match satu-arm;
    // presisi: literal `true` pasti mengambil cabang then).
    if (!is.elseBlock) {
      if (is.cond) {
        if (const auto *lit =
                std::get_if<Expr::LiteralBool>(&is.cond->node)) {
          if (lit->value)
            return blockAlwaysReturns(*is.thenBlock);
        }
      }
      return false;
    }
    return blockAlwaysReturns(*is.thenBlock) &&
           blockAlwaysReturns(*is.elseBlock);
  }
  return false;
}

void TypeChecker::checkBlock(Block &block) {
  env_.push(); // tiap Block = scope baru
  bool seenReturn = false;
  // Bangun ulang vektor (Phase 25): `match` di-splice menjadi
  // if/let biasa. Iterasi indeks aman (tak ada realloc `block`
  // selama loop; elemen dipindah, bukan dihapus).
  Block out;
  for (size_t i = 0; i < block.stmts.size(); i++) {
    Stmt &stmt = block.stmts[i];
    if (seenReturn) {
      const Location &loc = std::visit(
          [](const auto &s) -> const Location & { return s.loc; }, stmt);
      errorAt(loc, "unreachable code after 'return'",
              "'return' must be the last statement in its block");
      // Tetap periksa statement agar error lain ikut terlapor.
    }
    if (auto *ms = std::get_if<MatchStmt>(&stmt)) {
      checkMatchStmt(*ms, out);
      // seenReturn sengaja tak diperbarui (analisis all-paths fungsi
      // tetap bekerja pada hasil splice; lint lokal sedikit melemah).
      continue;
    }
    checkStmt(stmt);
    bool isRet = std::holds_alternative<ReturnStmt>(stmt);
    out.stmts.push_back(std::move(stmt));
    if (isRet)
      seenReturn = true;
  }
  env_.pop();
  block.stmts = std::move(out.stmts);
}

namespace {
// Lokasi pola (cermin patLoc parser; untuk diagnostik checker).
const Location &patLocOf(const Pat &p) {
  struct Visitor {
    const Location &operator()(const Pat::Wild &n) const { return n.loc; }
    const Location &operator()(const Pat::Bind &n) const { return n.loc; }
    const Location &operator()(const Pat::LitInt &n) const { return n.loc; }
    const Location &operator()(const Pat::LitBool &n) const { return n.loc; }
    const Location &operator()(const Pat::LitStr &n) const { return n.loc; }
    const Location &operator()(const Pat::Struct &n) const { return n.loc; }
    const Location &operator()(const Pat::Variant &n) const { return n.loc; }
  };
  return std::visit(Visitor{}, p.node);
}
} // namespace

const TypeChecker::StructInfo *TypeChecker::structInfoFor(const Type &ty) {
  if (ty.kind != TypeKind::Struct)
    return nullptr;
  auto it = structs_.find(structKey(ty));
  if (it == structs_.end())
    return nullptr;
  return &it->second;
}

bool TypeChecker::checkVariantPattern(Pat::Variant &v,
                                      const Type &subjectTy) {
  // Subject BUKAN enum (Phase 45/B1): encoding struct v1.x untuk
  // Some/None/Ok/Err DIHAPUS (breaking 2.0 — migration-2.0.md).
  // Varian hanya valid atas enum (cabang enum di checkPattern).
  std::string hint = "variants come from enums; match '" + v.name +
                     "' against its enum type";
  if (v.name == "Some" || v.name == "None")
    hint += " ('Option' is an enum since 2.0: import option;)";
  else if (v.name == "Ok" || v.name == "Err")
    hint += " ('Result' is an enum since 2.0: import result;)";
  errorAt(v.loc, "cannot match variant '" + v.name + "' against '" +
                     subjectTy.name() + "'",
          hint);
  return false;
}

bool TypeChecker::patIsIrrefutable(const Pat &p) {
  if (std::holds_alternative<Pat::Wild>(p.node) ||
      std::holds_alternative<Pat::Bind>(p.node))
    return true;
  if (const auto *s = std::get_if<Pat::Struct>(&p.node)) {
    for (const auto &f : s->fields) {
      if (!patIsIrrefutable(*f.pat))
        return false;
    }
    return true;
  }
  return false; // literal + varian (diskriminan literal) refutable
}

bool TypeChecker::checkPattern(Pat &pat, const Type &subjectTy) {
  // Bind bernama varian (Phase 26): `Red` polos atas subject enum
  // dibaca sebagai varian nullary, bukan binding. Mutasi node —
  // cermin rewrite MethodCall → Call (checker menulis ulang AST).
  if (const auto *b = std::get_if<Pat::Bind>(&pat.node)) {
    if (subjectTy.kind == TypeKind::Struct && subjectTy.sname &&
        enums_.count(*subjectTy.sname)) {
      const EnumInfo &einfo = enums_[*subjectTy.sname];
      bool found = false;
      for (const auto &c : einfo.variants) {
        if (c.name == b->name) {
          Pat::Variant vv;
          vv.name = b->name;
          vv.loc = b->loc;
          pat.node = std::move(vv);
          found = true;
          break;
        }
      }
      // Kapital tak-dikenal atas enum = kemungkinan salah ketik varian
      // (binding lowercase selalu bebas; binding Kapital dicadangkan
      // untuk varian agar typo tak diam-diam mengikat segalanya).
      if (!found && !b->name.empty() && b->name[0] >= 'A' &&
          b->name[0] <= 'Z') {
        std::string variants;
        for (size_t i = 0; i < einfo.variants.size(); i++) {
          if (i)
            variants += ", ";
          variants += einfo.variants[i].name;
        }
        errorAt(b->loc, "unknown variant '" + b->name + "' for enum '" +
                            *subjectTy.sname + "'",
                "variants of '" + *subjectTy.sname + "': " + variants);
        return false;
      }
    }
  }
  // Wild/bind selalu valid (bind dipakai nanti saat desugar).
  if (std::holds_alternative<Pat::Wild>(pat.node) ||
      std::holds_alternative<Pat::Bind>(pat.node))
    return true;
  const Location &ploc = patLocOf(pat);
  if (const auto *li = std::get_if<Pat::LitInt>(&pat.node)) {
    (void)li;
    if (subjectTy.kind != TypeKind::I64) {
      errorAt(ploc, "invalid pattern: integer literal cannot match '" +
                        subjectTy.name() + "'",
              "match integer patterns against 'i64' values");
      return false;
    }
    return true;
  }
  if (const auto *lb = std::get_if<Pat::LitBool>(&pat.node)) {
    (void)lb;
    if (subjectTy.kind != TypeKind::Bool) {
      errorAt(ploc, "invalid pattern: boolean literal cannot match '" +
                        subjectTy.name() + "'",
              "match boolean patterns against 'bool' values");
      return false;
    }
    return true;
  }
  if (const auto *ls = std::get_if<Pat::LitStr>(&pat.node)) {
    (void)ls;
    if (subjectTy.kind != TypeKind::String) {
      errorAt(ploc, "invalid pattern: string literal cannot match '" +
                        subjectTy.name() + "'",
              "match string patterns against 'string' values");
      return false;
    }
    return true;
  }
  if (const auto *sp = std::get_if<Pat::Struct>(&pat.node)) {
    // Enum bukan struct (Phase 26): pola struct atas hidden struct
    // ditolak dengan arahan (tempa manual tak didukung).
    if (enums_.count(sp->name)) {
      errorAt(ploc, "cannot match enum '" + sp->name + "' as struct",
              "use variant patterns: " + sp->name + ".Variant(...)");
      return false;
    }
    if (subjectTy.kind != TypeKind::Struct || !subjectTy.sname) {
      errorAt(ploc, "invalid pattern: struct '" + sp->name +
                        "' cannot match '" + subjectTy.name() + "'",
              "match struct patterns against struct values");
      return false;
    }
    // Nama template harus sama (bentuk aplikasi boleh beda presisi
    // selama nama cocok — field dicek di bawah via instance).
    std::string subjTmpl = *subjectTy.sname;
    // (Subject selalu kanonis: applied membawa nama template asli.)
    if (subjTmpl != sp->name) {
      // Mungkin subject mangled? Tak pernah (value selalu applied).
      errorAt(ploc, "invalid pattern: struct '" + sp->name +
                        "' cannot match '" + subjectTy.name() + "'",
              "match each struct with its own pattern");
      return false;
    }
    // Eksplisit harus sama persis dengan subject.
    if (!sp->targs.empty()) {
      Type want = Type::applied(sp->name, sp->targs);
      Type resolved = resolveType(want, ploc);
      if (resolved.isError())
        return false;
      if (!(resolved == subjectTy)) {
        errorAt(ploc, "invalid pattern: '" + want.name() +
                          "' cannot match '" + subjectTy.name() + "'",
                "write the pattern with the subject's type arguments");
        return false;
      }
    }
    // Field: dikenal + lengkap + rekursif.
    const StructInfo *info = structInfoFor(subjectTy);
    if (!info)
      return false; // defensif; seharusnya ada (subject valid)
    for (auto &f : sp->fields) {
      const Type *fty = nullptr;
      for (const auto &pr : info->fields) {
        if (pr.first == f.name) {
          fty = &pr.second;
          break;
        }
      }
      if (!fty) {
        errorAt(f.loc, "unknown field '" + f.name + "' in pattern for '" +
                           subjectTy.name() + "'",
                "pattern must list known fields");
        return false;
      }
      if (!checkPattern(*f.pat, *fty))
        return false;
    }
    for (const auto &pr : info->fields) {
      bool listed = false;
      for (const auto &f : sp->fields) {
        if (f.name == pr.first) {
          listed = true;
          break;
        }
      }
      if (!listed) {
        errorAt(ploc, "pattern missing field '" + pr.first + "' for '" +
                          subjectTy.name() + "'",
                "list every field in struct patterns");
        return false;
      }
    }
    return true;
  }
  if (auto *v = std::get_if<Pat::Variant>(&pat.node)) {
    // Enum pengguna dulu (Phase 26); std (`Some`/`Ok`/...) bila subject
    // bukan enum.
    if (subjectTy.kind == TypeKind::Struct && subjectTy.sname &&
        enums_.count(*subjectTy.sname) &&
        !enums_[*subjectTy.sname].bad)
      return checkEnumVariantPattern(*v, subjectTy);
    return checkVariantPattern(*v, subjectTy);
  }
  return true;
}

bool TypeChecker::checkEnumVariantPattern(Pat::Variant &v,
                                          const Type &subjectTy) {
  const std::string &ename = *subjectTy.sname;
  const EnumInfo &info = enums_[ename];
  const EnumVariantInfo *vi = nullptr;
  for (const auto &c : info.variants) {
    if (c.name == v.name) {
      vi = &c;
      break;
    }
  }
  if (!vi) {
    std::string variants;
    for (size_t i = 0; i < info.variants.size(); i++) {
      if (i)
        variants += ", ";
      variants += info.variants[i].name;
    }
    errorAt(v.loc, "unknown variant '" + v.name + "' for enum '" + ename +
                       "'",
            "variants of '" + ename + "': " + variants);
    return false;
  }
  if (v.args.size() != vi->arity) {
    errorAt(v.loc, "variant '" + v.name + "' expects " +
                       std::to_string(vi->arity) + " payload pattern(s), found " +
                       std::to_string(v.args.size()),
            "");
    return false;
  }
  // Anotasi tag untuk desugar (buildPatTest tak menerima tipe subject).
  v.tag = vi->tag;
  // Tipe payload konkret dari hidden struct terinstansiasi.
  const StructInfo *si = structInfoFor(subjectTy);
  if (!si)
    return false; // defensif; seharusnya ada (subject valid)
  for (size_t i = 0; i < v.args.size(); i++) {
    std::string fn = enumPayloadField(vi->tag, i);
    const Type *fty = nullptr;
    for (const auto &pr : si->fields) {
      if (pr.first == fn) {
        fty = &pr.second;
        break;
      }
    }
    if (!fty)
      return false; // defensif; hidden struct selalu punya slotnya
    if (!checkPattern(*v.args[i], *fty))
      return false;
  }
  return true;
}

std::unique_ptr<Expr> TypeChecker::makeVarRef(const std::string &name,
                                              const Location &loc) {
  auto e = std::make_unique<Expr>();
  e->node = Expr::VarRef{name, loc};
  return e;
}

std::unique_ptr<Expr> TypeChecker::makeLit(const Pat &pat) {
  auto e = std::make_unique<Expr>();
  const Location &loc = patLocOf(pat);
  if (const auto *n = std::get_if<Pat::LitInt>(&pat.node))
    e->node = Expr::LiteralInt{n->value, loc};
  else if (const auto *n = std::get_if<Pat::LitBool>(&pat.node))
    e->node = Expr::LiteralBool{n->value, loc};
  else if (const auto *n = std::get_if<Pat::LitStr>(&pat.node))
    e->node = Expr::LiteralString{n->value, loc};
  // Hanya dipanggil untuk pola literal (kontrak internal).
  return e;
}

std::unique_ptr<Expr> TypeChecker::makeEq(std::unique_ptr<Expr> lhs,
                                          std::unique_ptr<Expr> rhs,
                                          const Location &loc) {
  auto e = std::make_unique<Expr>();
  Expr::Binary b;
  b.op = BinOp::Eq;
  b.lhs = std::move(lhs);
  b.rhs = std::move(rhs);
  b.loc = loc;
  e->node = std::move(b);
  return e;
}

std::unique_ptr<Expr> TypeChecker::makeStrEq(std::unique_ptr<Expr> lhs,
                                             std::unique_ptr<Expr> rhs,
                                             const Location &loc) {
  auto e = std::make_unique<Expr>();
  Expr::Call c;
  c.callee = "str_eq";
  c.loc = loc;
  c.args.push_back(std::move(lhs));
  c.args.push_back(std::move(rhs));
  e->node = std::move(c);
  return e;
}

std::unique_ptr<Expr> TypeChecker::makeField(std::unique_ptr<Expr> base,
                                             const std::string &field,
                                             const Location &loc) {
  auto e = std::make_unique<Expr>();
  Expr::FieldAccess fa;
  fa.base = std::move(base);
  fa.field = field;
  fa.loc = loc;
  e->node = std::move(fa);
  return e;
}

void TypeChecker::buildPatTest(
    const Pat &pat, std::unique_ptr<Expr> accessor,
    std::vector<std::unique_ptr<Expr>> &tests,
    std::vector<LetStmt> &binds) {
  const Location &loc = patLocOf(pat);
  if (std::holds_alternative<Pat::Wild>(pat.node)) {
    return; // selalu cocok, tanpa bind
  }
  if (const auto *n = std::get_if<Pat::Bind>(&pat.node)) {
    LetStmt ls;
    ls.name = n->name;
    ls.loc = n->loc;
    ls.value = std::move(accessor);
    binds.push_back(std::move(ls));
    return;
  }
  if (std::holds_alternative<Pat::LitInt>(pat.node) ||
      std::holds_alternative<Pat::LitBool>(pat.node)) {
    tests.push_back(makeEq(std::move(accessor), makeLit(pat), loc));
    return;
  }
  if (std::holds_alternative<Pat::LitStr>(pat.node)) {
    tests.push_back(makeStrEq(std::move(accessor), makeLit(pat), loc));
    return;
  }
  if (const auto *s = std::get_if<Pat::Struct>(&pat.node)) {
    for (const auto &f : s->fields) {
      auto sub = makeField(cloneExpr(*accessor), f.name, f.loc);
      buildPatTest(*f.pat, std::move(sub), tests, binds);
    }
    return;
  }
  if (const auto *v = std::get_if<Pat::Variant>(&pat.node)) {
    if (v->tag >= 0) {
      // Varian enum pengguna (Phase 26; tag dianotasi saat validasi):
      // uji `tag` + destruktur slot payload `v{tag}_{pos}`.
      // (Mencakup Option/Result natif 2.0 — enum seperti lainnya.)
      auto disc = makeField(cloneExpr(*accessor), "tag", v->loc);
      auto wantLit = std::make_unique<Expr>();
      wantLit->node =
          Expr::LiteralInt{static_cast<int64_t>(v->tag), v->loc};
      tests.push_back(makeEq(std::move(disc), std::move(wantLit), v->loc));
      for (size_t i = 0; i < v->args.size(); i++) {
        std::unique_ptr<Expr> base =
            (i + 1 == v->args.size()) ? std::move(accessor)
                                      : cloneExpr(*accessor);
        auto sub =
            makeField(std::move(base), enumPayloadField(v->tag, i), v->loc);
        buildPatTest(*v->args[i], std::move(sub), tests, binds);
      }
      return;
    }
    // tag < 0 di sini = tak-terjangkau (validasi menolak varian atas
    // non-enum SEBELUM desugar). Defensif: tanpa uji.
    return;
  }
}

std::string TypeChecker::enumPayloadField(int tag, size_t pos) {
  return "v" + std::to_string(tag) + "_" + std::to_string(pos);
}

bool TypeChecker::processEnumDecl(const EnumDecl &ed, const Program &prog,
                                  StructDecl &hiddenOut) {
  // Tabrakan nama (pertahanan bila semantic dilewati; normalnya sudah
  // ditolak di sana).
  for (const auto &sd : prog.structs) {
    if (sd.name == ed.name) {
      errorAt(ed.loc, "duplicate type '" + ed.name + "'",
              "'" + ed.name + "' is already declared as a struct");
      EnumInfo bad;
      bad.bad = true;
      enums_[ed.name] = bad;
      return false;
    }
  }
  if (aliases_.count(ed.name)) {
    errorAt(ed.loc, "duplicate type '" + ed.name + "'",
            "'" + ed.name + "' is already declared as a type alias");
    EnumInfo bad;
    bad.bad = true;
    enums_[ed.name] = bad;
    return false;
  }
  if (enums_.count(ed.name)) {
    errorAt(ed.loc, "duplicate enum '" + ed.name + "'",
            "previous enum '" + ed.name + "' is kept");
    enums_[ed.name].bad = true;
    return false;
  }
  EnumInfo info;
  info.tparams = ed.tparams;
  // Payload: void/array/fn ditolak (cermin field struct; named lain
  // di-resolve mesin struct saat pass 1a/registrasi template).
  bool ok = true;
  int tag = 0;
  for (const auto &v : ed.variants) {
    EnumVariantInfo vi;
    vi.name = v.name;
    vi.arity = v.payload.size();
    vi.tag = tag++;
    info.variants.push_back(vi);
    for (const auto &pt : v.payload) {
      if (pt.kind == TypeKind::Void) {
        errorAt(v.loc, "enum variant '" + v.name + "' cannot have 'void' payload",
                "use a concrete type");
        ok = false;
      } else if (pt.kind == TypeKind::Array) {
        errorAt(v.loc, "enum variant '" + v.name + "' cannot have array payload",
                "store the array in a local variable, not in a variant");
        ok = false;
      } else if (pt.kind == TypeKind::Func) {
        errorAt(v.loc, "enum variant '" + v.name + "' cannot have function payload",
                "store the function in a local variable, not in a variant");
        ok = false;
      }
    }
  }
  if (!ok) {
    info.bad = true;
    enums_[ed.name] = info;
    return false;
  }
  // Hidden struct: `tag` + slot payload per varian (`v{tag}_{pos}`).
  // Tipe payload mentah (tparam-named ikut terkonversi Param oleh
  // registerStructTemplate untuk enum generik).
  StructDecl sd;
  sd.name = ed.name;
  sd.loc = ed.loc;
  sd.tparams = ed.tparams;
  sd.tparamBounds = ed.tparamBounds;
  sd.hiddenEnum = true;
  StructField tagF;
  tagF.name = "tag";
  tagF.type = Type::i64();
  tagF.loc = ed.loc;
  sd.fields.push_back(std::move(tagF));
  int t = 0;
  for (const auto &v : ed.variants) {
    for (size_t i = 0; i < v.payload.size(); i++) {
      StructField f;
      f.name = enumPayloadField(t, i);
      f.type = v.payload[i];
      f.loc = v.loc;
      sd.fields.push_back(std::move(f));
    }
    t++;
  }
  enums_[ed.name] = info;
  hiddenOut = std::move(sd);
  return true;
}

Type TypeChecker::checkEnumCtor(const std::string &ename,
                                const std::string &vname,
                                std::vector<std::unique_ptr<Expr>> args,
                                std::vector<Type> targs, Expr &owner,
                                const Location &loc) {
  auto eit = enums_.find(ename);
  if (eit == enums_.end() || eit->second.bad)
    return Type::error(); // defensif; deklarasi rusak sudah dilaporkan
  const EnumInfo &info = eit->second;
  const EnumVariantInfo *vi = nullptr;
  for (const auto &c : info.variants) {
    if (c.name == vname) {
      vi = &c;
      break;
    }
  }
  if (!vi) {
    std::string variants;
    for (size_t i = 0; i < info.variants.size(); i++) {
      if (i)
        variants += ", ";
      variants += info.variants[i].name;
    }
    errorAt(loc, "unknown variant '" + vname + "' for enum '" + ename + "'",
            "variants of '" + ename + "': " + variants);
    return Type::error();
  }
  if (args.size() != vi->arity) {
    errorAt(loc, "variant '" + vname + "' expects " +
                     std::to_string(vi->arity) + " argument(s), found " +
                     std::to_string(args.size()),
            "write '" + ename + "." + vname + "(...)' with " +
                std::to_string(vi->arity) + " argument(s)");
    return Type::error();
  }
  // Tipe payload formal: template (Param) untuk generik, konkret bila
  // non-generik. Kunci = nama field hidden (`v{tag}_{pos}`).
  auto formalOf = [&](size_t pos) -> const Type * {
    std::string fn = enumPayloadField(vi->tag, pos);
    if (!info.tparams.empty()) {
      auto tit = structTemplates_.find(ename);
      if (tit == structTemplates_.end() || !tit->second)
        return nullptr;
      for (const auto &f : tit->second->fields) {
        if (f.name == fn)
          return &f.type;
      }
      return nullptr;
    }
    auto sit = structs_.find(ename);
    if (sit == structs_.end())
      return nullptr;
    for (const auto &pr : sit->second.fields) {
      if (pr.first == fn)
        return &pr.second;
    }
    return nullptr;
  };
  // Cek argumen sekali (tanpa cek ganda di bawah).
  std::vector<Type> argTys;
  bool argsOk = true;
  for (auto &a : args) {
    Type t = checkExpr(*a);
    if (t.isError())
      argsOk = false;
    argTys.push_back(t);
  }
  if (!argsOk)
    return Type::error();
  std::unordered_map<std::string, Type> sub;
  std::vector<Type> instArgs;
  if (!targs.empty()) {
    // Eksplisit `E<T, ...>.V(...)`: resolve + instansiasi + verifikasi.
    if (targs.size() != info.tparams.size()) {
      errorAt(loc, "wrong number of type arguments for enum '" + ename +
                       "': expected " +
                       std::to_string(info.tparams.size()) + ", found " +
                       std::to_string(targs.size()),
              "");
      return Type::error();
    }
    if (info.tparams.empty()) {
      errorAt(loc, "enum '" + ename + "' is not generic",
              "remove the type arguments");
      return Type::error();
    }
    for (size_t i = 0; i < targs.size(); i++) {
      Type r = resolveType(targs[i], loc);
      if (r.isError())
        return Type::error();
      sub[info.tparams[i]] = r;
      instArgs.push_back(r);
    }
  } else if (!info.tparams.empty()) {
    // Inferensi dari payload: unifikasi formal vs aktual.
    for (size_t i = 0; i < args.size(); i++) {
      const Type *formal = formalOf(i);
      if (!formal)
        return Type::error(); // defensif; template hilang
      if (!unifyType(*formal, argTys[i], sub)) {
        Type want = substType(*formal, sub);
        errorAt(args[i]->loc(), "mismatched variant payload: '" + ename +
                                    "." + vname + "' expects '" +
                                    want.name() + "', found '" +
                                    argTys[i].name() + "'");
        return Type::error();
      }
    }
    for (const auto &tp : info.tparams) {
      if (!sub.count(tp)) {
        errorAt(loc, "cannot infer type argument '" + tp + "' for enum '" +
                         ename + "'",
                "write '" + ename + "<...>" + "." + vname +
                    "(...)' with explicit types");
        return Type::error();
      }
      instArgs.push_back(sub[tp]);
    }
  }
  std::string litName = ename;
  Type retTy = Type::named(ename);
  if (!info.tparams.empty()) {
    Type app = instantiateStruct(ename, instArgs, loc);
    if (app.isError())
      return Type::error();
    litName = mangleStruct(ename, instArgs);
    retTy = Type::applied(ename, instArgs);
    if (!targs.empty()) {
      // Verifikasi payload eksplisit vs aktual (cermin mismatch struct).
      for (size_t i = 0; i < args.size(); i++) {
        const Type *formal = formalOf(i);
        if (!formal)
          continue;
        Type want = substType(*formal, sub);
        if (!(argTys[i] == want)) {
          errorAt(args[i]->loc(), "mismatched variant payload: '" + ename +
                                      "." + vname + "' expects '" +
                                      want.name() + "', found '" +
                                      argTys[i].name() + "'");
          return Type::error();
        }
      }
    }
  } else {
    // Non-generik: bandingkan langsung (cermin mismatch struct).
    for (size_t i = 0; i < args.size(); i++) {
      const Type *want = formalOf(i);
      if (!want || argTys[i].isError())
        continue;
      if (!(argTys[i] == *want)) {
        errorAt(args[i]->loc(), "mismatched variant payload: '" + ename +
                                    "." + vname + "' expects '" +
                                    want->name() + "', found '" +
                                    argTys[i].name() + "'");
        return Type::error();
      }
    }
  }
  // Tulis ulang menjadi StructLit SUBSET hidden struct: `tag` + slot
  // aktif saja. Slot non-aktif tak diisi (= undef); codegen
  // mentoleransi field hilang khusus hidden-enum.
  Expr::StructLit lit;
  lit.name = litName;
  lit.loc = loc;
  lit.enumCtor = true;
  lit.enumType = retTy;
  {
    Expr::StructLit::FieldInit tagInit;
    tagInit.name = "tag";
    tagInit.loc = loc;
    auto tagVal = std::make_unique<Expr>();
    tagVal->node = Expr::LiteralInt{static_cast<int64_t>(vi->tag), loc};
    tagInit.value = std::move(tagVal);
    lit.fields.push_back(std::move(tagInit));
  }
  for (size_t i = 0; i < args.size(); i++) {
    Expr::StructLit::FieldInit fi;
    fi.name = enumPayloadField(vi->tag, i);
    fi.loc = args[i]->loc();
    fi.value = std::move(args[i]);
    lit.fields.push_back(std::move(fi));
  }
  owner.node = std::move(lit);
  return retTy;
}

void TypeChecker::expectLambda(Expr &e, const Type &formal) {
  auto *lam = std::get_if<Expr::Lambda>(&e.node);
  if (!lam || lam->bad || lam->expected)
    return;
  if (formal.kind != TypeKind::Func || !formal.ret)
    return;
  if (containsParam(formal))
    return; // jalur generik (ditangani checkGenericCall, bukan di sini)
  if (lam->params.size() != formal.params.size()) {
    errorAt(lam->loc, "lambda expects " +
                          std::to_string(formal.params.size()) +
                          " parameter(s), found " +
                          std::to_string(lam->params.size()),
            "a lambda used here must match '" + formal.name() + "'");
    lam->bad = true;
    return;
  }
  lam->expectParams.clear();
  for (size_t i = 0; i < lam->params.size(); i++) {
    if (lam->params[i].annot) {
      Type a = resolveType(*lam->params[i].annot, lam->params[i].loc);
      if (a.isError()) {
        lam->bad = true;
        return;
      }
      if (!(a == formal.params[i])) {
        errorAt(lam->params[i].loc, "mismatched lambda parameter type: '" +
                                        lam->params[i].name + "' is '" +
                                        a.name() + "', expected '" +
                                        formal.params[i].name() + "'",
                "remove the annotation or match the expected type");
        lam->bad = true;
        return;
      }
      lam->expectParams.push_back(a);
    } else {
      lam->expectParams.push_back(formal.params[i]);
    }
  }
  lam->expectRet = *formal.ret;
  lam->expected = true;
}

namespace {
// Nama yang di-bind satu pola (untuk scope-walk lambda, Phase 27).
void patBindNames(const Pat &pat, std::vector<std::string> &out) {
  struct Visitor {
    std::vector<std::string> &out;
    void operator()(const Pat::Bind &n) { out.push_back(n.name); }
    void operator()(const Pat::Struct &n) {
      for (const auto &f : n.fields)
        patBindNames(*f.pat, out);
    }
    void operator()(const Pat::Variant &n) {
      for (const auto &a : n.args)
        patBindNames(*a, out);
    }
    void operator()(const Pat::Wild &) {}
    void operator()(const Pat::LitInt &) {}
    void operator()(const Pat::LitBool &) {}
    void operator()(const Pat::LitStr &) {}
  };
  std::visit(Visitor{out}, pat.node);
}
} // namespace

void TypeChecker::collectLambdaCaptures(
    const Expr::Lambda &lam,
    std::vector<std::pair<std::string, Location>> &out) {
  struct Walker {
    std::vector<std::unordered_set<std::string>> scopes;
    std::vector<std::pair<std::string, Location>> &out;
    void push() { scopes.emplace_back(); }
    void pop() { scopes.pop_back(); }
    void add(const std::string &name) { scopes.back().insert(name); }
    bool declared(const std::string &name) const {
      for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->count(name))
          return true;
      }
      return false;
    }
    void use(const std::string &name, const Location &loc) {
      if (declared(name))
        return;
      for (const auto &pr : out) {
        if (pr.first == name)
          return; // dedup (lokasi pertama menang)
      }
      out.emplace_back(name, loc);
    }
    void walkExpr(const Expr &e) {
      struct V {
        Walker *self;
        void operator()(const Expr::VarRef &n) { self->use(n.name, n.loc); }
        void operator()(const Expr::Call &n) {
          // Callee string: bila variabel lokal (indirect call), ia free
          // var (global fn/builtin disaring saat resolve capture).
          self->use(n.callee, n.loc);
          for (const auto &a : n.args)
            self->walkExpr(*a);
        }
        void operator()(const Expr::MethodCall &n) {
          self->walkExpr(*n.receiver);
          for (const auto &a : n.args)
            self->walkExpr(*a);
        }
        void operator()(const Expr::Propagate &n) {
          self->walkExpr(*n.expr);
        }
        void operator()(const Expr::ArrayLit &n) {
          for (const auto &el : n.elems)
            self->walkExpr(*el);
        }
        void operator()(const Expr::Index &n) {
          self->walkExpr(*n.base);
          self->walkExpr(*n.index);
        }
        void operator()(const Expr::StructLit &n) {
          for (const auto &f : n.fields)
            self->walkExpr(*f.value);
        }
        void operator()(const Expr::FieldAccess &n) {
          self->walkExpr(*n.base);
        }
        void operator()(const Expr::Unary &n) {
          self->walkExpr(*n.operand);
        }
        void operator()(const Expr::Binary &n) {
          self->walkExpr(*n.lhs);
          self->walkExpr(*n.rhs);
        }
        void operator()(const Expr::Lambda &) {
          // Nested lambda: capture-nya diselesaikan saat lift-nya
          // sendiri (scope prepended-lets); jangan descend.
        }
        void operator()(const Expr::Closure &n) {
          for (const auto &c : n.captures)
            self->walkExpr(*c);
        }
        void operator()(const Expr::LiteralInt &) {}
        void operator()(const Expr::LiteralBool &) {}
        void operator()(const Expr::LiteralString &) {}
      };
      std::visit(V{this}, e.node);
    }
    void walkBlock(const Block &b) {
      push();
      for (const auto &s : b.stmts)
        walkStmt(s);
      pop();
    }
    void walkStmt(const Stmt &s) {
      struct V {
        Walker *self;
        void operator()(const LetStmt &n) {
          self->walkExpr(*n.value);
          self->add(n.name);
        }
        void operator()(const PrintStmt &n) { self->walkExpr(*n.arg); }
        void operator()(const ReturnStmt &n) {
          if (n.value)
            self->walkExpr(*n.value);
        }
        void operator()(const ExprStmt &n) { self->walkExpr(*n.expr); }
        void operator()(const IfStmt &n) {
          self->walkExpr(*n.cond);
          self->walkBlock(*n.thenBlock);
          if (n.elseBlock)
            self->walkBlock(*n.elseBlock);
        }
        void operator()(const WhileStmt &n) {
          self->walkExpr(*n.cond);
          self->walkBlock(*n.body);
        }
        void operator()(const AssignStmt &n) {
          self->walkExpr(*n.value);
        }
        void operator()(const MatchStmt &n) {
          self->walkExpr(*n.subject);
          for (const auto &arm : n.arms) {
            self->push();
            std::vector<std::string> binds;
            patBindNames(arm.pat, binds);
            for (const auto &bn : binds)
              self->add(bn);
            if (arm.guard)
              self->walkExpr(*arm.guard);
            // Body arm = Block (scope sendiri via walkBlock).
            self->walkBlock(*arm.body);
            self->pop();
          }
        }
      };
      std::visit(V{this}, s);
    }
  };
  Walker w{{}, out};
  w.push();
  for (const auto &p : lam.params)
    w.add(p.name);
  w.walkBlock(*lam.body);
  w.pop();
}

bool TypeChecker::exprHasLambda(const Expr &e) {
  struct V {
    bool found = false;
    void rec(const std::unique_ptr<Expr> &c) {
      if (c && !found)
        walk(*c);
    }
    void walk(const Expr &x) {
      if (found)
        return;
      if (std::holds_alternative<Expr::Lambda>(x.node)) {
        found = true;
        return;
      }
      struct W {
        V *self;
        void operator()(const Expr::VarRef &) {}
        void operator()(const Expr::LiteralInt &) {}
        void operator()(const Expr::LiteralBool &) {}
        void operator()(const Expr::LiteralString &) {}
        void operator()(const Expr::Call &n) {
          for (const auto &a : n.args)
            self->rec(a);
        }
        void operator()(const Expr::MethodCall &n) {
          self->rec(n.receiver);
          for (const auto &a : n.args)
            self->rec(a);
        }
        void operator()(const Expr::Propagate &n) { self->rec(n.expr); }
        void operator()(const Expr::ArrayLit &n) {
          for (const auto &el : n.elems)
            self->rec(el);
        }
        void operator()(const Expr::Index &n) {
          self->rec(n.base);
          self->rec(n.index);
        }
        void operator()(const Expr::StructLit &n) {
          for (const auto &f : n.fields)
            self->rec(f.value);
        }
        void operator()(const Expr::FieldAccess &n) { self->rec(n.base); }
        void operator()(const Expr::Unary &n) { self->rec(n.operand); }
        void operator()(const Expr::Binary &n) {
          self->rec(n.lhs);
          self->rec(n.rhs);
        }
        void operator()(const Expr::Lambda &) {}
        void operator()(const Expr::Closure &n) {
          for (const auto &c : n.captures)
            self->rec(c);
        }
      };
      std::visit(W{this}, x.node);
    }
  };
  V v;
  v.walk(e);
  return v.found;
}

Type TypeChecker::checkLambda(Expr::Lambda &lam, Expr &owner,
                               const Location &loc) {
  if (lam.bad)
    return Type::error();
  // 1. Parameter: dari propagasi, atau anotasi mandiri.
  std::vector<Type> ptypes;
  if (lam.expected) {
    ptypes = lam.expectParams;
  } else {
    for (auto &p : lam.params) {
      if (!p.annot) {
        errorAt(p.loc, "cannot infer type of lambda parameter '" + p.name +
                           "'",
                "annotate '|" + p.name +
                    ": T|' or use the lambda where a 'fn' type is expected");
        return Type::error();
      }
      Type a = resolveType(*p.annot, p.loc);
      if (a.isError())
        return Type::error();
      ptypes.push_back(a);
    }
  }
  // 2. Return: konkret → normal; Param/tanpa-ekspektasi → inferensi
  // kolektif (return dikumpulkan saat cek body, diputuskan sesudahnya).
  bool inferRet = true;
  Type retTy = Type::error();
  if (lam.expected && !lam.expectRet.isError() &&
      !containsParam(lam.expectRet)) {
    inferRet = false;
    retTy = lam.expectRet;
  }
  // 3. Free vars → captures (resolve di env_ enclosing). Tak-terresolve
  // dilewati (dilapor sekali saat cek body); global fn/builtin bukan
  // capture (di-resolve di body lifted).
  std::vector<std::pair<std::string, Location>> frees;
  collectLambdaCaptures(lam, frees);
  struct Cap {
    std::string name;
    Type type;
  };
  std::vector<Cap> caps;
  for (auto &fr : frees) {
    auto found = env_.lookup(fr.first);
    if (!found)
      continue;
    if (found->isError() || found->kind == TypeKind::Void)
      continue; // defensif; body melapor
    if (found->kind == TypeKind::Array) {
      errorAt(fr.second, "cannot capture array '" + fr.first + "' in lambda",
              "copy the needed elements to locals first");
      return Type::error();
    }
    if (containsParam(*found)) {
      errorAt(fr.second, "cannot capture generic value '" + fr.first +
                             "' in lambda",
              "lambdas capture concrete values; this is a type parameter");
      return Type::error();
    }
    caps.push_back(Cap{fr.first, *found});
  }
  // 4. Hidden env struct `$env_N { c0, ... }` — daftarkan SEGERA agar
  // resolveType param `$env` menemukannya (concrete; tanpa pass 1a).
  std::string envName = "$env_" + std::to_string(lambdaSeq_);
  std::string lamName = "$lambda_" + std::to_string(lambdaSeq_);
  lambdaSeq_++;
  StructDecl envDecl;
  envDecl.name = envName;
  envDecl.loc = loc;
  envDecl.closureEnv = true;
  StructInfo envInfo;
  for (size_t i = 0; i < caps.size(); i++) {
    StructField f;
    f.name = "c" + std::to_string(i);
    f.type = caps[i].type;
    f.loc = loc;
    envDecl.fields.push_back(f);
    envInfo.fields.emplace_back(f.name, f.type);
  }
  structs_[envName] = std::move(envInfo);
  lambdaStructs_.push_back(std::move(envDecl));
  // 5. Lifted function `$lambda_N($env, params...)`.
  FunctionDecl fn;
  fn.name = lamName;
  fn.loc = loc;
  Param envP;
  envP.name = "$env";
  envP.type = Type::named(envName);
  envP.loc = loc;
  fn.params.push_back(std::move(envP));
  for (size_t i = 0; i < lam.params.size(); i++) {
    Param np;
    np.name = lam.params[i].name;
    np.type = ptypes[i];
    np.loc = lam.params[i].loc;
    fn.params.push_back(std::move(np));
  }
  fn.retType = inferRet ? Type::error() : retTy;
  // Prepended lets: `let <cap> = $env.c<j>;` (salinan by-value di scope
  // body; cermin akses field tanpa rewrite manual).
  for (size_t j = 0; j < caps.size(); j++) {
    auto base = std::make_unique<Expr>();
    base->node = Expr::VarRef{"$env", loc};
    auto fa = std::make_unique<Expr>();
    Expr::FieldAccess facc;
    facc.base = std::move(base);
    facc.field = "c" + std::to_string(j);
    facc.loc = loc;
    fa->node = std::move(facc);
    LetStmt ls;
    ls.name = caps[j].name;
    ls.loc = loc;
    ls.value = std::move(fa);
    fn.body.stmts.insert(fn.body.stmts.begin() + j, Stmt{std::move(ls)});
  }
  for (auto &st : lam.body->stmts)
    fn.body.stmts.push_back(std::move(st));
  Signature sig;
  sig.params.push_back(Type::named(envName));
  for (auto &t : ptypes)
    sig.params.push_back(t);
  sig.ret = fn.retType;
  sigs_[lamName] = sig;
  // Cek body (rekursif: nested lambda ter-lift di sini; mode koleksi
  // disimpan-dipulihkan agar tak mencemari outer).
  size_t errBase = errors_.size();
  bool savedCollect = collectLambdaRet_;
  auto savedActuals = std::move(lambdaRetActuals_);
  lambdaRetActuals_.clear();
  collectLambdaRet_ = inferRet;
  checkFunction(fn);
  collectLambdaRet_ = savedCollect;
  std::vector<std::pair<Location, Type>> actuals =
      std::move(lambdaRetActuals_);
  lambdaRetActuals_ = std::move(savedActuals);
  // Samarkan nama hidden di error baru (`$lambda_N` → `lambda`).
  for (size_t i = errBase; i < errors_.size(); i++) {
    auto scrub = [&](std::string &s) {
      size_t p = 0;
      while ((p = s.find(lamName, p)) != std::string::npos) {
        s.replace(p, lamName.size(), "lambda");
        p += 6;
      }
    };
    scrub(errors_[i].message);
    scrub(errors_[i].hint);
  }
  if (errors_.size() != errBase)
    return Type::error();
  if (inferRet) {
    // Putuskan tipe return dari aktual terkumpul.
    for (size_t i = 1; i < actuals.size(); i++) {
      if (!(actuals[i].second == actuals[0].second)) {
        errorAt(actuals[i].first,
                "inconsistent lambda return types: '" +
                    actuals[0].second.name() + "' vs '" +
                    actuals[i].second.name() + "'",
                "make every 'return' produce the same type");
        return Type::error();
      }
    }
    if (actuals.empty()) {
      errorAt(loc, "cannot infer lambda return type (no 'return' found)",
              "use a block body with 'return <expr>' or place the lambda "
              "where a 'fn' type is expected");
      return Type::error();
    }
    retTy = actuals[0].second;
    fn.retType = retTy;
    sigs_[lamName].ret = retTy;
    if (!blockAlwaysReturns(fn.body)) {
      errorAt(loc, "lambda must return a value of type '" + retTy.name() +
                       "' on every path",
              "end the body (and each 'if'/'else' branch) with "
              "'return <expr>'");
      return Type::error();
    }
  }
  lambdaFuncs_.push_back(std::move(fn));
  // 6. Tulis ulang menjadi Closure (dibaca codegen).
  Expr::Closure c;
  c.lifted = lamName;
  c.envStruct = envName;
  c.loc = loc;
  for (auto &cp : caps) {
    auto ve = std::make_unique<Expr>();
    ve->node = Expr::VarRef{cp.name, loc};
    c.captures.push_back(std::move(ve));
  }
  c.funcType = Type::func(ptypes, retTy);
  Type ft = c.funcType;
  owner.node = std::move(c);
  return ft;
}

bool TypeChecker::checkExternSig(FunctionDecl &fn) {
  // Batasan FFI v1 (lihat docs/ffi.md): hanya primitif C; tanpa
  // struct/array/fn (tanpa layout/ABI-nya) dan tanpa string-return
  // (lifetime tak diketahui).
  bool ok = true;
  for (auto &param : fn.params) {
    Type pt = resolveType(param.type, param.loc);
    if (pt.isError()) {
      ok = false;
      continue;
    }
    param.type = mapAppToMangled(pt);
    bool allowed = pt.kind == TypeKind::I64 || pt.kind == TypeKind::String ||
                   pt.kind == TypeKind::Bool;
    if (!allowed) {
      std::string why;
      if (pt.kind == TypeKind::Void)
        why = "use 'i64', 'bool', or 'string'";
      else if (pt.kind == TypeKind::Func)
        why = "C cannot receive Nusa closures (no trampoline ABI)";
      else if (pt.kind == TypeKind::Array)
        why = "pass length + first element instead (slices are future work)";
      else
        why = "structs have no C layout (pass fields one by one)";
      errorAt(param.loc, "extern parameter '" + param.name +
                             "' cannot have type '" + pt.name() + "'", why);
      ok = false;
    }
  }
  Type rt = resolveType(fn.retType, fn.loc);
  if (rt.isError()) {
    ok = false;
  } else {
    fn.retType = mapAppToMangled(rt);
    bool allowed = rt.kind == TypeKind::I64 || rt.kind == TypeKind::Bool ||
                   rt.kind == TypeKind::Void;
    if (!allowed) {
      std::string why =
          (rt.kind == TypeKind::String)
              ? "C-returned strings have unknown lifetime; return an "
                "'i64' handle or length instead"
              : "only 'i64', 'bool', and 'void' can cross the FFI boundary";
      errorAt(fn.loc, "extern function '" + fn.name +
                          "' cannot return '" + rt.name() + "'",
              why);
      ok = false;
    }
  }
  Signature sig;
  sig.params.reserve(fn.params.size());
  for (const auto &p : fn.params)
    sig.params.push_back(p.type);
  sig.ret = ok ? rt : Type::error();
  sigs_[fn.name] = sig;
  if (ok)
    externFns_.insert(fn.name);
  return ok;
}

void TypeChecker::checkMatchStmt(MatchStmt &ms, Block &out) {  Type subTy = checkExpr(*ms.subject);
  if (subTy.isError())
    return; // subject rusak; diam (sudah dilapor)
  // 1. Validasi tiap pola.
  bool patsOk = true;
  for (auto &arm : ms.arms) {
    if (!checkPattern(arm.pat, subTy))
      patsOk = false;
  }
  if (!patsOk)
    return;
  // 2. Exhaustiveness + unreachable + literal ganda.
  // (Lanjut walau error agar body arm tetap dicek di desugar.)
  bool covered = false;
  std::unordered_set<std::string> seenLits;
  bool seenTrue = false;
  bool seenFalse = false;
  bool seenSome = false;
  bool seenNone = false;
  bool seenOk = false;
  bool seenErr = false;
  // Varian top-level tanpa guard (untuk coverage enum pengguna).
  std::unordered_set<std::string> seenVariants;
  for (auto &arm : ms.arms) {
    const Location &aloc = patLocOf(arm.pat);
    bool armUnreachable = false;
    if (covered) {
      errorAt(aloc, "unreachable pattern",
              "a previous arm always matches; remove this arm");
      armUnreachable = true;
    }
    // Guard membuat arm gagal walau pola cocok → arm ber-guard tak
    // menyumbang coverage (literal, bool, maupun varian).
    // Literal ganda (top-level) tak terjangkau.
    std::string litKey;
    if (const auto *n = std::get_if<Pat::LitInt>(&arm.pat.node))
      litKey = "i" + std::to_string(n->value);
    else if (const auto *n = std::get_if<Pat::LitBool>(&arm.pat.node))
      litKey = std::string("b") + (n->value ? "1" : "0");
    else if (const auto *n = std::get_if<Pat::LitStr>(&arm.pat.node))
      litKey = "s" + n->value;
    if (!litKey.empty() && !arm.guard) {
      if (!seenLits.insert(litKey).second) {
        errorAt(aloc, "unreachable pattern",
                "this literal is already covered above; remove this arm");
        armUnreachable = true;
      }
      if (litKey == "b1")
        seenTrue = true;
      if (litKey == "b0")
        seenFalse = true;
    }
    // Varian top-level tanpa guard (Some/None, Ok/Err tervalidasi).
    if (!arm.guard) {
      if (const auto *v = std::get_if<Pat::Variant>(&arm.pat.node)) {
        seenVariants.insert(v->name);
        if (v->name == "Some")
          seenSome = true;
        else if (v->name == "None")
          seenNone = true;
        else if (v->name == "Ok")
          seenOk = true;
        else if (v->name == "Err")
          seenErr = true;
      }
    }
    // (Arm unreachable tetap didesugar agar body-nya ikut diperiksa;
    // driver berhenti karena error sudah tercatat.)
    // Guard membuat arm refutable (gagal → lanjut ke arm berikut),
    // jadi tak menutup coverage walau polanya irrefutable.
    if (!armUnreachable && !arm.guard && patIsIrrefutable(arm.pat))
      covered = true;
  }
  // Bool tertutup penuh oleh `true`+`false` (tanpa `_` pun lengkap).
  if (!covered && subTy.kind == TypeKind::Bool && seenTrue && seenFalse)
    covered = true;
  // Option/Result tertutup oleh pasangan variannya (Some+None, Ok+Err).
  // Redundan dengan cabang enum generik di bawah (2.0: natif), tetapi
  // dipertahankan sebagai jaring pengaman.
  if (!covered && subTy.kind == TypeKind::Struct && subTy.sname) {
    if (*subTy.sname == "Option" && seenSome && seenNone)
      covered = true;
    if (*subTy.sname == "Result" && seenOk && seenErr)
      covered = true;
  }
  // Enum pengguna tertutup oleh SELURUH variannya (Phase 26).
  std::string missingVariants;
  if (!covered && subTy.kind == TypeKind::Struct && subTy.sname &&
      enums_.count(*subTy.sname) && !enums_[*subTy.sname].bad) {
    bool all = true;
    for (const auto &c : enums_[*subTy.sname].variants) {
      if (!seenVariants.count(c.name)) {
        all = false;
        if (!missingVariants.empty())
          missingVariants += ", ";
        missingVariants += c.name;
      }
    }
    if (all)
      covered = true;
  }
  if (!covered) {
    if (subTy.kind == TypeKind::Bool && (seenTrue != seenFalse)) {
      errorAt(ms.loc,
              std::string("non-exhaustive match: missing pattern for '") +
                  (seenTrue ? "false" : "true") + "'",
              "add the missing arm");
    } else if (!missingVariants.empty()) {
      errorAt(ms.loc, "non-exhaustive match: missing pattern(s) for '" +
                          missingVariants + "'",
              "add arms for the missing variants");
    } else {
      errorAt(ms.loc, "non-exhaustive match",
              "add '_' arm to cover remaining cases");
    }
  }
  // 3. Desugar (pola valid; node baru bersih → diam bila input bersih).
  desugarMatch(ms, subTy, out);
}

void TypeChecker::substGuardBinds(
    Expr &e, const std::unordered_map<std::string, const Expr *> &acc) {
  if (auto *v = std::get_if<Expr::VarRef>(&e.node)) {
    auto f = acc.find(v->name);
    if (f != acc.end() && f->second)
      e = std::move(*cloneExpr(*f->second));
    return;
  }
  struct Walker {
    TypeChecker *self;
    const std::unordered_map<std::string, const Expr *> &acc;
    void rec(std::unique_ptr<Expr> &c) {
      if (c)
        self->substGuardBinds(*c, acc);
    }
    void operator()(Expr::Call &n) {
      for (auto &a : n.args)
        rec(a);
    }
    void operator()(Expr::MethodCall &n) {
      rec(n.receiver);
      for (auto &a : n.args)
        rec(a);
    }
    void operator()(Expr::Propagate &n) { rec(n.expr); }
    void operator()(Expr::ArrayLit &n) {
      for (auto &el : n.elems)
        rec(el);
    }
    void operator()(Expr::Index &n) {
      rec(n.base);
      rec(n.index);
    }
    void operator()(Expr::StructLit &n) {
      for (auto &f : n.fields)
        rec(f.value);
    }
    void operator()(Expr::FieldAccess &n) { rec(n.base); }
    void operator()(Expr::Unary &n) { rec(n.operand); }
    void operator()(Expr::Binary &n) {
      rec(n.lhs);
      rec(n.rhs);
    }
    // Lambda di guard (Phase 27): ditolak checker (lihat desugarMatch),
    // jadi walker tak pernah menemuinya; pertahanan tanpa descend
    // (bind di dalam lambda diselesaikan saat lift, bukan substitusi).
    void operator()(Expr::Lambda &) {}
    void operator()(Expr::Closure &n) {
      for (auto &c : n.captures)
        rec(c);
    }
    void operator()(Expr::LiteralInt &) {}
    void operator()(Expr::LiteralBool &) {}
    void operator()(Expr::LiteralString &) {}
    void operator()(Expr::VarRef &) {} // sudah ditangani di atas
  };
  std::visit(Walker{this, acc}, e.node);
}

void TypeChecker::desugarMatch(MatchStmt &ms, const Type &subjectTy,
                                Block &out) {
  // subjectTy sudah divalidasi pemanggil; desugar murni struktural.
  (void)subjectTy;
  std::string tmp = "$match" + std::to_string(matchTmp_++);
  // let tmp = <subject> (dievaluasi sekali).
  {
    LetStmt ls;
    ls.name = tmp;
    ls.loc = ms.loc;
    ls.value = std::move(ms.subject);
    Stmt s = Stmt{std::move(ls)};
    checkStmt(s);
    out.stmts.push_back(std::move(s));
  }
  // Rantai dari belakang: else sebuah arm = ekspansi arm berikutnya.
  // Bentuk per arm (tanpa duplikasi, tanpa nesting dalam):
  //   let $mK = true;
  //   if T1 { } else { $mK = false; }   (per kondisi; murni → aman)
  //   if $mK { BINDS; BODY } else REST
  // Arm terakhir (irrefutable pada kode valid) = isi else langsung.
  // Satu arm = `if true { BINDS; BODY }` (transparan untuk all-paths).
  std::unique_ptr<Block> rest;
  for (size_t k = ms.arms.size(); k-- > 0;) {
    MatchArm &arm = ms.arms[k];
    std::vector<std::unique_ptr<Expr>> tests;
    std::vector<LetStmt> binds;
    buildPatTest(arm.pat, makeVarRef(tmp, arm.loc), tests, binds);
    bool isLast = (k + 1 == ms.arms.size());
    bool inlineTail = isLast && !arm.guard && patIsIrrefutable(arm.pat);
    if (!inlineTail) {
      // Guard: inline binding (VarRef → klon aksesor murni) agar guard
      // dapat dievaluasi SEBELUM binds materialisasi (tes berjalan di
      // luar thenB, sebelum `let` dieksekusi). Aman: aksesor murni
      // (var/field/lit); guard tak bisa shadowing (tanpa lambda di Nusa).
      if (arm.guard) {
        // Lambda di guard (Phase 27): capture akan membaca binding
        // yang belum materialisasi (tes berjalan sebelum `let` body).
        if (exprHasLambda(*arm.guard)) {
          errorAt(arm.guard->loc(), "lambdas cannot appear in match guards",
                  "bind first, then call the lambda in the arm body");
        } else {
          std::unordered_map<std::string, const Expr *> acc;
          for (auto &bn : binds)
            if (bn.value)
              acc[bn.name] = bn.value.get();
          substGuardBinds(*arm.guard, acc);
          // Validasi bool di sini (sekali) agar pesan menyebut guard;
          // guard invalid tak didorong ke tests (cegah error ganda).
          Type gt = checkExpr(*arm.guard);
          if (!gt.isError() && gt.kind != TypeKind::Bool) {
            errorAt(arm.guard->loc(),
                    "match guard must be of type 'bool', found '" +
                        gt.name() + "'",
                    "guards filter arms: `<pattern> if <bool-expr> => ...`");
          } else if (!gt.isError()) {
            tests.push_back(std::move(arm.guard));
          }
        }
      }
    }
    auto thenB = std::make_unique<Block>();
    for (auto &bn : binds)
      thenB->stmts.push_back(Stmt{std::move(bn)});
    for (auto &st : arm.body->stmts)
      thenB->stmts.push_back(std::move(st));
    // (thenB dicek sekali via checkStmt induk di bawah — tanpa cek ganda.)
    if (inlineTail) {
      if (ms.arms.size() == 1) {
        // Satu arm: bungkus `if true` (scope + transparan all-paths).
        auto condTrue = std::make_unique<Expr>();
        condTrue->node = Expr::LiteralBool{true, arm.loc};
        IfStmt is;
        is.cond = std::move(condTrue);
        is.thenBlock = std::move(thenB);
        is.elseBlock = nullptr;
        is.loc = arm.loc;
        Stmt s = Stmt{std::move(is)};
        checkStmt(s);
        out.stmts.push_back(std::move(s));
      } else {
        // Ekor rantai: isi else (scope asli, tanpa if).
        rest = std::move(thenB);
      }
      continue;
    }
    // Arm refutable (atau defensif): bentuk flag.
    std::string flag = "$m" + std::to_string(matchTmp_++);
    {
      LetStmt fl;
      fl.name = flag;
      fl.loc = arm.loc;
      auto fval = std::make_unique<Expr>();
      fval->node = Expr::LiteralBool{true, arm.loc};
      fl.value = std::move(fval);
      Stmt fs = Stmt{std::move(fl)};
      checkStmt(fs);
      out.stmts.push_back(std::move(fs));
    }
    for (auto &t : tests) {
      Location tloc = t->loc();
      auto setFalse = std::make_unique<Block>();
      {
        AssignStmt as;
        as.name = flag;
        as.loc = tloc;
        auto fval = std::make_unique<Expr>();
        fval->node = Expr::LiteralBool{false, tloc};
        as.value = std::move(fval);
        Stmt s = Stmt{std::move(as)};
        checkStmt(s);
        setFalse->stmts.push_back(std::move(s));
      }
      IfStmt is;
      is.cond = std::move(t);
      is.thenBlock = std::make_unique<Block>();
      is.elseBlock = std::move(setFalse);
      is.loc = tloc;
      Stmt s = Stmt{std::move(is)};
      checkStmt(s);
      out.stmts.push_back(std::move(s));
    }
    // if $mK { body } else REST (REST null hanya di kode invalid).
    auto condFlag = makeVarRef(flag, arm.loc);
    IfStmt is;
    is.cond = std::move(condFlag);
    is.thenBlock = std::move(thenB);
    is.elseBlock = std::move(rest);
    is.loc = arm.loc;
    if (k == 0) {
      // Level terluar: langsung ke out (else = ekor terkumpul).
      Stmt s = Stmt{std::move(is)};
      checkStmt(s);
      out.stmts.push_back(std::move(s));
    } else {
      // Bungkus menjadi ekor untuk level sebelumnya.
      rest = std::make_unique<Block>();
      Stmt s = Stmt{std::move(is)};
      checkStmt(s);
      rest->stmts.push_back(std::move(s));
    }
  }
  // rest selalu habis terpakai (arm pertama menjadi IfStmt ke `out`).
}

void TypeChecker::checkStmt(Stmt &stmt) {
  if (std::holds_alternative<LetStmt>(stmt)) {
    const auto &let = std::get<LetStmt>(stmt);
    // Lambda (Phase 27): propagasi anotasi SEBELUM cek init; resolve
    // sekali dan dipakai ulang di bawah (tanpa resolve ganda).
    Type preDeclTy = Type::error();
    bool hasPreDecl = false;
    if (let.declaredType &&
        std::holds_alternative<Expr::Lambda>(let.value->node)) {
      preDeclTy = resolveType(*let.declaredType, let.loc);
      hasPreDecl = !preDeclTy.isError();
      if (hasPreDecl && preDeclTy.kind == TypeKind::Func)
        expectLambda(*let.value, preDeclTy);
    }
    Type initTy = checkExpr(*let.value);
    if (let.declaredType) {
      Type declTy = hasPreDecl ? preDeclTy
                               : resolveType(*let.declaredType, let.loc);
      if (declTy.isError()) {
        env_.set(let.name, Type::error());
        return;
      }
      if (declTy.kind == TypeKind::Void) {
        errorAt(let.loc, "cannot declare variable '" + let.name +
                             "' with type 'void'",
                "use 'i64', 'bool', or 'string'");
        env_.set(let.name, Type::error());
        return;
      }
      if (declTy.kind == TypeKind::Array &&
          (!declTy.elem || declTy.elem->kind == TypeKind::Void)) {
        errorAt(let.loc, "array element type cannot be 'void'");
        env_.set(let.name, Type::error());
        return;
      }
      if (declTy.kind == TypeKind::Array &&
          std::holds_alternative<Expr::ArrayLit>(let.value->node)) {
        // Literal dicek per-elemen terhadap anotasi (termasuk literal
        // kosong yang tetap butuh kepastian; kosong ditolak di bawah).
        const auto &lit = std::get<Expr::ArrayLit>(let.value->node);
        for (const auto &e : lit.elems) {
          Type t = checkExpr(*e);
          if (!t.isError() && declTy.elem && t != *declTy.elem) {
            errorAt(e->loc(), "mismatched array element type: array '" +
                                  let.name + "' holds '" +
                                  declTy.elem->name() + "', found '" +
                                  t.name() + "'");
          }
        }
        env_.set(let.name, declTy);
        return;
      }
      if (!initTy.isError() && declTy != initTy) {
        errorAt(let.loc,
                "mismatched types in 'let': '" + let.name +
                    "' is declared as '" + declTy.name() +
                    "' but initializer has type '" + initTy.name() + "'",
                "change the annotation or the initial value to match");
        env_.set(let.name, declTy);
        return;
      }
      env_.set(let.name, declTy);
    } else {
      // Inferensi sederhana: tipe variabel = tipe inisialisasi.
      // Nilai void tak bisa disimpan ke variabel.
      if (initTy.kind == TypeKind::Void) {
        errorAt(let.loc, "cannot declare variable '" + let.name +
                             "' with 'void' value",
                "call the void function as a statement, not an initializer");
        env_.set(let.name, Type::error());
        return;
      }
      env_.set(let.name, initTy);
    }
    return;
  }
  if (std::holds_alternative<PrintStmt>(stmt)) {
    const auto &pr = std::get<PrintStmt>(stmt);
    Type argTy = checkExpr(*pr.arg);
    if (argTy.kind == TypeKind::Void) {
      errorAt(pr.loc, "cannot print value of type 'void'");
    } else if (argTy.kind == TypeKind::Struct) {
      // Enum (Phase 26): arahkan ke match, bukan field hidden.
      if (argTy.sname && enums_.count(*argTy.sname)) {
        errorAt(pr.loc, "cannot print value of type '" + argTy.name() + "'",
                "match on '" + *argTy.sname + "' and print the payload instead");
      } else {
        errorAt(pr.loc, "cannot print value of type '" + argTy.name() + "'",
                "print fields one by one, e.g. print(p.name)");
      }
    } else if (argTy.kind == TypeKind::Array) {
      errorAt(pr.loc, "cannot print value of type '" + argTy.name() + "'",
              "print elements one by one, e.g. print(a[0])");
    } else if (argTy.kind == TypeKind::Func) {
      errorAt(pr.loc, "cannot print value of type '" + argTy.name() + "'",
              "call it and print the result instead");
    }
    return;
  }
  if (std::holds_alternative<ExprStmt>(stmt)) {
    // Nilai hasil dibuang; tipe apa pun (termasuk void) legal.
    checkExpr(*std::get<ExprStmt>(stmt).expr);
    return;
  }
  if (std::holds_alternative<IfStmt>(stmt)) {
    const auto &is = std::get<IfStmt>(stmt);
    Type condTy = checkExpr(*is.cond);
    if (!condTy.isError() && condTy.kind != TypeKind::Bool) {
      errorAt(is.cond->loc(), "if condition must be of type 'bool', found '" +
                                  condTy.name() + "'");
    }
    checkBlock(*is.thenBlock);
    if (is.elseBlock)
      checkBlock(*is.elseBlock);
    return;
  }
  if (std::holds_alternative<WhileStmt>(stmt)) {
    const auto &ws = std::get<WhileStmt>(stmt);
    Type condTy = checkExpr(*ws.cond);
    if (!condTy.isError() && condTy.kind != TypeKind::Bool) {
      errorAt(ws.cond->loc(),
              "while condition must be of type 'bool', found '" +
                  condTy.name() + "'");
    }
    checkBlock(*ws.body);
    return;
  }
  if (std::holds_alternative<AssignStmt>(stmt)) {
    const auto &as = std::get<AssignStmt>(stmt);
    Type valTy = checkExpr(*as.value);
    auto found = env_.lookup(as.name);
    if (!found)
      return; // sudah dilaporkan semantic; tanpa duplikat
    if (valTy.isError() || found->isError())
      return;
    if (valTy != *found) {
      errorAt(as.loc, "mismatched types in assignment: '" + as.name +
                          "' has type '" + found->name() +
                          "' but value has type '" + valTy.name() + "'");
    }
    return;
  }
  const auto &ret = std::get<ReturnStmt>(stmt);
  if (!ret.value) {
    // `return;` polos: hanya untuk fungsi void.
    if (currentRet_.kind != TypeKind::Void && !currentRet_.isError()) {
      errorAt(ret.loc, "bare 'return' in function '" + currentFunc_ +
                           "' which returns '" + currentRet_.name() + "'",
              "return a value: 'return <expr>'");
    }
    return;
  }
  Type valTy = Type::error();
  // Lambda (Phase 27): propagasi tipe return SEBELUM cek (kecuali mode
  // koleksi inferensi — return lambda tak dipropagasi menembus batas).
  if (ret.value &&
      std::holds_alternative<Expr::Lambda>(ret.value->node) &&
      !collectLambdaRet_ && !currentRet_.isError() &&
      currentRet_.kind == TypeKind::Func)
    expectLambda(*ret.value, currentRet_);
  if (collectLambdaRet_ && ret.value) {
    Type t = checkExpr(*ret.value);
    if (!t.isError())
      lambdaRetActuals_.emplace_back(ret.loc, t);
    return;
  }
  if (ret.value)
    valTy = checkExpr(*ret.value);
  if (currentRet_.kind == TypeKind::Void && !currentRet_.isError()) {
    errorAt(ret.loc, "cannot return a value from void function '" +
                         currentFunc_ + "'",
            "use a bare 'return;' or change the return type");
    return;
  }
  if (!valTy.isError() && !currentRet_.isError() && valTy != currentRet_) {
    errorAt(ret.loc, "mismatched return type in '" + currentFunc_ +
                         "': expected '" + currentRet_.name() + "', found '" +
                         valTy.name() + "'");
  }
}

Type TypeChecker::checkBuiltin(Expr::Call &n, const Location &loc) {
  // Periksa semua argumen dulu (error dalam ikut terlapor).
  std::vector<Type> argTys;
  for (const auto &a : n.args)
    argTys.push_back(checkExpr(*a));
  for (const auto &t : argTys) {
    if (t.isError())
      return Type::error(); // tanpa error beruntun
  }
  auto arity = [&](size_t want) -> bool {
    if (n.args.size() != want) {
      errorAt(loc, "wrong number of arguments for '" + n.callee +
                       "': expected " + std::to_string(want) + ", found " +
                       std::to_string(n.args.size()),
                "", static_cast<int>(n.callee.size()));
      return false;
    }
    return true;
  };
  // Pastikan argumen posisi i bertipe kind (pesan seragam).
  auto expect = [&](size_t i, TypeKind kind, const char *need) -> bool {
    if (argTys[i].kind != kind) {
      errorAt(n.args[i]->loc(),
              "'" + n.callee + "' requires " + need + ", found '" +
                  argTys[i].name() + "'");
      return false;
    }
    return true;
  };
  const std::string &c = n.callee;
  if (c == "input") {
    if (!arity(0))
      return Type::error();
    return Type::string();
  }
  if (c == "assert") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::Bool, "a 'bool' argument"))
      return Type::error();
    return Type::void_();
  }
  if (c == "str_len" || c == "parse_int" || c == "file_read" ||
      c == "file_exists" || c == "parse_int_valid") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::String, "a 'string' argument"))
      return Type::error();
    if (c == "str_len" || c == "parse_int")
      return Type::i64();
    if (c == "file_exists" || c == "parse_int_valid")
      return Type::boolean();
    return Type::string(); // file_read
  }
  if (c == "int_to_str") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' argument"))
      return Type::error();
    return Type::string();
  }
  // Kriptografi (Phase 76, RFC-0011 slice 1): sha256 string->string,
  // rand hex i64->string (keduanya bisa "" — lihat docs/crypto-2.27.md).
  if (c == "sha256_hex") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::String, "a 'string' argument"))
      return Type::error();
    return Type::string();
  }
  if (c == "crypto_rand_hex") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' argument"))
      return Type::error();
    return Type::string();
  }
  if (c == "str_concat" || c == "file_write" || c == "str_eq") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::String, "a 'string' argument") ||
        !expect(1, TypeKind::String, "a 'string' argument"))
      return Type::error();
    if (c == "file_write")
      return Type::boolean();
    if (c == "str_eq")
      return Type::boolean();
    return Type::string(); // str_concat
  }
  if (c == "array_sum" || c == "array_min" || c == "array_max") {
    if (!arity(1))
      return Type::error();
    if (argTys[0].kind != TypeKind::Array || !argTys[0].elem ||
        argTys[0].elem->kind != TypeKind::I64) {
      errorAt(n.args[0]->loc(),
              "'" + c + "' requires an '[i64]' argument, found '" +
                  argTys[0].name() + "'");
      return Type::error();
    }
    return Type::i64();
  }
  // Concurrency (Phase 28, v1.9.0): handle = i64 opak (pointer heap
  // runtime); thread entry = `fn() -> i64` (trampolin pair).
  if (c == "thread_spawn") {
    if (!arity(1))
      return Type::error();
    if (argTys[0].kind != TypeKind::Func || !argTys[0].ret ||
        !argTys[0].params.empty() ||
        argTys[0].ret->kind != TypeKind::I64) {
      errorAt(n.args[0]->loc(), "'thread_spawn' requires a 'fn() -> i64' "
                                "argument, found '" +
                                    argTys[0].name() + "'");
      return Type::error();
    }
    return Type::i64();
  }
  if (c == "thread_join" || c == "chan_recv" || c == "atomic_get" ||
      c == "mutex_try_lock" || c == "chan_len") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument"))
      return Type::error();
    return Type::i64();
  }
  if (c == "mutex_new" || c == "chan_new" || c == "atomic_new") {
    if (c == "atomic_new") {
      if (!arity(1))
        return Type::error();
      if (!expect(0, TypeKind::I64, "an 'i64' initial value"))
        return Type::error();
    } else if (!arity(0)) {
      return Type::error();
    }
    return Type::i64();
  }
  // Backpressure (Phase 118, v3.19.0): bounded + try_send i64.
  if (c == "chan_new_bounded") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' capacity argument"))
      return Type::error();
    return Type::i64();
  }
  if (c == "chan_try_send") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument") ||
        !expect(1, TypeKind::I64, "an 'i64' value argument"))
      return Type::error();
    return Type::boolean();
  }
  // Channel bertag 2.0 (bool/string): bertipe statis di checker,
  // diverifikasi dinamis di runtime (tag handle).
  if (c == "chan_new_bool" || c == "chan_new_str") {
    if (!arity(0))
      return Type::error();
    return Type::i64();
  }
  if (c == "chan_recv_bool") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument"))
      return Type::error();
    return Type::boolean();
  }
  if (c == "chan_recv_str") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument"))
      return Type::error();
    return Type::string();
  }
  if (c == "chan_send_bool") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument") ||
        !expect(1, TypeKind::Bool, "a 'bool' value argument"))
      return Type::error();
    return Type::void_();
  }
  if (c == "chan_send_str") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument") ||
        !expect(1, TypeKind::String, "a 'string' value argument"))
      return Type::error();
    return Type::void_();
  }
  if (c == "mutex_lock" || c == "mutex_unlock" || c == "mutex_free" ||
      c == "chan_free" || c == "atomic_free" || c == "sleep_ms") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument"))
      return Type::error();
    return Type::void_();
  }
  if (c == "chan_send" || c == "atomic_set") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument") ||
        !expect(1, TypeKind::I64, "an 'i64' value argument"))
      return Type::error();
    return Type::void_();
  }
  if (c == "atomic_add") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' handle argument") ||
        !expect(1, TypeKind::I64, "an 'i64' delta argument"))
      return Type::error();
    return Type::i64();
  }
  // Stdlib 2.0 (Phase 29, v1.10.0): string/time/random/process/net.
  if (c == "str_char_at") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::String, "a 'string' argument") ||
        !expect(1, TypeKind::I64, "an 'i64' index argument"))
      return Type::error();
    return Type::string();
  }
  if (c == "str_slice") {
    if (!arity(3))
      return Type::error();
    if (!expect(0, TypeKind::String, "a 'string' argument") ||
        !expect(1, TypeKind::I64, "an 'i64' start argument") ||
        !expect(2, TypeKind::I64, "an 'i64' end argument"))
      return Type::error();
    return Type::string();
  }
  if (c == "time_now_ms" || c == "time_now_s" || c == "random_u64" ||
      c == "process_argc" || c == "process_cwd") {
    if (!arity(0))
      return Type::error();
    if (c == "process_cwd")
      return Type::string();
    return Type::i64();
  }
  if (c == "process_exit") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' exit code"))
      return Type::error();
    return Type::void_();
  }
  if (c == "process_env" || c == "process_argv") {
    if (!arity(1))
      return Type::error();
    if (c == "process_env") {
      if (!expect(0, TypeKind::String, "a 'string' argument"))
        return Type::error();
    } else if (!expect(0, TypeKind::I64, "an 'i64' index argument")) {
      return Type::error();
    }
    return Type::string();
  }
  if (c == "tcp_connect") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::String, "a 'string' host argument") ||
        !expect(1, TypeKind::I64, "an 'i64' port argument"))
      return Type::error();
    return Type::i64();
  }
  if (c == "tcp_send") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' socket argument") ||
        !expect(1, TypeKind::String, "a 'string' data argument"))
      return Type::error();
    return Type::boolean();
  }
  if (c == "tcp_recv") {
    if (!arity(2))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' socket argument") ||
        !expect(1, TypeKind::I64, "an 'i64' max-length argument"))
      return Type::error();
    return Type::string();
  }
  if (c == "tcp_close") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' socket argument"))
      return Type::error();
    return Type::void_();
  }
  // TCP server 2.0 (loopback-only; lihat nusa_tcp_listen).
  if (c == "tcp_listen") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' port argument"))
      return Type::error();
    return Type::i64();
  }
  if (c == "tcp_accept") {
    if (!arity(1))
      return Type::error();
    if (!expect(0, TypeKind::I64, "an 'i64' socket argument"))
      return Type::error();
    return Type::i64();
  }
  return Type::error(); // tak terjangkau (semantic menjamin builtin)
}

Type TypeChecker::checkCallArgs(const Expr::Call &n, const Location &loc,
                                const std::vector<Type> &params,
                                const Type &ret, const std::string &display) {
  bool arityOk = true;
  if (n.args.size() != params.size()) {
    errorAt(loc, "wrong number of arguments for '" + display +
                     "': expected " + std::to_string(params.size()) +
                     ", found " + std::to_string(n.args.size()),
            "", static_cast<int>(display.size()));
    arityOk = false;
  }
  // Tetap cek tiap argumen yang ada agar error lain ikut terlapor;
  // bandingkan tipe hanya untuk posisi yang valid.
  for (size_t i = 0; i < n.args.size(); i++) {
    // Lambda (Phase 27): propagasi tipe formal SEBELUM cek argumen.
    if (arityOk && i < params.size() && params[i].kind == TypeKind::Func)
      expectLambda(*n.args[i], params[i]);
    Type argTy = checkExpr(*n.args[i]);
    if (arityOk && i < params.size() && !argTy.isError() &&
        argTy != params[i]) {
      const Location &aloc = n.args[i]->loc();
      errorAt(aloc, "mismatched argument type for '" + display +
                        "': parameter " + std::to_string(i + 1) +
                        " expects '" + params[i].name() + "', found '" +
                        argTy.name() + "'");
    }
  }
  if (!arityOk)
    return Type::error();
  return ret;
}

Type TypeChecker::substSelf(const Type &ty, const Type &target) {
  if (ty.kind == TypeKind::Struct && ty.sname && *ty.sname == "self" &&
      ty.sargs.empty())
    return target;
  if (ty.kind == TypeKind::Array && ty.elem) {
    Type e = substSelf(*ty.elem, target);
    if (e.isError())
      return Type::error();
    return Type::array(e);
  }
  if (ty.kind == TypeKind::Func && ty.ret) {
    std::vector<Type> ps;
    for (const auto &p : ty.params)
      ps.push_back(substSelf(p, target));
    Type r = substSelf(*ty.ret, target);
    if (r.isError())
      return Type::error();
    return Type::func(std::move(ps), r);
  }
  if (ty.kind == TypeKind::Struct && ty.sname && !ty.sargs.empty()) {
    std::vector<Type> as;
    for (const auto &a : ty.sargs) {
      Type c = substSelf(a, target);
      if (c.isError())
        return Type::error();
      as.push_back(c);
    }
    return Type::applied(*ty.sname, std::move(as));
  }
  return ty;
}

std::string TypeChecker::methodTypeKey(const Type &ty) {
  switch (ty.kind) {
  case TypeKind::I64: return "i64";
  case TypeKind::Bool: return "bool";
  case TypeKind::String: return "string";
  case TypeKind::Struct:
    if (ty.sname && !ty.sargs.empty())
      return mangleStruct(*ty.sname, ty.sargs);
    return ty.sname ? *ty.sname : "";
  default: return "";
  }
}

std::string TypeChecker::mangleMethod(const std::string &trait,
                                      const std::string &method,
                                      const Type &target) {
  return trait + "_" + method + "_" + mangleType(target);
}

bool TypeChecker::typeImplementsTrait(const Type &ty,
                                      const std::string &trait) {
  std::string key = methodTypeKey(ty);
  if (key.empty())
    return false;
  auto it = typeTraits_.find(key);
  return it != typeTraits_.end() && it->second.count(trait);
}

Type TypeChecker::checkMethodCall(Expr::MethodCall &n, Expr &owner,
                                  const Location &loc) {
  // Konstruktor enum `E.V(args)` (Phase 26): receiver nama-tipe, bukan
  // nilai — tulis ulang menjadi StructLit hidden struct.
  if (const auto *vr = std::get_if<Expr::VarRef>(&n.receiver->node)) {
    if (enums_.count(vr->name)) {
      std::vector<std::unique_ptr<Expr>> args;
      args.reserve(n.args.size());
      for (auto &a : n.args)
        args.push_back(std::move(a));
      return checkEnumCtor(vr->name, n.method, std::move(args),
                           std::move(n.targs), owner, loc);
    }
  }
  // Resolusi metode statis (Phase 22): (tipe receiver, nama).
  Type recvTy = checkExpr(*n.receiver);
  if (recvTy.isError())
    return Type::error();
  if (containsParam(recvTy)) {
    errorAt(n.loc, "cannot resolve method '" + n.method +
                       "' on generic type '" + recvTy.name() + "'",
            "methods resolve on concrete types; add a trait "
            "bound and call through an instance",
            static_cast<int>(n.method.size()));
    return Type::error();
  }
  std::string key = methodTypeKey(recvTy);
  std::string display = recvTy.name() + "." + n.method;
  if (key.empty()) {
    errorAt(n.loc, "cannot call method '" + n.method + "' on type '" +
                       recvTy.name() + "'",
            "methods are defined on structs and primitives via impl blocks",
            static_cast<int>(n.method.size()));
    return Type::error();
  }
  auto mit = methodImpls_.find({key, n.method});
  if (mit == methodImpls_.end()) {
    std::string hint2;
    auto tmit = typeMethods_.find(key);
    if (tmit != typeMethods_.end() && !tmit->second.empty()) {
      hint2 = "type '" + recvTy.name() + "' has methods: ";
      for (size_t i = 0; i < tmit->second.size(); i++) {
        if (i)
          hint2 += ", ";
        hint2 += tmit->second[i];
      }
    } else {
      hint2 =
          "no methods are implemented for type '" + recvTy.name() + "'";
    }
    errorAt(n.loc, "unknown method '" + n.method + "' for type '" +
                       recvTy.name() + "'",
            hint2, static_cast<int>(n.method.size()));
    return Type::error();
  }
  if (mit->second.size() > 1) {
    std::string traits;
    for (size_t i = 0; i < mit->second.size(); i++) {
      if (i)
        traits += ", ";
      traits += mit->second[i].trait;
    }
    errorAt(n.loc, "ambiguous method '" + n.method + "' for type '" +
                       recvTy.name() + "'",
            "implemented by traits: " + traits,
            static_cast<int>(n.method.size()));
    return Type::error();
  }
  const MethodImpl &mi = mit->second[0];
  // Privacy `_` (cermin fungsi): lintas-file ditolak.
  if (!n.method.empty() && n.method[0] == '_') {
    if (mi.loc.file != n.loc.file) {
      errorAt(n.loc, "'" + n.method + "' is private to module '" +
                         mi.loc.file + "'",
              "names starting with '_' are module-private",
              static_cast<int>(n.method.size()));
      return Type::error();
    }
  }
  auto sigIt = sigs_.find(mi.mangled);
  if (sigIt == sigs_.end())
    return Type::error(); // defensif; seharusnya terdaftar
  // Tulis ulang menjadi panggilan langsung (dispatch statis).
  Expr::Call dc;
  dc.callee = mi.mangled;
  dc.loc = n.loc;
  dc.args.push_back(std::move(n.receiver));
  for (auto &a : n.args)
    dc.args.push_back(std::move(a));
  Type ret = checkCallArgs(dc, loc, sigIt->second.params, sigIt->second.ret,
                           display);
  if (ret.isError())
    return Type::error();
  owner.node = std::move(dc);
  return ret;
}

bool TypeChecker::checkBoundsSatisfied(
    const std::string &ownerKind, const std::string &ownerName,
    const Location &loc, const std::vector<std::string> &tparams,
    const std::unordered_map<std::string, std::vector<std::string>> &bounds,
    const std::unordered_map<std::string, Type> &sub) {
  for (const auto &tp : tparams) {
    auto bit = bounds.find(tp);
    if (bit == bounds.end())
      continue;
    auto sit = sub.find(tp);
    if (sit == sub.end())
      continue; // unbound — dilaporkan pemanggil
    if (containsParam(sit->second))
      continue; // rigid — tak dapat diputuskan di sini
    for (const auto &b : bit->second) {
      if (!typeImplementsTrait(sit->second, b)) {
        errorAt(loc, "type '" + sit->second.name() +
                         "' does not implement trait '" + b + "' in '" +
                         ownerName + "'",
                "add 'impl " + b + " for " + sit->second.name() + " { ... }'");
        return false;
      }
    }
  }
  return true;
}

void TypeChecker::buildTraitSig(const TraitDecl &td) {
  if (traitSigs_.count(td.name))
    return; // duplikat — sudah dilaporkan semantic; diam
  TraitSig ts;
  for (const auto &m : td.methods) {
    if (!m.tparams.empty()) {
      errorAt(m.loc, "generic methods are not supported",
              "remove '<...>' from method '" + m.name + "'");
      continue;
    }
    if (m.params.empty() || m.params[0].name != "self") {
      errorAt(m.loc, "first parameter of method '" + m.name +
                         "' must be 'self'",
              "write 'fn " + m.name + "(self, ...)'");
      continue;
    }
    if (m.params.empty() || m.params[0].name != "self") {
      errorAt(m.loc, "first parameter of method '" + m.name +
                         "' must be 'self'",
              "write 'fn " + m.name + "(self, ...)'");
      continue;
    }
    TraitMethodSig ms;
    ms.params.push_back(Type::named("self")); // marker (validasi di atas)
    for (size_t i = 1; i < m.params.size(); i++) {
      Type pt = resolveType(m.params[i].type, m.params[i].loc);
      if (pt.isError())
        break;
      ms.params.push_back(pt);
    }
    if (ms.params.size() != m.params.size())
      continue;
    Type rt = resolveType(m.retType, m.loc);
    if (rt.isError())
      continue;
    ms.ret = rt;
    ts.methods[m.name] = std::move(ms);
  }
  traitSigs_[td.name] = std::move(ts);
}

void TypeChecker::processImplBlock(const ImplBlock &ib) {
  auto tit = traitSigs_.find(ib.trait);
  if (tit == traitSigs_.end())
    return; // unknown trait — sudah dilaporkan semantic; diam
  // Resolve target (harus konkret nominal: struct/primitif).
  Type target = resolveType(ib.target, ib.loc);
  if (target.isError())
    return;
  bool nominalOk = false;
  if (target.kind == TypeKind::Struct)
    nominalOk = true;
  else if (target.kind == TypeKind::I64 ||
           target.kind == TypeKind::Bool ||
           target.kind == TypeKind::String)
    nominalOk = true;
  if (!nominalOk) {
    errorAt(ib.loc, "cannot implement trait '" + ib.trait + "' for '" +
                        target.name() + "'",
            "implement traits for structs and primitive types");
    return;
  }
  std::string typeKey = methodTypeKey(target);
  // Duplikat impl (trait + tipe sama)?
  auto dupIt = typeTraits_.find(typeKey);
  if (dupIt != typeTraits_.end() && dupIt->second.count(ib.trait)) {
    errorAt(ib.loc, "duplicate impl of '" + ib.trait + "' for '" +
                        target.name() + "'",
            "each trait may be implemented once per type");
    return;
  }
  const TraitSig &tsig = tit->second;
  std::unordered_set<std::string> implemented;
  for (const auto &m : ib.methods) {
    if (!m.tparams.empty()) {
      errorAt(m.loc, "generic methods are not supported",
              "remove '<...>' from method '" + m.name + "'");
      continue;
    }
    if (!m.tparams.empty()) {
      errorAt(m.loc, "generic methods are not supported",
              "remove '<...>' from method '" + m.name + "'");
      continue;
    }
    if (m.params.empty() || m.params[0].name != "self") {
      errorAt(m.loc, "first parameter of method '" + m.name +
                         "' must be 'self'",
              "write 'fn " + m.name + "(self, ...)'");
      continue;
    }
    auto tmit = tsig.methods.find(m.name);
    if (tmit == tsig.methods.end()) {
      errorAt(m.loc, "method '" + m.name + "' is not a member of trait '" +
                         ib.trait + "'",
              "add it to the trait or rename the method");
      continue;
    }
    implemented.insert(m.name); // ada (walau konformansi bisa gagal)
    // Konformansi: substitusi self → target lalu bandingkan.
    const TraitMethodSig &want = tmit->second;
    std::vector<Type> wantParams;
    for (const auto &wp : want.params)
      wantParams.push_back(substSelf(wp, target));
    Type wantRet = substSelf(want.ret, target);
    std::vector<Type> gotParams;
    bool paramsOk = true;
    for (const auto &p : m.params) {
      Type pt;
      if (p.name == "self") {
        pt = target; // receiver = tipe target (parser: hanya posisi 1)
      } else {
        pt = resolveType(p.type, p.loc);
        if (pt.isError()) {
          paramsOk = false;
          break;
        }
      }
      gotParams.push_back(pt);
    }
    Type gotRet = resolveType(m.retType, m.loc);
    if (!paramsOk || gotRet.isError())
      continue;
    if (gotParams.size() != wantParams.size()) {
      errorAt(m.loc, "method '" + m.name + "' has wrong arity: trait '" +
                         ib.trait + "' declares " +
                         std::to_string(wantParams.size()) +
                         " parameters, impl has " +
                         std::to_string(gotParams.size()),
              "");
      continue;
    }
    bool match = (gotRet == wantRet);
    size_t badIdx = 0;
    for (size_t i = 0; match && i < gotParams.size(); i++) {
      if (gotParams[i] != wantParams[i]) {
        match = false;
        badIdx = i;
      }
    }
    if (!match) {
      if (gotRet != wantRet) {
        errorAt(m.loc, "method '" + m.name + "' mismatches trait '" +
                           ib.trait + "': return expects '" +
                           wantRet.name() + "', found '" + gotRet.name() +
                           "'");
      } else {
        errorAt(m.params[badIdx].loc,
                "method '" + m.name + "' mismatches trait '" + ib.trait +
                    "': parameter " + std::to_string(badIdx + 1) +
                    " expects '" + wantParams[badIdx].name() + "', found '" +
                    gotParams[badIdx].name() + "'");
      }
      continue;
    }
    // Bangun decl mangled → pendingMethods_ + registry + sigs_.
    std::string mangled = mangleMethod(ib.trait, m.name, target);
    if (funcNames_.count(mangled) || sigs_.count(mangled) ||
        genericSigs_.count(mangled)) {
      errorAt(m.loc, "cannot define method '" + m.name + "': name '" +
                         mangled + "' is already declared",
              "rename the conflicting function");
      continue;
    }
    FunctionDecl inst;
    inst.name = mangled;
    inst.loc = m.loc;
    for (size_t i = 0; i < m.params.size(); i++) {
      Param np;
      np.name = m.params[i].name;
      np.loc = m.params[i].loc;
      // Codegen membaca anotasi mentah: petakan aplikasi ke mangled.
      np.type = mapAppToMangled((i == 0) ? target : gotParams[i]);
      inst.params.push_back(np);
    }
    inst.retType = mapAppToMangled(gotRet);
    inst.body = cloneBlock(m.body);
    Signature sig;
    sig.params = gotParams;
    sig.ret = gotRet;
    sigs_[mangled] = sig;
    pendingMethods_.push_back(std::move(inst));
    MethodImpl mi;
    mi.trait = ib.trait;
    mi.mangled = mangled;
    mi.loc = m.loc;
    methodImpls_[{typeKey, m.name}].push_back(std::move(mi));
    if (std::find(typeMethods_[typeKey].begin(), typeMethods_[typeKey].end(),
                  m.name) == typeMethods_[typeKey].end())
      typeMethods_[typeKey].push_back(m.name);
    typeTraits_[typeKey].insert(ib.trait);
    implemented.insert(m.name);
  }
  // Metode trait yang belum diimplementasikan?
  for (const auto &kv : tsig.methods) {
    if (!implemented.count(kv.first)) {
      errorAt(ib.loc, "missing method '" + kv.first + "' for trait '" +
                         ib.trait + "'",
              "implement 'fn " + kv.first + "(self, ...)' in this impl block");
    }
  }
}

// Konversi anotasi mentah template generik: nama yang cocok parameter
// tipe → Param; sisanya di-resolve normal (unknown → error).
Type TypeChecker::toFormalType(const Type &ty,
                               const std::vector<std::string> &tparams,
                               const Location &loc) {
  if (ty.kind == TypeKind::Struct && ty.sname) {
    bool isTp = false;
    for (const auto &tp : tparams) {
      if (*ty.sname == tp) {
        isTp = true;
        break;
      }
    }
    if (isTp) {
      if (!ty.sargs.empty()) {
        errorAt(loc, "type parameter '" + *ty.sname +
                         "' cannot take type arguments",
                "use a plain name, e.g. '" + *ty.sname + "'");
        return Type::error();
      }
      return Type::param(*ty.sname);
    }
    // Aplikasi generik (`Point<T>`): konversi argumen lalu resolve
    // penuh (validasi head/arity + instansiasi bila konkret).
    std::vector<Type> as;
    for (const auto &a : ty.sargs) {
      Type c = toFormalType(a, tparams, loc);
      if (c.isError())
        return Type::error();
      as.push_back(c);
    }
    return resolveType(Type::applied(*ty.sname, std::move(as)), loc);
  }
  if (ty.kind == TypeKind::Array && ty.elem) {
    Type e = toFormalType(*ty.elem, tparams, loc);
    if (e.isError())
      return Type::error();
    return Type::array(e);
  }
  if (ty.kind == TypeKind::Func && ty.ret) {
    std::vector<Type> ps;
    for (const auto &p : ty.params) {
      Type c = toFormalType(p, tparams, loc);
      if (c.isError())
        return Type::error();
      ps.push_back(c);
    }
    Type r = toFormalType(*ty.ret, tparams, loc);
    if (r.isError())
      return Type::error();
    return Type::func(std::move(ps), r);
  }
  return resolveType(ty, loc);
}

void TypeChecker::registerStructTemplate(StructDecl &sd) {
  // Pertahanan bila semantic dilewati (normalnya sudah ditolak di sana).
  if (structs_.count(sd.name) || structTemplates_.count(sd.name)) {
    errorAt(sd.loc, "duplicate struct '" + sd.name + "'", "");
    return;
  }
  // Daftarkan DULU agar self-reference (`tail: L<T>` di dalam `L<T>`)
  // dikenali sebagai template (rigid), bukan unknown.
  structTemplates_[sd.name] = &sd;
  bool ok = true;
  for (auto &f : sd.fields) {
    Type ft = toFormalType(f.type, sd.tparams, f.loc);
    if (ft.isError()) {
      ok = false;
      continue; // sudah dilaporkan; field rusak dilewati
    }
    f.type = ft; // kanonikalisasi (tparam-named → Param)
  }
  // Duplikat field (cermin pass 1a; dihitung terpisah agar sekali lapor).
  std::unordered_set<std::string> seen;
  for (const auto &f : sd.fields) {
    if (!seen.insert(f.name).second) {
      errorAt(f.loc, "duplicate field '" + f.name + "' in struct '" +
                         sd.name + "'");
      ok = false;
    }
  }
  if (!ok)
    structBad_.insert(sd.name);
}

void TypeChecker::buildGenericSig(FunctionDecl &fn) {
  // Pertahanan bila semantic dilewati (normalnya sudah ditolak di sana).
  if (sigs_.count(fn.name) || genericSigs_.count(fn.name)) {
    errorAt(fn.loc, "duplicate function '" + fn.name + "'", "");
    return;
  }
  GenericSig gs;
  gs.tparams = fn.tparams;
  gs.tmpl = &fn;
  gs.bounds = fn.tparamBounds;
  bool ok = true;
  // Batasan trait harus dikenal (Phase 22; normalnya dicek semantic).
  for (const auto &tp : fn.tparams) {
    auto bit = fn.tparamBounds.find(tp);
    if (bit == fn.tparamBounds.end())
      continue;
    for (const auto &b : bit->second) {
      if (!traitSigs_.count(b)) {
        errorAt(fn.loc, "unknown trait '" + b + "' in bound for '" + tp +
                            "' in '" + fn.name + "'",
                "declare 'trait " + b + " { ... }' first");
        ok = false;
      }
    }
  }
  for (auto &param : fn.params) {
    Type ft = toFormalType(param.type, fn.tparams, param.loc);
    if (ft.isError()) {
      ok = false;
      gs.params.push_back(Type::error());
      continue;
    }
    param.type = ft; // kanonikalisasi (tparam-named → Param)
    gs.params.push_back(ft);
  }
  Type rt = toFormalType(fn.retType, fn.tparams, fn.loc);
  if (rt.isError()) {
    ok = false;
  } else {
    fn.retType = rt; // kanonikalisasi
  }
  gs.ret = rt;
  gs.ok = ok;
  if (!ok)
    gs.ret = Type::error();
  genericSigs_[fn.name] = std::move(gs);
}

bool TypeChecker::unifyType(const Type &formal, const Type &actual,
                            std::unordered_map<std::string, Type> &sub) {
  if (formal.kind == TypeKind::Param && formal.pname) {
    auto it = sub.find(*formal.pname);
    if (it == sub.end()) {
      sub[*formal.pname] = actual;
      return true;
    }
    return it->second == actual;
  }
  if (formal.kind == TypeKind::Array && actual.kind == TypeKind::Array &&
      formal.elem && actual.elem)
    return unifyType(*formal.elem, *actual.elem, sub);
  if (formal.kind == TypeKind::Func && actual.kind == TypeKind::Func &&
      formal.ret && actual.ret &&
      formal.params.size() == actual.params.size()) {
    for (size_t i = 0; i < formal.params.size(); i++) {
      if (!unifyType(formal.params[i], actual.params[i], sub))
        return false;
    }
    return unifyType(*formal.ret, *actual.ret, sub);
  }
  if (formal.kind == TypeKind::Struct && actual.kind == TypeKind::Struct &&
      formal.sname && actual.sname && *formal.sname == *actual.sname &&
      formal.sargs.size() == actual.sargs.size()) {
    for (size_t i = 0; i < formal.sargs.size(); i++) {
      if (!unifyType(formal.sargs[i], actual.sargs[i], sub))
        return false;
    }
    return true;
  }
  return formal == actual;
}

Type TypeChecker::substType(const Type &ty,
                            const std::unordered_map<std::string, Type> &sub) {
  if (ty.kind == TypeKind::Param && ty.pname) {
    auto it = sub.find(*ty.pname);
    if (it != sub.end())
      return it->second;
    return ty;
  }
  if (ty.kind == TypeKind::Array && ty.elem) {
    Type e = substType(*ty.elem, sub);
    if (e.isError())
      return Type::error();
    return Type::array(e);
  }
  if (ty.kind == TypeKind::Func && ty.ret) {
    std::vector<Type> ps;
    for (const auto &p : ty.params)
      ps.push_back(substType(p, sub));
    Type r = substType(*ty.ret, sub);
    if (r.isError())
      return Type::error();
    return Type::func(std::move(ps), r);
  }
  if (ty.kind == TypeKind::Struct && ty.sname && !ty.sargs.empty()) {
    std::vector<Type> as;
    for (const auto &a : ty.sargs) {
      Type c = substType(a, sub);
      if (c.isError())
        return Type::error();
      as.push_back(c);
    }
    return Type::applied(*ty.sname, std::move(as));
  }
  // Nama telanjang yang cocok tparam instance (Phase 45/B1):
  // posisi-tipe ekspresi (`E<T>.V()`, anotasi lambda) tak pernah
  // lewat toFormalType, sehingga tiba di sini sebagai Struct{T}.
  // Konsisten dengan deklarasi (tparam-named → tersubstitusi).
  if (ty.kind == TypeKind::Struct && ty.sname && ty.sargs.empty()) {
    auto it = sub.find(*ty.sname);
    if (it != sub.end())
      return it->second;
  }
  return ty;
}

void TypeChecker::substBlockTypes(
    Block &b, const std::unordered_map<std::string, Type> &sub) {
  for (auto &s : b.stmts)
    substStmtTypes(s, sub);
}

void TypeChecker::substStmtTypes(
    Stmt &s, const std::unordered_map<std::string, Type> &sub) {
  if (auto *ls = std::get_if<LetStmt>(&s)) {
    if (ls->declaredType)
      ls->declaredType = substType(*ls->declaredType, sub);
    substExprTypes(*ls->value, sub);
    return;
  }
  if (auto *ps = std::get_if<PrintStmt>(&s)) {
    substExprTypes(*ps->arg, sub);
    return;
  }
  if (auto *rs = std::get_if<ReturnStmt>(&s)) {
    if (rs->value)
      substExprTypes(*rs->value, sub);
    return;
  }
  if (auto *es = std::get_if<ExprStmt>(&s)) {
    substExprTypes(*es->expr, sub);
    return;
  }
  if (auto *is = std::get_if<IfStmt>(&s)) {
    substExprTypes(*is->cond, sub);
    substBlockTypes(*is->thenBlock, sub);
    if (is->elseBlock)
      substBlockTypes(*is->elseBlock, sub);
    return;
  }
  if (auto *ws = std::get_if<WhileStmt>(&s)) {
    substExprTypes(*ws->cond, sub);
    substBlockTypes(*ws->body, sub);
    return;
  }
  if (auto *as = std::get_if<AssignStmt>(&s)) {
    substExprTypes(*as->value, sub);
    return;
  }
  if (auto *ms = std::get_if<MatchStmt>(&s)) {
    substExprTypes(*ms->subject, sub);
    for (auto &arm : ms->arms) {
      substPatTypes(arm.pat, sub);
      if (arm.guard)
        substExprTypes(*arm.guard, sub);
      substBlockTypes(*arm.body, sub);
    }
    return;
  }
}

void TypeChecker::substPatTypes(
    Pat &p, const std::unordered_map<std::string, Type> &sub) {
  if (auto *sp = std::get_if<Pat::Struct>(&p.node)) {
    for (auto &t : sp->targs)
      t = substType(t, sub);
    for (auto &f : sp->fields)
      substPatTypes(*f.pat, sub);
    return;
  }
  if (auto *vp = std::get_if<Pat::Variant>(&p.node)) {
    for (auto &a : vp->args)
      substPatTypes(*a, sub);
    return;
  }
}

void TypeChecker::substExprTypes(
    Expr &e, const std::unordered_map<std::string, Type> &sub) {
  // Cermin cloneExpr (AST.cpp): setiap varian dengan anak/type.
  if (auto *n = std::get_if<Expr::Call>(&e.node)) {
    for (auto &a : n->args)
      substExprTypes(*a, sub);
  } else if (auto *n = std::get_if<Expr::MethodCall>(&e.node)) {
    substExprTypes(*n->receiver, sub);
    for (auto &t : n->targs)
      t = substType(t, sub);
    for (auto &a : n->args)
      substExprTypes(*a, sub);
  } else if (auto *n = std::get_if<Expr::Propagate>(&e.node)) {
    substExprTypes(*n->expr, sub);
  } else if (auto *n = std::get_if<Expr::ArrayLit>(&e.node)) {
    for (auto &el : n->elems)
      substExprTypes(*el, sub);
  } else if (auto *n = std::get_if<Expr::Index>(&e.node)) {
    substExprTypes(*n->base, sub);
    substExprTypes(*n->index, sub);
  } else if (auto *n = std::get_if<Expr::StructLit>(&e.node)) {
    for (auto &t : n->targs)
      t = substType(t, sub);
    for (auto &f : n->fields)
      substExprTypes(*f.value, sub);
  } else if (auto *n = std::get_if<Expr::FieldAccess>(&e.node)) {
    substExprTypes(*n->base, sub);
  } else if (auto *n = std::get_if<Expr::Unary>(&e.node)) {
    substExprTypes(*n->operand, sub);
  } else if (auto *n = std::get_if<Expr::Binary>(&e.node)) {
    substExprTypes(*n->lhs, sub);
    substExprTypes(*n->rhs, sub);
  } else if (auto *n = std::get_if<Expr::Lambda>(&e.node)) {
    for (auto &lp : n->params) {
      if (lp.annot)
        lp.annot = substType(*lp.annot, sub);
    }
    substBlockTypes(*n->body, sub);
  } else if (auto *n = std::get_if<Expr::Closure>(&e.node)) {
    for (auto &c : n->captures)
      substExprTypes(*c, sub);
    n->funcType = substType(n->funcType, sub);
  }
  // Literal/VarRef: tanpa tipe/anak — nothing to do.
}

std::string TypeChecker::mangleType(const Type &ty) {
  switch (ty.kind) {
  case TypeKind::I64: return "i64";
  case TypeKind::Bool: return "bool";
  case TypeKind::String: return "string";
  case TypeKind::Void: return "void";
  case TypeKind::Array:
    return "arr_" + (ty.elem ? mangleType(*ty.elem) : "e");
  case TypeKind::Func: {
    std::string s = "fn";
    for (const auto &p : ty.params)
      s += "_" + mangleType(p);
    s += "_r_" + (ty.ret ? mangleType(*ty.ret) : "e");
    return s;
  }
  case TypeKind::Struct: {
    std::string s = ty.sname ? *ty.sname : "u";
    for (const auto &a : ty.sargs)
      s += "_" + mangleType(a);
    return s;
  }
  case TypeKind::Param: return "t_" + (ty.pname ? *ty.pname : "u");
  case TypeKind::Error: return "e";
  }
  return "e";
}

std::string TypeChecker::mangleInstance(
    const std::string &name, const std::vector<std::string> &tparams,
    const std::unordered_map<std::string, Type> &sub) {
  std::string out = name;
  for (const auto &tp : tparams) {
    auto it = sub.find(tp);
    out += "_" + (it == sub.end() ? "u" : mangleType(it->second));
  }
  return out;
}

bool TypeChecker::checkInstanceSig(FunctionDecl &inst, Signature &sigOut) {
  // Aturan sama seperti pass 1b (pesan identik), lokasi = template.
  // Tipe sudah konkret (tersubstitusi); tinggal resolve + validasi.
  // sigOut menerima tipe APLIKASI (cantik, untuk pesan + unify);
  // inst ditulis-balik MANGLED (untuk codegen).
  bool ok = true;
  for (auto &param : inst.params) {
    Type pt = resolveType(param.type, param.loc);
    if (pt.isError()) {
      ok = false;
      sigOut.params.push_back(Type::error());
      param.type = Type::error();
      continue;
    }
    sigOut.params.push_back(pt);
    param.type = mapAppToMangled(pt);
    if (pt.kind == TypeKind::Void) {
      errorAt(param.loc, "parameter '" + param.name +
                             "' cannot have type 'void'",
              "use 'i64', 'bool', or 'string'");
      ok = false;
    }
    if (pt.kind == TypeKind::Array) {
      errorAt(param.loc, "array parameters are not supported",
              "move the array logic inside the function");
      ok = false;
    }
  }
  Type rt = resolveType(inst.retType, inst.loc);
  if (rt.isError()) {
    ok = false;
    sigOut.ret = Type::error();
  } else {
    sigOut.ret = rt;
    inst.retType = mapAppToMangled(rt);
    if (rt.kind == TypeKind::Array) {
      errorAt(inst.loc, "array return types are not supported",
              "return an element or the length instead");
      ok = false;
    }
  }
  if (!ok) {
    sigOut.ret = Type::error();
  }
  return ok;
}

bool TypeChecker::containsParam(const Type &ty) {
  if (ty.kind == TypeKind::Param)
    return true;
  if (ty.kind == TypeKind::Array && ty.elem)
    return containsParam(*ty.elem);
  if (ty.kind == TypeKind::Func && ty.ret) {
    for (const auto &p : ty.params) {
      if (containsParam(p))
        return true;
    }
    return containsParam(*ty.ret);
  }
  if (ty.kind == TypeKind::Struct) {
    for (const auto &a : ty.sargs) {
      if (containsParam(a))
        return true;
    }
  }
  return false;
}

Type TypeChecker::instantiateStruct(const std::string &name,
                                    const std::vector<Type> &args,
                                    const Location &loc) {
  std::string mangled = mangleStruct(name, args);
  std::string pretty = Type::applied(name, args).name();
  // Cache milik sendiri (lihat structInstNames_); di bawah untuk baru.
  // Urutan penting: siklus dulu (error), lalu memo gagal (diam), lalu
  // sukses (pakai), lalu tabrakan user (error).
  if (instantiating_.count(mangled)) {
    errorAt(loc, "recursive struct '" + pretty + "' has infinite size",
            "remove the self-reference or restructure the type");
    return Type::error();
  }
  if (structBadInst_.count(mangled))
    return Type::error();
  if (structInstNames_.count(mangled))
    return Type::applied(name, args);
  // Tabrakan dengan deklarasi user (nama mangled dipakai orang)?
  if (structNames_.count(mangled)) {
    errorAt(loc, "cannot instantiate '" + pretty + "': name '" + mangled +
                     "' is already declared",
            "rename the struct '" + mangled + "'");
    return Type::error();
  }
  // Siklus = ukuran infinit (struct by-value; cermin v1.x yang menolak
  // struct rekursif). Tanpa pointer, tak ada siklus legal.
  if (instDepth_ >= kMaxInstDepth) {
    errorAt(loc, "generic instantiation too deep (over 64 levels)",
            "instantiation chain for '" + name +
                "' diverges; simplify the types");
    return Type::error();
  }
  const StructDecl *tmpl = structTemplates_[name];
  instantiating_.insert(mangled);
  instDepth_++;
  bool ok = true;
  std::unordered_map<std::string, Type> sub;
  for (size_t i = 0; i < tmpl->tparams.size() && i < args.size(); i++)
    sub[tmpl->tparams[i]] = args[i];
  // Batasan trait (Phase 22): argumen konkret harus memenuhi bounds.
  // (Di sini agar kegagalan ikut termemo di exit — tanpa duplikat.)
  if (!checkBoundsSatisfied("struct", name, loc, tmpl->tparams,
                            tmpl->tparamBounds, sub))
    ok = false;
  StructInfo info;
  StructDecl instDecl;
  instDecl.name = mangled;
  instDecl.loc = tmpl->loc;
  // Lewati pembangunan bila bounds gagal (hindari error beruntun);
  // kegagalan ikut termemo di exit.
  if (ok) {
    for (const auto &f : tmpl->fields) {
      Type ft = substType(f.type, sub);
      ft = resolveType(ft, f.loc);
      if (ft.isError()) {
        ok = false;
        continue;
      }
      if (ft.kind == TypeKind::Void) {
        errorAt(f.loc,
                "struct field '" + f.name + "' cannot have type 'void'");
        ok = false;
        continue;
      }
      if (ft.kind == TypeKind::Array) {
        errorAt(f.loc, "array fields are not supported",
                "store the array in a local variable, not in a struct");
        ok = false;
        continue;
      }
      if (ft.kind == TypeKind::Func) {
        errorAt(f.loc, "function fields are not supported",
                "store the function in a local variable, not in a struct");
        ok = false;
        continue;
      }
      info.fields.emplace_back(f.name, ft);
      StructField ff;
      ff.name = f.name;
      ff.type = mapAppToMangled(ft); // codegen: nama layout
      ff.loc = f.loc;
      instDecl.fields.push_back(ff);
    }
  }
  if (ok) {
    structs_[mangled] = std::move(info);
    structInstNames_.insert(mangled);
    // Wariskan flag hidden-enum (Phase 26) agar codegen/DocGen
    // mengenali instance generik enum.
    if (tmpl->hiddenEnum)
      instDecl.hiddenEnum = true;
    structInstDecls_.push_back(std::move(instDecl));
  } else {
    structBadInst_.insert(mangled);
  }
  instantiating_.erase(mangled);
  instDepth_--;
  if (!ok)
    return Type::error();
  return Type::applied(name, args);
}

Type TypeChecker::checkGenericStructLit(Expr::StructLit &n,
                                        const Location &loc) {
  const std::string orig = n.name;
  const StructDecl *tmpl = structTemplates_[orig];
  // Pembanding field (nama cantik = template asli, bukan mangled).
  auto mismatch = [&](const std::string &field, const Type &want,
                      const Type &got, const Location &aloc) {
    errorAt(aloc, "mismatched field type: '" + orig + "." + field +
                      "' expects '" + want.name() + "', found '" +
                      got.name() + "'");
  };
  auto wantOf = [&](const StructInfo &info,
                    const std::string &field) -> const Type * {
    for (const auto &pr : info.fields) {
      if (pr.first == field)
        return &pr.second;
    }
    return nullptr;
  };
  std::unordered_map<std::string, Type> sub;
  if (!n.targs.empty()) {
    // Eksplisit: resolve → instansiasi → rewrite → cek + bandingkan.
    if (n.targs.size() != tmpl->tparams.size()) {
      errorAt(n.loc, "wrong number of type arguments for '" + orig +
                         "': expected " +
                         std::to_string(tmpl->tparams.size()) + ", found " +
                         std::to_string(n.targs.size()),
              "");
      return Type::error();
    }
    for (size_t i = 0; i < n.targs.size(); i++) {
      Type r = resolveType(n.targs[i], n.loc);
      if (r.isError())
        return Type::error();
      sub[tmpl->tparams[i]] = r;
    }
    std::vector<Type> args;
    for (const auto &tp : tmpl->tparams)
      args.push_back(sub[tp]);
    Type app = instantiateStruct(orig, args, n.loc);
    if (app.isError())
      return Type::error();
    n.name = mangleStruct(orig, args);
    const StructInfo &info = structs_[n.name];
    for (auto &f : n.fields) {
      const Type *want = wantOf(info, f.name);
      Type got = checkExpr(*f.value);
      // Field tak dikenal/duplikat/hilang sudah dilaporkan semantic.
      if (!want || got.isError())
        continue;
      if (got != *want)
        mismatch(f.name, *want, got, f.value->loc());
    }
    return app;
  }
  // Inferensi: cek semua inits dulu (error bersarang tetap terlapor).
  // SATU-SATUNYA cek init (tanpa cek ulang pasca-rewrite — node berubah!).
  std::unordered_map<std::string, Type> initTys;
  bool initsOk = true;
  for (auto &f : n.fields) {
    Type t = checkExpr(*f.value);
    if (t.isError())
      initsOk = false;
    initTys[f.name] = t;
  }
  if (!initsOk)
    return Type::error();
  // Unifikasi field template yang dikenal vs nilai init.
  for (const auto &tf : tmpl->fields) {
    auto it = initTys.find(tf.name);
    if (it == initTys.end())
      continue; // hilang — semantic sudah lapor
    if (!unifyType(tf.type, it->second, sub)) {
      Type want = substType(tf.type, sub);
      auto fit = n.fields.begin();
      for (; fit != n.fields.end(); ++fit) {
        if (fit->name == tf.name)
          break;
      }
      const Location &aloc =
          (fit == n.fields.end()) ? n.loc : fit->value->loc();
      mismatch(tf.name, want, it->second, aloc);
      return Type::error();
    }
  }
  for (const auto &tp : tmpl->tparams) {
    if (!sub.count(tp)) {
      errorAt(n.loc, "cannot infer type argument '" + tp +
                         "' for struct '" + orig + "'",
              "write '" + orig + "<...>' with explicit types");
      return Type::error();
    }
  }
  std::vector<Type> args;
  for (const auto &tp : tmpl->tparams)
    args.push_back(sub[tp]);
  Type app = instantiateStruct(orig, args, n.loc);
  if (app.isError())
    return Type::error();
  n.name = mangleStruct(orig, args);
  // Bandingkan memakai tipe init yang SUDAH dicek (tanpa cek ulang).
  const StructInfo &info = structs_[n.name];
  for (auto &f : n.fields) {
    const Type *want = wantOf(info, f.name);
    auto git = initTys.find(f.name);
    if (!want || git == initTys.end() || git->second.isError())
      continue;
    if (git->second != *want)
      mismatch(f.name, *want, git->second, f.value->loc());
  }
  return app;
}

std::string TypeChecker::mangleStruct(const std::string &name,
                                        const std::vector<Type> &args) {
  std::string m = name;
  for (const auto &a : args)
    m += "_" + mangleType(a);
  return m;
}

std::string TypeChecker::structKey(const Type &ty) {
  if (ty.kind == TypeKind::Struct && ty.sname && !ty.sargs.empty())
    return mangleStruct(*ty.sname, ty.sargs);
  return ty.sname ? *ty.sname : "";
}

Type TypeChecker::mapAppToMangled(const Type &ty) {
  if (ty.kind == TypeKind::Struct && ty.sname && !ty.sargs.empty())
    return Type::named(mangleStruct(*ty.sname, ty.sargs));
  return ty;
}

Type TypeChecker::checkGenericCall(Expr::Call &n, const Location &loc) {
  const GenericSig &gs = genericSigs_[n.callee];
  if (!gs.ok)
    return Type::error(); // signature rusak (sudah dilaporkan); diam
  // 1. Cek argumen non-lambda (tipenya dibutuhkan inferensi; lambda
  // menyusul sesudah tparams terikat non-lambda — lihat 3b).
  std::vector<Type> actuals(n.args.size(), Type::error());
  std::vector<bool> isLam(n.args.size(), false);
  bool argsOk = true;
  for (size_t i = 0; i < n.args.size(); i++) {
    if (std::holds_alternative<Expr::Lambda>(n.args[i]->node)) {
      isLam[i] = true;
      continue;
    }
    Type t = checkExpr(*n.args[i]);
    if (t.isError())
      argsOk = false;
    actuals[i] = t;
  }
  if (!argsOk)
    return Type::error();
  // 2. Arity (pesan sama seperti panggilan biasa).
  if (n.args.size() != gs.params.size()) {
    errorAt(loc, "wrong number of arguments for '" + n.callee +
                     "': expected " + std::to_string(gs.params.size()) +
                     ", found " + std::to_string(actuals.size()),
            "", static_cast<int>(n.callee.size()));
    return Type::error();
  }
  // 3. Inferensi: unifikasi formal vs aktual (non-lambda dulu agar
  // tparams terikat sebelum lambda dicek).
  std::unordered_map<std::string, Type> sub;
  for (size_t i = 0; i < actuals.size(); i++) {
    if (isLam[i])
      continue;
    if (!unifyType(gs.params[i], actuals[i], sub)) {
      Type want = substType(gs.params[i], sub);
      const Location &aloc = n.args[i]->loc();
      errorAt(aloc, "mismatched argument type for '" + n.callee +
                        "': parameter " + std::to_string(i + 1) +
                        " expects '" + want.name() + "', found '" +
                        actuals[i].name() + "'");
      return Type::error();
    }
  }
  // 3b. Lambda (Phase 27): ekspektasi dari formal tersubstitusi, cek,
  // lalu unifikasi balik (return lambda ikut mengikat tparams).
  for (size_t i = 0; i < n.args.size(); i++) {
    if (!isLam[i])
      continue;
    Type exp = substType(gs.params[i], sub);
    if (exp.kind != TypeKind::Func || !exp.ret) {
      errorAt(n.args[i]->loc(), "mismatched argument type for '" +
                                    n.callee + "': parameter " +
                                    std::to_string(i + 1) + " expects '" +
                                    exp.name() + "', found lambda",
              "pass a lambda only where a 'fn' type is expected");
      return Type::error();
    }
    auto *lam = std::get_if<Expr::Lambda>(&n.args[i]->node);
    if (!lam || lam->bad)
      return Type::error(); // defensif
    if (lam->params.size() != exp.params.size()) {
      errorAt(lam->loc, "lambda expects " +
                            std::to_string(exp.params.size()) +
                            " parameter(s), found " +
                            std::to_string(lam->params.size()),
              "a lambda used here must match '" + exp.name() + "'");
      return Type::error();
    }
    lam->expectParams.clear();
    bool lamOk = true;
    for (size_t j = 0; j < lam->params.size(); j++) {
      if (lam->params[j].annot) {
        Type a = resolveType(*lam->params[j].annot, lam->params[j].loc);
        if (a.isError())
          return Type::error();
        if (!unifyType(exp.params[j], a, sub)) {
          errorAt(lam->params[j].loc,
                  "mismatched lambda parameter type: '" +
                      lam->params[j].name + "' is '" + a.name() +
                      "', expected '" + substType(exp.params[j], sub).name() +
                      "'",
                  "remove the annotation or match the expected type");
          return Type::error();
        }
        lam->expectParams.push_back(a);
      } else {
        if (containsParam(exp.params[j])) {
          errorAt(lam->params[j].loc,
                  "cannot infer type of lambda parameter '" +
                      lam->params[j].name + "'",
                  "annotate '|" + lam->params[j].name +
                      ": T|' or bind the type via other arguments");
          lamOk = false;
          break;
        }
        lam->expectParams.push_back(exp.params[j]);
      }
    }
    if (!lamOk)
      return Type::error();
    lam->expectRet = *exp.ret;
    lam->expected = true;
    Type at = checkExpr(*n.args[i]);
    if (at.isError())
      return Type::error();
    actuals[i] = at;
    if (!unifyType(gs.params[i], at, sub)) {
      Type want = substType(gs.params[i], sub);
      errorAt(n.args[i]->loc(), "mismatched argument type for '" +
                                    n.callee + "': parameter " +
                                    std::to_string(i + 1) + " expects '" +
                                    want.name() + "', found '" + at.name() +
                                    "'");
      return Type::error();
    }
  }
  for (const auto &tp : gs.tparams) {
    if (!sub.count(tp)) {
      errorAt(loc, "cannot infer type argument '" + tp + "' for '" +
                       n.callee + "'",
              "use '" + tp + "' in a parameter type so it can be inferred");
      return Type::error();
    }
  }
  // Batasan trait (Phase 22): tipe aktual harus mengimplementasikan.
  if (!checkBoundsSatisfied("function", n.callee, loc, gs.tparams,
                            gs.bounds, sub))
    return Type::error();
  // 4. Instansiasi + rewrite callee ke nama mangled.
  // (Unifikasi sukses ⟹ aktual cocok dengan formal tersubstitusi;
  // tak perlu cek ulang.)
  Type ret = instantiateGeneric(n.callee, loc, sub);
  if (ret.isError())
    return Type::error();
  n.callee = mangleInstance(n.callee, gs.tparams, sub);
  return ret;
}

Type TypeChecker::instantiateGeneric(
    const std::string &name, const Location &loc,
    const std::unordered_map<std::string, Type> &sub) {
  const GenericSig &gs = genericSigs_[name];
  std::string mangled = mangleInstance(name, gs.tparams, sub);
  // Cache (termasuk yang sedang dibuat → rekursi aman, seperti pass 1b/2).
  if (instantiated_.count(mangled))
    return substType(gs.ret, sub);
  // Tabrakan dengan fungsi user (Phase 21d; nama mangled dipakai orang)?
  if (funcNames_.count(mangled)) {
    errorAt(loc, "cannot instantiate '" + name + "<...>': name '" +
                     mangled + "' is already declared",
            "rename the function '" + mangled + "'");
    return Type::error();
  }
  if (instDepth_ >= kMaxInstDepth) {
    errorAt(loc, "generic instantiation too deep (over 64 levels)",
            "instantiation chain for '" + name +
                "' diverges; simplify the recursion");
    return Type::error();
  }
  instantiated_.insert(mangled);
  instDepth_++;
  // Template untuk kloning body (disimpan saat buildGenericSig;
  // stabil: prog.functions tak pernah realloc saat pass 2).
  const FunctionDecl *tmpl = gs.tmpl;
  Type ret = Type::error();
  if (!tmpl) {
    errorAt(loc, "internal error: generic template '" + name + "' not found",
            "");
  } else {
    FunctionDecl inst;
    inst.name = mangled;
    inst.loc = tmpl->loc;
    for (const auto &p : tmpl->params) {
      Param np;
      np.name = p.name;
      np.loc = p.loc;
      np.type = substType(p.type, sub);
      inst.params.push_back(np);
    }
    inst.retType = substType(tmpl->retType, sub);
    inst.body = cloneBlock(tmpl->body);
    substBlockTypes(inst.body, sub);
    Signature sig;
    if (checkInstanceSig(inst, sig))
      ret = substType(gs.ret, sub);
    else {
      // Gagal: signature Error (cegah error beruntun).
      sig.params.clear();
      for (size_t i = 0; i < inst.params.size(); i++)
        sig.params.push_back(Type::error());
      sig.ret = Type::error();
    }
    sigs_[mangled] = sig;
    if (!ret.isError())
      checkFunction(inst);
    instances_.push_back(std::move(inst));
  }
  instDepth_--;
  return ret;
}

Type TypeChecker::checkExpr(Expr &expr) {
  if (++exprDepth_ > kMaxExprDepth) {
    --exprDepth_;
    errorAt(expr.loc(), "expression too deeply nested",
            "simplify this nested expression");
    return Type::error();
  }
  const Location &loc = expr.loc();
  // Panggilan method (Phase 22): resolve + tulis ulang node menjadi
  // Call biasa (dispatch statis) + validasi penuh di checkMethodCall.
  if (std::holds_alternative<Expr::MethodCall>(expr.node)) {
    return checkMethodCall(std::get<Expr::MethodCall>(expr.node), expr,
                           loc);
  }
  // Varian unit `E.V` (Phase 26): base nama-tipe → ctor tanpa argumen.
  if (const auto *fa = std::get_if<Expr::FieldAccess>(&expr.node)) {
    if (const auto *vr = std::get_if<Expr::VarRef>(&fa->base->node)) {
      if (enums_.count(vr->name)) {
        return checkEnumCtor(vr->name, fa->field,
                             std::vector<std::unique_ptr<Expr>>{},
                             std::vector<Type>{}, expr, loc);
      }
    }
  }
  // Lambda (Phase 27): lift + rewrite menjadi Closure.
  if (std::holds_alternative<Expr::Lambda>(expr.node)) {
    return checkLambda(std::get<Expr::Lambda>(expr.node), expr, loc);
  }
  struct Visitor {
    TypeChecker *self;
    const Location &loc;
    Type operator()(const Expr::LiteralInt &) { return Type::i64(); }
    Type operator()(const Expr::LiteralBool &) { return Type::boolean(); }
    Type operator()(const Expr::LiteralString &) { return Type::string(); }
    Type operator()(const Expr::VarRef &n) {
      auto found = self->env_.lookup(n.name);
      if (found)
        return *found;
      // Referensi fungsi sebagai value (Phase 21b): tipenya signature.
      auto sigIt = self->sigs_.find(n.name);
      if (sigIt != self->sigs_.end()) {
        if (n.name == "main")
          return Type::error(); // sudah dilaporkan semantic; tanpa duplikat
        // Extern C (Phase 32): hanya panggilan langsung (ABI trampolin
        // tak berlaku untuk function pointer C).
        if (self->externFns_.count(n.name)) {
          self->errorAt(n.loc, "cannot use extern function '" + n.name +
                                   "' as a value",
                        "call it directly: " + n.name + "(...)",
                        static_cast<int>(n.name.size()));
          return Type::error();
        }
        const Signature &sig = sigIt->second;
        if (sig.ret.isError())
          return Type::error();
        for (const auto &pt : sig.params) {
          if (pt.isError())
            return Type::error();
        }
        return Type::func(sig.params, sig.ret);
      }
      // Fungsi generik tak bisa direifikasi (Phase 21c): panggil dengan
      // argumen konkret agar terinstansiasi.
      if (self->genericSigs_.count(n.name)) {
        self->errorAt(n.loc, "cannot use generic function '" + n.name +
                                 "' as a value",
                        "call it with concrete arguments to instantiate it, "
                        "e.g. " +
                            n.name + "(...)",
                        static_cast<int>(n.name.size()));
        return Type::error();
      }
      return Type::error(); // sudah dilaporkan semantic; tanpa duplikat
    }
    Type operator()(const Expr::ArrayLit &n) {
      if (n.elems.empty()) {
        self->errorAt(loc, "cannot infer element type of empty array literal",
                      "write at least one element, e.g. [0]");
        return Type::error();
      }
      Type elemTy = Type::error();
      bool first = true;
      for (const auto &e : n.elems) {
        Type t = self->checkExpr(*e);
        if (t.isError())
          return Type::error();
        if (t.kind == TypeKind::Void) {
          self->errorAt(e->loc(), "array element cannot have type 'void'");
          return Type::error();
        }
        if (t.kind == TypeKind::Array) {
          self->errorAt(e->loc(),
                        "nested arrays are not supported");
          return Type::error();
        }
        if (t.kind == TypeKind::Func) {
          self->errorAt(e->loc(),
                        "arrays of functions are not supported");
          return Type::error();
        }
        if (first) {
          elemTy = t;
          first = false;
        } else if (t != elemTy) {
          self->errorAt(e->loc(),
                        "mismatched array element type: expected '" +
                            elemTy.name() + "', found '" + t.name() + "'");
          return Type::error();
        }
      }
      return Type::array(elemTy);
    }
    Type operator()(const Expr::Index &n) {
      Type baseTy = self->checkExpr(*n.base);
      Type idxTy = self->checkExpr(*n.index);
      if (baseTy.isError() || idxTy.isError())
        return Type::error();
      if (baseTy.kind != TypeKind::Array) {
        self->errorAt(n.loc, "cannot index value of type '" + baseTy.name() +
                                 "'",
                      "indexing is only for arrays, e.g. numbers[0]");
        return Type::error();
      }
      if (idxTy.kind != TypeKind::I64) {
        self->errorAt(n.index->loc(),
                      "array index must be of type 'i64', found '" +
                          idxTy.name() + "'");
        return Type::error();
      }
      if (!baseTy.elem)
        return Type::error();
      return *baseTy.elem;
    }
    Type operator()(Expr::StructLit &n) {
      // Rewrite ctor enum yang diperiksa ulang (Phase 45/B1):
      // desugar match memeriksa ulang body arm hasil klon — tipe
      // HARUS applied asal, bukan `named(mangled)`. Field sudah
      // tervalidasi saat rewrite; tak perlu (dan tak boleh, agar
      // idempoten) divalidasi ulang.
      if (n.enumCtor && !n.enumType.isError())
        return n.enumType;
      // Struct generik (Phase 21d): inferensi/explisit + rewrite mangled.
      if (self->structTemplates_.count(n.name)) {
        if (self->structBad_.count(n.name))
          return Type::error(); // sudah dilaporkan; diam
        return self->checkGenericStructLit(n, loc);
      }
      // Enum bukan struct (Phase 26; pertahanan bila semantic dilewati).
      // Literal hasil rewrite ctor (`enumCtor`) dikecualikan — ia MEMANG
      // hidden struct yang valid dan dicek ulang sebagai argumen/payload.
      if (self->enums_.count(n.name) && !n.enumCtor) {
        self->errorAt(n.loc, "cannot construct enum '" + n.name +
                                 "' as struct",
                        "construct a variant: " + n.name + ".Variant(...)");
        return Type::error();
      }
      auto it = self->structs_.find(n.name);
      if (it == self->structs_.end())
        return Type::error(); // sudah dilaporkan semantic; tanpa duplikat
      const TypeChecker::StructInfo &info = it->second;
      for (const auto &f : n.fields) {
        const Type *want = nullptr;
        for (const auto &pr : info.fields) {
          if (pr.first == f.name) {
            want = &pr.second;
            break;
          }
        }
        Type got = self->checkExpr(*f.value);
        // Field tak dikenal/duplikat/hilang sudah dilaporkan semantic.
        if (!want || got.isError())
          continue;
        if (got != *want) {
          self->errorAt(f.value->loc(),
                        "mismatched field type: '" + n.name + "." + f.name +
                            "' expects '" + want->name() + "', found '" +
                            got.name() + "'");
        }
      }
      return Type::named(n.name);
    }
    Type operator()(const Expr::FieldAccess &n) {
      Type baseTy = self->checkExpr(*n.base);
      if (baseTy.isError())
        return Type::error();
      if (baseTy.kind != TypeKind::Struct || !baseTy.sname) {
        self->errorAt(n.loc, "cannot access field '" + n.field +
                                 "' on value of type '" + baseTy.name() + "'",
                      "field access is only for structs, e.g. person.age",
                      1 + static_cast<int>(n.field.size()));
        return Type::error();
      }
      auto it = self->structs_.find(self->structKey(baseTy));
      if (it == self->structs_.end())
        return Type::error(); // defensif; semantic meloloskan berarti ada
      for (const auto &pr : it->second.fields) {
        if (pr.first == n.field)
          return pr.second;
      }
      std::string fields;
      for (size_t i = 0; i < it->second.fields.size(); i++) {
        if (i)
          fields += ", ";
        fields += it->second.fields[i].first;
      }
      // Metode bernama sama? Arahkan ke panggilan (Phase 22).
      std::string hint = "struct '" + baseTy.name() + "' has fields: " + fields;
      auto mmit = self->typeMethods_.find(self->structKey(baseTy));
      if (mmit != self->typeMethods_.end()) {
        for (const auto &mn : mmit->second) {
          if (mn == n.field) {
            hint = "'" + baseTy.name() + "' has no field '" + n.field +
                   "'; did you mean to call the method: '" + n.field +
                   "(...)'?";
            break;
          }
        }
      }
      self->errorAt(n.loc, "unknown field '" + n.field + "' for struct '" +
                               baseTy.name() + "'",
                    hint, 1 + static_cast<int>(n.field.size()));
      return Type::error();
    }
    Type operator()(Expr::MethodCall &n) {
      // Dihandle di checkExpr (butuh ganti node) — tak terjangkau.
      self->errorAt(n.loc, "internal error: unresolved method call", "");
      return Type::error();
    }
    Type operator()(Expr::Propagate &n) {
      // Propagasi `?` (Phase 24; enum natif Phase 45/B1): operan
      // Result<T, E> + fungsi mengembalikan Result yang SAMA persis
      // (tanpa konversi E).
      Type inTy = self->checkExpr(*n.expr);
      if (inTy.isError())
        return Type::error();
      if (self->containsParam(inTy)) {
        self->errorAt(n.loc, "cannot propagate generic type '" +
                                 inTy.name() + "' with '?'",
                        "'?' resolves on concrete 'Result<T, E>'",
                        1);
        return Type::error();
      }
      if (inTy.kind != TypeKind::Struct || !inTy.sname ||
          *inTy.sname != "Result" || inTy.sargs.size() != 2) {
        self->errorAt(n.loc, "cannot propagate '" + inTy.name() +
                                 "' with '?'",
                        "'?' needs 'Result<T, E>'",
                        1);
        return Type::error();
      }
      const Type &retTy = self->currentRet_;
      if (retTy.kind != TypeKind::Struct || !retTy.sname ||
          *retTy.sname != "Result" || retTy.sargs.size() != 2 ||
          !(retTy.sargs[1] == inTy.sargs[1])) {
        self->errorAt(n.loc, "cannot propagate '" + inTy.name() +
                                 "' in function returning '" +
                                 retTy.name() + "'",
                        "'?' needs the enclosing function to return the "
                        "same 'Result<T, E>'",
                        1);
        return Type::error();
      }
      // Enum Result natif (std/result.ns 2.0): anotasi tag/slot varian
      // Ok/Err untuk codegen (cermin Variant.tag pada pola match).
      auto eit = self->enums_.find("Result");
      if (eit != self->enums_.end() && !eit->second.bad) {
        const EnumVariantInfo *okV = nullptr;
        const EnumVariantInfo *errV = nullptr;
        for (const auto &c : eit->second.variants) {
          if (c.name == "Ok")
            okV = &c;
          else if (c.name == "Err")
            errV = &c;
        }
        if (!okV || !errV || okV->arity != 1 || errV->arity != 1) {
          self->errorAt(n.loc, "cannot propagate '" + inTy.name() +
                                   "' with '?'",
                          "enum 'Result' must have 'Ok(T)' and 'Err(E)'",
                          1);
          return Type::error();
        }
        n.okTag = okV->tag;
        n.okSlot = self->enumPayloadField(okV->tag, 0);
        n.errSlot = self->enumPayloadField(errV->tag, 0);
      }
      return inTy.sargs[0];
    }
    Type operator()(Expr::Call &n) {
      // Builtin `len(arr)` (v0.7.0): panjang array statis → i64.
      if (n.callee == "len") {
        if (n.args.size() != 1) {
          self->errorAt(loc, "wrong number of arguments for 'len': expected "
                             "1, found " +
                                 std::to_string(n.args.size()),
                            "", static_cast<int>(n.callee.size()));
          return Type::error();
        }
        Type argTy = self->checkExpr(*n.args[0]);
        if (argTy.isError())
          return Type::error();
        if (argTy.kind != TypeKind::Array) {
          self->errorAt(n.args[0]->loc(),
                        "len() requires an array argument, found '" +
                            argTy.name() + "'");
          return Type::error();
        }
        return Type::i64();
      }
      // Builtins runtime v0.9.0 (diimplementasikan di codegen).
      if (isBuiltin(n.callee))
        return self->checkBuiltin(n, loc);
      // Panggilan fungsi generik (Phase 21c): inferensi + instansiasi.
      if (self->genericSigs_.count(n.callee))
        return self->checkGenericCall(n, loc);
      auto sigIt = self->sigs_.find(n.callee);
      if (sigIt == self->sigs_.end()) {
        // Panggilan tak langsung via variabel (Phase 21b).
        auto envTy = self->env_.lookup(n.callee);
        if (!envTy || envTy->isError())
          return Type::error(); // sudah dilaporkan semantic; tanpa duplikat
        if (envTy->kind != TypeKind::Func || !envTy->ret) {
          self->errorAt(loc, "cannot call '" + n.callee + "' of type '" +
                                 envTy->name() + "'",
                        "only function values can be called",
                        static_cast<int>(n.callee.size()));
          return Type::error();
        }
        return self->checkCallArgs(n, loc, envTy->params, *envTy->ret,
                                   n.callee);
      }
      const Signature &sig = sigIt->second;
      if (sig.ret.isError())
        return Type::error();
      return self->checkCallArgs(n, loc, sig.params, sig.ret, n.callee);
    }
    Type operator()(const Expr::Unary &n) {
      Type in = self->checkExpr(*n.operand);
      if (in.isError())
        return Type::error();
      if (in.kind != TypeKind::I64) {
        self->errorAt(
            loc, "unary '" + std::string(n.op == UnOp::Neg ? "-" : "+") +
                     "' requires operand of type 'i64', found '" + in.name() +
                     "'");
        return Type::error();
      }
      return Type::i64();
    }
    Type operator()(const Expr::Binary &n) {
      Type lhs = self->checkExpr(*n.lhs);
      Type rhs = self->checkExpr(*n.rhs);
      if (lhs.isError() || rhs.isError())
        return Type::error(); // tanpa error beruntun
      const std::string op = binOpName(n.op);
      switch (n.op) {
      case BinOp::Add:
      case BinOp::Sub:
      case BinOp::Mul:
      case BinOp::Div:
        if (lhs.kind == TypeKind::I64 && rhs.kind == TypeKind::I64) {
          // v0.9.5 hardening: pembagi literal-nol ditolak statis
          // (yang dinamis ditangani runtime check di codegen).
          if (n.op == BinOp::Div) {
            if (auto lit =
                    std::get_if<Expr::LiteralInt>(&n.rhs->node)) {
              if (lit->value == 0) {
                self->errorAt(loc, "division by zero",
                              "the divisor here is always 0");
                return Type::error();
              }
            }
          }
          return Type::i64();
        }
        self->errorAt(loc, "operator '" + op + "' requires 'i64' operands, "
                             "found '" + lhs.name() + "' and '" + rhs.name() +
                             "'");
        return Type::error();
      case BinOp::Lt:
      case BinOp::Gt:
      case BinOp::LtEq:
      case BinOp::GtEq:
        if (lhs.kind == TypeKind::I64 && rhs.kind == TypeKind::I64)
          return Type::boolean();
        self->errorAt(loc, "operator '" + op + "' requires 'i64' operands, "
                             "found '" + lhs.name() + "' and '" + rhs.name() +
                             "'");
        return Type::error();
      case BinOp::Eq:
      case BinOp::NotEq:
        if (lhs.kind == TypeKind::Struct || rhs.kind == TypeKind::Struct) {
          self->errorAt(loc, "cannot compare structs with '" + op + "'",
                        "compare fields instead");
          return Type::error();
        }
        if (lhs.kind == TypeKind::Array || rhs.kind == TypeKind::Array) {
          self->errorAt(loc, "cannot compare arrays with '" + op + "'",
                        "compare elements or len() instead");
          return Type::error();
        }
        if (lhs.kind == TypeKind::Func || rhs.kind == TypeKind::Func) {
          self->errorAt(loc, "cannot compare functions with '" + op + "'",
                        "compare the results of calling them instead");
          return Type::error();
        }
        if (lhs.kind == TypeKind::String || rhs.kind == TypeKind::String) {
          self->errorAt(loc, "cannot compare 'string' with '" + op + "'",
                        "string operations are coming with the standard "
                        "library");
          return Type::error();
        }
        if (lhs.kind == TypeKind::Void || rhs.kind == TypeKind::Void) {
          self->errorAt(loc, "cannot compare 'void' with '" + op + "'");
          return Type::error();
        }
        if (lhs != rhs) {
          self->errorAt(loc, "mismatched types for '" + op + "': '" +
                                 lhs.name() + "' and '" + rhs.name() + "'");
          return Type::error();
        }
        return Type::boolean();
      }
      return Type::error(); // tak terjangkau
    }
    Type operator()(const Expr::Lambda &) {
      // Tak terjangkau (di-intercept pre-dispatch → checkLambda);
      // pertahanan anti-crash.
      self->errorAt(loc, "internal error: unlifted lambda", "");
      return Type::error();
    }
    Type operator()(const Expr::Closure &n) { return n.funcType; }
  };
  Type result = std::visit(Visitor{this, loc}, expr.node);
  --exprDepth_;
  return result;
}

// ---------- konstanta waktu-kompilasi (Phase 70, RFC-0009 slice 1) ----------
// Batas keamanan (dienforce konstruksi): literal + unary/arith/cmp +
// ref-const. Tanpa call/variabel/fungsi/loop/string-ops → terminating
// trivial, deterministik, tanpa efek samping.

std::unique_ptr<Expr>
TypeChecker::makeConstLit(const TypeChecker::ConstVal &v,
                          const Location &loc) {
  auto e = std::make_unique<Expr>();
  if (v.type.kind == TypeKind::I64) {
    Expr::LiteralInt lit;
    lit.value = v.ival;
    lit.loc = loc;
    e->node = lit;
  } else if (v.type.kind == TypeKind::Bool) {
    Expr::LiteralBool lit;
    lit.value = v.bval;
    lit.loc = loc;
    e->node = lit;
  } else {
    Expr::LiteralString lit;
    lit.value = v.sval;
    lit.loc = loc;
    e->node = lit;
  }
  return e;
}

bool TypeChecker::evalConstExpr(const Expr &e, const Location &errLoc,
                                ConstVal &out) {
  struct V {
    TypeChecker *self;
    const Location *errLoc;
    ConstVal *out;
    bool operator()(const Expr::LiteralInt &n) {
      out->type = Type::i64();
      out->ival = n.value;
      return true;
    }
    bool operator()(const Expr::LiteralBool &n) {
      out->type = Type::boolean();
      out->bval = n.value;
      return true;
    }
    bool operator()(const Expr::LiteralString &n) {
      out->type = Type::string();
      out->sval = n.value;
      return true;
    }
    bool operator()(const Expr::VarRef &n) {
      // Ref const lain (rekursif + guard siklus via evalConst).
      if (!self->evalConst(n.name, n.loc))
        return false;
      auto it = self->constVals_.find(n.name);
      if (it == self->constVals_.end())
        return false; // error sudah dilaporkan
      *out = it->second;
      return true;
    }
    bool i64op(BinOp op, int64_t a, int64_t b, const Location &loc) {
      __int128 r = 0;
      switch (op) {
      case BinOp::Add: r = (__int128)a + b; break;
      case BinOp::Sub: r = (__int128)a - b; break;
      case BinOp::Mul: r = (__int128)a * b; break;
      case BinOp::Div:
        if (b == 0) {
          self->errorAt(loc, "division by zero in constant expression",
                        "constant divisors must be non-zero");
          return false;
        }
        r = (__int128)a / b; // INT64_MIN/-1 dicek di bawah (= overflow)
        break;
      default: return false;
      }
      if (r < (__int128)INT64_MIN || r > (__int128)INT64_MAX) {
        self->errorAt(loc, "integer overflow in constant expression",
                      "constant arithmetic must fit in 'i64'");
        return false;
      }
      out->type = Type::i64();
      out->ival = (int64_t)r;
      return true;
    }
    bool operator()(const Expr::Unary &n) {
      ConstVal v;
      if (!self->evalConstExpr(*n.operand, *errLoc, v))
        return false;
      if (v.type.kind != TypeKind::I64) {
        self->errorAt(n.loc, "unary minus needs an 'i64' constant",
                      "constant must be an integer literal expression");
        return false;
      }
      if (n.op == UnOp::Pos) {
        *out = v;
        return true;
      }
      __int128 r = -(__int128)v.ival;
      if (r < (__int128)INT64_MIN) {
        self->errorAt(n.loc, "integer overflow in constant expression",
                      "constant arithmetic must fit in 'i64'");
        return false;
      }
      out->type = Type::i64();
      out->ival = (int64_t)r;
      return true;
    }
    bool operator()(const Expr::Binary &n) {
      ConstVal a, b;
      if (!self->evalConstExpr(*n.lhs, *errLoc, a))
        return false;
      if (!self->evalConstExpr(*n.rhs, *errLoc, b))
        return false;
      if (n.op == BinOp::Add || n.op == BinOp::Sub || n.op == BinOp::Mul ||
          n.op == BinOp::Div) {
        if (a.type.kind != TypeKind::I64 ||
            b.type.kind != TypeKind::I64) {
          self->errorAt(n.loc, "arithmetic needs 'i64' constants",
                        "constant must be an integer literal expression");
          return false;
        }
        return i64op(n.op, a.ival, b.ival, n.loc);
      }
      if (n.op == BinOp::Lt || n.op == BinOp::Gt || n.op == BinOp::LtEq ||
          n.op == BinOp::GtEq) {
        if (a.type.kind != TypeKind::I64 ||
            b.type.kind != TypeKind::I64) {
          self->errorAt(n.loc, "comparison needs 'i64' constants",
                        "constant must be an integer literal expression");
          return false;
        }
        bool r = (n.op == BinOp::Lt)    ? (a.ival < b.ival)
                 : (n.op == BinOp::Gt)  ? (a.ival > b.ival)
                 : (n.op == BinOp::LtEq) ? (a.ival <= b.ival)
                                         : (a.ival >= b.ival);
        out->type = Type::boolean();
        out->bval = r;
        return true;
      }
      // == / != : sisi sama, non-string/void (cermin checker).
      if (a.type.kind != b.type.kind || a.type.kind == TypeKind::Void ||
          a.type.kind == TypeKind::String) {
        self->errorAt(n.loc, "constant equality needs matching "
                             "non-string operands",
                      "compare two 'i64' or two 'bool' constants");
        return false;
      }
      bool eq = (a.type.kind == TypeKind::I64) ? (a.ival == b.ival)
                                              : (a.bval == b.bval);
      out->type = Type::boolean();
      out->bval = (n.op == BinOp::Eq) ? eq : !eq;
      return true;
    }
    bool operator()(const Expr::Call &) {
      self->errorAt(*errLoc, "not a constant expression",
                    "constant items allow literals, arithmetic, and other "
                    "constants only (no calls)");
      return false;
    }
    bool notConst() {
      self->errorAt(*errLoc, "not a constant expression",
                    "constant items allow literals, arithmetic, and other "
                    "constants only");
      return false;
    }
    bool operator()(const Expr::MethodCall &) { return notConst(); }
    bool operator()(const Expr::Propagate &) { return notConst(); }
    bool operator()(const Expr::ArrayLit &) { return notConst(); }
    bool operator()(const Expr::Index &) { return notConst(); }
    bool operator()(const Expr::StructLit &) { return notConst(); }
    bool operator()(const Expr::FieldAccess &) { return notConst(); }
    bool operator()(const Expr::Lambda &) { return notConst(); }
    bool operator()(const Expr::Closure &) { return notConst(); }
  };
  return std::visit(V{this, &errLoc, &out}, e.node);
}

bool TypeChecker::evalConst(const std::string &name, const Location &useLoc) {
  if (constVals_.count(name))
    return true;
  if (constBad_.count(name))
    return false;
  if (!constVisiting_.insert(name).second) {
    errorAt(useLoc, "circular constant '" + name + "'",
            "constants cannot depend on themselves, directly or indirectly");
    constBad_.insert(name);
    return false;
  }
  // Cari deklarasi (prog tak disimpan; pindai via pending? — const
  // decls diakses lewat program di processConsts; evalConst hanya
  // dipanggil dari sana/evalConstExpr dengan tabel decl terisi).
  // (Implementasi: processConsts mengisi constDecls_ dulu — lihat bawah.)
  bool ok = false;
  auto dit = constDecls_.find(name);
  if (dit == constDecls_.end()) {
    // Bukan const: fungsi (legal sebagai value, tapi bukan konstanta)
    // atau unknown (semantic sudah error) — diam agar tak beruntun,
    // KECUALI fungsi (butuh pesan jelas karena semantic meloloskan).
    if (funcNames_.count(name) || sigs_.count(name)) {
      errorAt(useLoc, "not a constant expression",
              "'" + name + "' is a function; constants need values");
      constBad_.insert(name);
    }
  } else {
    ConstVal v;
    if (evalConstExpr(*dit->second->value, dit->second->loc, v)) {
      if (dit->second->declaredType) {
        Type want = resolveType(*dit->second->declaredType,
                                dit->second->loc);
        if (!want.isError() && !(want == v.type)) {
          errorAt(dit->second->loc,
                  "mismatched types in 'const': '" + name +
                      "' is declared as '" + want.name() +
                      "' but initializer has type '" + v.type.name() + "'",
                  "adjust the annotation or the initializer");
        } else if (!want.isError()) {
          v.type = want;
          constVals_[name] = v;
          ok = true;
        }
      } else {
        constVals_[name] = v;
        ok = true;
      }
    }
    if (!ok)
      constBad_.insert(name);
  }
  constVisiting_.erase(name);
  return ok;
}

void TypeChecker::substExpr(std::unique_ptr<Expr> &e,
                            std::unordered_set<std::string> &bound) {
  if (!e)
    return;
  struct V {
    TypeChecker *self;
    std::unordered_set<std::string> *bound;
    void kid(std::unique_ptr<Expr> &k) { self->substExpr(k, *bound); }
    void operator()(Expr::LiteralInt &) {}
    void operator()(Expr::LiteralBool &) {}
    void operator()(Expr::LiteralString &) {}
    void operator()(Expr::VarRef &) {}
    void operator()(Expr::Call &n) {
      for (auto &a : n.args)
        kid(a);
    }
    void operator()(Expr::MethodCall &n) {
      // Receiver tipe-enum (`E.V()`) bukan nilai — jangan substitusi.
      bool enumCtor = false;
      if (auto *vr = std::get_if<Expr::VarRef>(&n.receiver->node))
        enumCtor = self->enums_.count(vr->name) > 0;
      if (!enumCtor)
        kid(n.receiver);
      for (auto &a : n.args)
        kid(a);
    }
    void operator()(Expr::Propagate &n) { kid(n.expr); }
    void operator()(Expr::ArrayLit &n) {
      for (auto &el : n.elems)
        kid(el);
    }
    void operator()(Expr::Index &n) {
      kid(n.base);
      kid(n.index);
    }
    void operator()(Expr::StructLit &n) {
      for (auto &f : n.fields)
        kid(f.value);
    }
    void operator()(Expr::FieldAccess &n) { kid(n.base); }
    void operator()(Expr::Unary &n) { kid(n.operand); }
    void operator()(Expr::Binary &n) {
      kid(n.lhs);
      kid(n.rhs);
    }
    void operator()(Expr::Lambda &n) {
      for (const auto &p : n.params)
        bound->insert(p.name);
      if (n.body)
        self->substBlock(*n.body, *bound);
    }
    void operator()(Expr::Closure &n) {
      for (auto &c : n.captures)
        kid(c);
    }
  };
  // VarRef→const di root: ganti node (scope-aware).
  if (auto *vr = std::get_if<Expr::VarRef>(&e->node)) {
    auto it = constVals_.find(vr->name);
    if (it != constVals_.end() && !bound.count(vr->name)) {
      e = makeConstLit(it->second, vr->loc);
      return;
    }
  }
  std::visit(V{this, &bound}, e->node);
}

static void patBindNames(const Pat &pat, std::unordered_set<std::string> &out) {
  struct V {
    std::unordered_set<std::string> *out;
    void operator()(const Pat::Bind &n) { out->insert(n.name); }
    void operator()(const Pat::Struct &n) {
      for (const auto &f : n.fields)
        patBindNames(*f.pat, *out);
    }
    void operator()(const Pat::Variant &n) {
      for (const auto &a : n.args)
        patBindNames(*a, *out);
    }
    void operator()(const Pat::Wild &) {}
    void operator()(const Pat::LitInt &) {}
    void operator()(const Pat::LitBool &) {}
    void operator()(const Pat::LitStr &) {}
  };
  std::visit(V{&out}, pat.node);
}

void TypeChecker::substBlock(Block &block,
                             std::unordered_set<std::string> &bound) {
  for (auto &stmt : block.stmts)
    substStmt(stmt, bound);
}

void TypeChecker::substStmt(Stmt &stmt,
                            std::unordered_set<std::string> &bound) {
  struct V {
    TypeChecker *self;
    std::unordered_set<std::string> *bound;
    void operator()(LetStmt &n) {
      self->substExpr(n.value, *bound); // RHS scope luar dulu
      bound->insert(n.name);
    }
    void operator()(PrintStmt &n) { self->substExpr(n.arg, *bound); }
    void operator()(ReturnStmt &n) {
      if (n.value)
        self->substExpr(n.value, *bound);
    }
    void operator()(ExprStmt &n) { self->substExpr(n.expr, *bound); }
    void operator()(IfStmt &n) {
      self->substExpr(n.cond, *bound);
      auto inner = *bound;
      self->substBlock(*n.thenBlock, inner);
      if (n.elseBlock) {
        auto inner2 = *bound;
        self->substBlock(*n.elseBlock, inner2);
      }
    }
    void operator()(WhileStmt &n) {
      self->substExpr(n.cond, *bound);
      auto inner = *bound;
      self->substBlock(*n.body, inner);
    }
    void operator()(AssignStmt &n) { self->substExpr(n.value, *bound); }
    void operator()(MatchStmt &n) {
      self->substExpr(n.subject, *bound);
      for (auto &arm : n.arms) {
        auto inner = *bound;
        patBindNames(arm.pat, inner);
        if (arm.guard)
          self->substExpr(arm.guard, inner);
        self->substBlock(*arm.body, inner);
      }
    }
  };
  std::visit(V{this, &bound}, stmt);
}

void TypeChecker::processConsts(Program &prog) {
  constVals_.clear();
  constBad_.clear();
  constVisiting_.clear();
  constDecls_.clear();
  for (auto &cd : prog.consts)
    constDecls_[cd.name] = &cd;
  // Evaluasi (urutan-bebas; siklus/div-nol/overflow/non-const = error
  // per deklarasi; tepat satu error per masalah via constBad_).
  for (auto &cd : prog.consts)
    evalConst(cd.name, cd.loc);
  // Substitusi ke body fungsi + metode pending (template generik ikut
  // tersubstitusi sebelum instansiasi mengklonnya).
  for (auto &fn : prog.functions) {
    if (fn.isExtern || !fn.tparams.empty()) {
      // Template generik: substitusi juga (instance mengklon template).
      // Extern: tanpa body.
      if (fn.isExtern)
        continue;
    }
    std::unordered_set<std::string> bound;
    for (const auto &p : fn.params)
      bound.insert(p.name);
    substBlock(fn.body, bound);
  }
  for (auto &md : pendingMethods_) {
    std::unordered_set<std::string> bound;
    for (const auto &p : md.params)
      bound.insert(p.name);
    substBlock(md.body, bound);
  }
}

} // namespace nusa
