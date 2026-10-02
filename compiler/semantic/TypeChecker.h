#pragma once
#include "compiler/ast/AST.h"
#include "compiler/semantic/Scopes.h"
#include "compiler/semantic/Semantic.h"
#include "compiler/types/Type.h"
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nusa {

// Type checking v0.5.0: signatures + bodies + calls + returns
// (tanpa generics / advanced type system).
//
//  - Pass 1: kumpulkan signature semua fungsi (dukung rekursi/mutual).
//    Parameter `void` ditolak.
//  - Pass 2: periksa setiap body dengan return-type context:
//      * `return <expr>`: tipe harus == return type (fungsi void → error).
//      * `return;` polos: hanya fungsi void (non-void → error).
//      * fungsi non-void harus diakhiri `return <expr>`.
//      * `return` harus statement terakhir (unreachable sesudahnya → error;
//        aturan v0.5.0 untuk body straight-line).
//  - Calls: callee dikenal (diam bila tak dikenal — semantic sudah lapor),
//    `main`/reserved tak bisa dipanggil (diam — semantic sudah lapor),
//    jumlah argumen + tipe tiap argumen dicek; hasil = return type.
//  - Kondisi `if`/`while` harus `bool`.
//  - Return (aturan v0.6.0, generalisasi all-paths): `return` harus
//    terakhir di block-nya; fungsi non-void harus selalu me-return
//    (block selalu-return = diakhiri return bernilai, atau `if` dengan
//    else yang kedua cabangnya selalu-return; `while` tak pernah
//    dihitung karena bisa nol iterasi).
  //  - Struct (v0.8.0): tabel struct (field : tipe); tipe bernama harus
  //    dikenal; field struct dicek tipenya saat konstruksi; akses field
  //    menghasilkan tipe field; struct tanpa param/return/array-elem.
  //  - Alias tipe (Phase 21a, v1.2.0): `type X = T;` menghuni namespace
  //    tipe bersama struct; forward reference OK; siklus/rantai dalam
  //    ditolak; dikanonikalisasi ke target (lihat check()).
  //  - Generik (Phase 21c, v1.2.0): `fn f<T>(x: T) -> T`. Inferensi di
  //    call-site (tanpa turbofish); monomorphisasi di checker: template
  //    body TAK pernah dicek (seperti C++ — error muncul saat
  //    instansiasi); tiap kombinasi tipe argumen menghasilkan instance
  //    konkret bernama-mangled yang dicek penuh lalu di-append ke
  //    program; template dibuang sebelum codegen (codegen nol berubah).
  //  - `main`: `fn() -> void` (parser menjamin bentuk; di sini ditegaskan).
//  - Prasyarat: semantic analysis (nama) sudah lolos.
class TypeChecker {
public:
  // false jika ada error; errors() berisi semuanya (tidak berhenti
  // di error pertama). Error memakai struct yang sama bentuknya dengan
  // SemanticError agar diagnostic konsisten.
  //
  // prog diambil non-const (Phase 21a): alias tipe (`type X = T;`)
  // dikanonikalisasi IN-PLACE ke tipe targetnya agar codegen (yang
  // membaca anotasi mentah AST) melihat tipe kanonis. Hanya ditulis
  // bila resolusi sukses; bila gagal program ditolak sehingga AST
  // tak dipakai hilir.
  bool check(Program &prog);
  const std::vector<SemanticError> &errors() const { return errors_; }
  const Type &mainFuncType() const { return mainFuncType_; }

private:
  struct Signature {
    std::vector<Type> params;
    Type ret = Type::error();
  };
  // Signature fungsi generik (Phase 21c): formal mengandung Param.
  struct GenericSig {
    std::vector<std::string> tparams;
    std::vector<Type> params;
    Type ret = Type::error();
    bool ok = true; // false = signature rusak (sudah dilaporkan)
    // Batasan trait per tparam (Phase 22): `fn f<T: Printable>`.
    std::unordered_map<std::string, std::vector<std::string>> bounds;
    // Template sumber (untuk kloning body). Stabil selama check():
    // prog.functions tak di-append saat pass 2 (instance ke instances_).
    const FunctionDecl *tmpl = nullptr;
  };
  // Signature metode trait (Phase 22): `self` marker bernama "self".
  struct TraitMethodSig {
    std::vector<Type> params; // [0] = named("self")
    Type ret = Type::error();
  };
  struct TraitSig {
    std::unordered_map<std::string, TraitMethodSig> methods;
  };
  // Satu implementasi metode (Phase 22) untuk resolusi.
  struct MethodImpl {
    std::string trait;
    std::string mangled;
    Location loc; // lokasi definisi metode (privacy `_`)
  };
  // Definisi struct ter-resolve: field berurutan + tipe valid.
  struct StructInfo {
    std::vector<std::pair<std::string, Type>> fields;
  };
  // Satu varian enum (Phase 26): tag = indeks deklarasi; arity =
  // jumlah payload posisional. Tipe payload konkret dibaca dari
  // hidden struct (terinstansiasi) per situs pakai.
  struct EnumVariantInfo {
    std::string name;
    size_t arity = 0;
    int tag = 0;
  };
  // Definisi enum: varian berurutan + tparams (untuk ctor/pola).
  struct EnumInfo {
    std::vector<EnumVariantInfo> variants;
    std::vector<std::string> tparams;
    bool bad = false;
  };

  Type checkExpr(Expr &expr);
  // Validasi argumen panggilan bersama (langsung via signature maupun
  // tak langsung via value fungsi — Phase 21b): arity + tipe per posisi.
  // Mengembalikan tipe return (atau Error bila arity salah). `display`
  // adalah nama untuk pesan (callee biasa; `'T.m'` untuk metode).
  Type checkCallArgs(const Expr::Call &n, const Location &loc,
                     const std::vector<Type> &params, const Type &ret,
                     const std::string &display);
  // Panggilan fungsi generik (Phase 21c): inferensi + instansiasi +
  // rewrite callee ke nama mangled. n tak-const (callee ditulis ulang).
  Type checkGenericCall(Expr::Call &n, const Location &loc);
  // Instansiasi template fungsi untuk subst terinferensi → tipe return.
  Type instantiateGeneric(
      const std::string &name, const Location &loc,
      const std::unordered_map<std::string, Type> &sub);
  // Bangun signature template: tparam-named → Param, sisanya di-resolve.
  // Tulis-balik kanonis ke fn (diperlukan instansiasi via substType).
  void buildGenericSig(FunctionDecl &fn);
  // Daftarkan template struct: konversi field (tparam-named → Param,
  // tulis-balik) + validasi nama tak dikenal. Tanpa layout.
  void registerStructTemplate(StructDecl &sd);
  // Konversi satu anotasi mentah template (rekursif untuk [T]/fn).
  Type toFormalType(const Type &ty, const std::vector<std::string> &tparams,
                    const Location &loc);
  // Instansiasi struct generik konkret → tipe aplikasi (layout
  // didaftarkan; instance menyusul ke prog). Error → Type::error().
  Type instantiateStruct(const std::string &name,
                         const std::vector<Type> &args, const Location &loc);
  // Pemeriksaan literal struct generik (inferensi/eksplisit + rewrite).
  Type checkGenericStructLit(Expr::StructLit &n, const Location &loc);
  // Validasi signature extern C (Phase 32): param i64/string/bool,
  // return i64/bool/void; daftarkan ke sigs_ + externFns_. True bila ok.
  bool checkExternSig(FunctionDecl &fn);
  // Nama mangled instance struct ("Point_i64").
  std::string mangleStruct(const std::string &name,
                           const std::vector<Type> &args);
  // Kunci structs_ untuk tipe struct: aplikasi → nama mangled.
  std::string structKey(const Type &ty);
  // Bentuk codegen untuk tipe ter-resolve: aplikasi struct → nama
  // mangled (layout); sisanya apa adanya. Dipakai di semua writeback
  // anotasi (field/param/return/instance-decl) agar codegen yang
  // membaca AST mentah selalu melihat nama layout yang ada.
  Type mapAppToMangled(const Type &ty);
  // True bila tipe mengandung Param (aplikasi rigid).
  bool containsParam(const Type &ty);
  // Validasi signature instance konkret (aturan void/array
  // sama seperti pass 1b) + daftarkan ke sigs_. Tulis-balik kanonis.
  // sigOut = bentuk aplikasi (pesan); inst = bentuk mangled (codegen).
  bool checkInstanceSig(FunctionDecl &inst, Signature &sigOut);
  // Substitusi parameter tipe (Phase 21c) pada tipe / anotasi body.
  Type substType(const Type &ty,
                 const std::unordered_map<std::string, Type> &sub);
  void substBlockTypes(Block &b,
                       const std::unordered_map<std::string, Type> &sub);
  void substStmtTypes(Stmt &s,
                      const std::unordered_map<std::string, Type> &sub);
  // Substitusi tparam ke dalam posisi-tipe pada ekspresi & pola
  // (Phase 45/B1): MethodCall.targs (ctor enum `E<T>.V()`), StructLit
  // .targs, anotasi lambda, Pat::Struct.targs. Sebelumnya hanya
  // LetStmt.declaredType yang tersubstitusi sehingga targs eksplisit
  // di body generik selalu gagal ("unknown type 'T'").
  void substExprTypes(Expr &e,
                      const std::unordered_map<std::string, Type> &sub);
  void substPatTypes(Pat &p,
                     const std::unordered_map<std::string, Type> &sub);
  // Unifikasi formal (bisa Param) vs aktual konkret untuk inferensi.
  // false = konflik (pemanggil yang melapor).
  bool unifyType(const Type &formal, const Type &actual,
                 std::unordered_map<std::string, Type> &sub);
  // Nama instance: fn + tipe argumen ter-mangle ("f_i64_bool").
  std::string mangleInstance(const std::string &name,
                             const std::vector<std::string> &tparams,
                             const std::unordered_map<std::string, Type> &sub);
  std::string mangleType(const Type &ty);
  // Validasi panggilan builtin runtime v0.9.0 (arity + tipe argumen).
  Type checkBuiltin(Expr::Call &n, const Location &loc);
  // Kumpulkan signature trait (Phase 22): validasi self-first +
  // tanpa-tparams; simpan mentah (self marker).
  void buildTraitSig(const TraitDecl &td);
  // Proses satu blok impl (Phase 22): resolve target, cek duplikat +
  // konformansi, bangun decl mangled ke pendingMethods_ + registry.
  void processImplBlock(const ImplBlock &ib);
  // Substitusi marker named("self") → tipe target (Phase 22).
  Type substSelf(const Type &ty, const Type &target);
  // Kunci registry metode untuk tipe receiver ("" bila tak bermetode).
  std::string methodTypeKey(const Type &ty);
  // Nama mangled metode: trait_method_type ("Printable_print_Person").
  std::string mangleMethod(const std::string &trait,
                           const std::string &method, const Type &target);
  // Cek bounds `T: Trait` terpenuhi tipe aktual konkret (Phase 22).
  // false + error (sekali) bila ada bound gagal.
  bool checkBoundsSatisfied(const std::string &ownerKind,
                            const std::string &ownerName, const Location &loc,
                            const std::vector<std::string> &tparams,
                            const std::unordered_map<std::string,
                                                     std::vector<std::string>>
                                &bounds,
                            const std::unordered_map<std::string, Type> &sub);
  // Apakah tipe konkret mengimplementasikan trait (registry)?
  bool typeImplementsTrait(const Type &ty, const std::string &trait);
  // Resolusi + rewrite MethodCall → Call biasa (dispatch statis).
  // Menulis ulang owner.node bila sukses; Error bila gagal.
  Type checkMethodCall(Expr::MethodCall &n, Expr &owner,
                       const Location &loc);
  // Statement `match` (Phase 25): validasi pola + exhaustiveness,
  // lalu desugar ke if/let biasa yang di-append ke `out` (pengganti
  // node match — tanpa sisa untuk codegen).
  void checkMatchStmt(MatchStmt &ms, Block &out);
  // Validasi satu pola terhadap tipe subject (rekursif). False + error
  // bila invalid (nama/tipe/field/arity tak cocok). Non-const: bind
  // bernama varian ditulis ulang menjadi Variant (Phase 26).
  bool checkPattern(Pat &pat, const Type &subjectTy);
  // True bila pola selalu cocok (wildcard/bind/struct-semua-irrefutable).
  bool patIsIrrefutable(const Pat &pat);
  // Info layout untuk tipe struct (applied → mangled). Null bila absen.
  const StructInfo *structInfoFor(const Type &ty);
  // Validasi pola varian (`Some`/`None`/`Ok`/`Err` std; varian enum 26).
  bool checkVariantPattern(Pat::Variant &v, const Type &subjectTy);
  // Validasi pola varian enum pengguna (Phase 26): payload posisional
  // vs tipe konkret hidden struct terinstansiasi.
  bool checkEnumVariantPattern(Pat::Variant &v, const Type &subjectTy);
  // Proses deklarasi enum (Phase 26): validasi + bangun hidden struct
  // (`tag` + `v{tag}_{pos}`) ke hiddenOut; pemanggil menyisipkannya di
  // AWAL prog.structs sebelum pass 1a (agar field struct bisa memakai
  // enum generik tanpa peduli urutan deklarasi). False bila deklarasi
  // rusak (hidden dilewati; tanpa error beruntun).
  bool processEnumDecl(const EnumDecl &ed, const Program &prog,
                       StructDecl &hiddenOut);
  // Konstruktor enum `E.V(args)` (Phase 26): validasi varian/arity,
  // inferensi tparam dari payload (atau targs eksplisit), lalu tulis
  // ulang owner menjadi StructLit SUBSET hidden struct (slot non-aktif
  // tak diisi = undef; codegen mentoleransi). Mengembalikan tipe enum.
  Type checkEnumCtor(const std::string &ename, const std::string &vname,
                     std::vector<std::unique_ptr<Expr>> args,
                     std::vector<Type> targs, Expr &owner,
                     const Location &loc);
  // Nama field payload hidden struct untuk varian (tag,pos).
  static std::string enumPayloadField(int tag, size_t pos);
  // Lambda (Phase 27): isi ekspektasi tipe dari konteks (call/let/
  // return) ke node; diam bila bukan lambda/formal-bukan-fn.
  void expectLambda(Expr &e, const Type &formal);
  // Cek + lift lambda: desugar menjadi fungsi hidden `$lambda_N`
  // (env struct `$env_N` + prepended lets) dan tulis ulang owner
  // menjadi Closure. Mengembalikan tipe fn.
  Type checkLambda(Expr::Lambda &lam, Expr &owner, const Location &loc);
  // Kumpulkan free vars body lambda (scope-aware; lewati nested
  // lambda; bind pola match ikut membuka scope).
  void collectLambdaCaptures(const Expr::Lambda &lam,
                             std::vector<std::pair<std::string, Location>> &out);
  // True bila subtree memuat Lambda (untuk tolak lambda di guard).
  bool exprHasLambda(const Expr &e);
  // Bangun desugar: `let tmp` + rantai if/else + binds ke `out`.
  void desugarMatch(MatchStmt &ms, const Type &subjectTy, Block &out);
  // Uji kesetaraan satu pola vs aksesor + kumpulkan binds (rekursif).
  // tests: kondisi (konjungsi via flag datar); binds: let siap pakai.
  void buildPatTest(const Pat &pat, std::unique_ptr<Expr> accessor,
                    std::vector<std::unique_ptr<Expr>> &tests,
                    std::vector<LetStmt> &binds);
  // Inline binding guard: VarRef terdaftar → klon aksesor (rekursif).
  void substGuardBinds(Expr &e,
                       const std::unordered_map<std::string, const Expr *> &acc);
  // Buat node ekspresi pembantu (lokasi dari pola).
  std::unique_ptr<Expr> makeVarRef(const std::string &name,
                                   const Location &loc);
  std::unique_ptr<Expr> makeLit(const Pat &pat);
  std::unique_ptr<Expr> makeEq(std::unique_ptr<Expr> lhs,
                               std::unique_ptr<Expr> rhs,
                               const Location &loc);
  std::unique_ptr<Expr> makeStrEq(std::unique_ptr<Expr> lhs,
                                  std::unique_ptr<Expr> rhs,
                                  const Location &loc);
  std::unique_ptr<Expr> makeField(std::unique_ptr<Expr> base,
                                  const std::string &field,
                                  const Location &loc);
  // Counter nama tmp desugar (`$match0`, …).
  int matchTmp_ = 0;
  Type resolveType(const Type &ty, const Location &loc);
  void checkFunction(FunctionDecl &fn);
  void checkBlock(Block &block);
  void checkStmt(Stmt &stmt);
  // True bila block PASTI keluar via `return` bernilai (non-void).
  bool blockAlwaysReturns(const Block &block);
  void errorAt(const Location &loc, const std::string &msg,
               const std::string &hint = "", int length = 1);

  Scopes<Type> env_; // tumpukan scope nilai fungsi aktif
  std::unordered_map<std::string, Signature> sigs_; // semua fungsi
  std::unordered_map<std::string, GenericSig> genericSigs_; // fn generik
  // Instance monomorphisasi (Phase 21c): di-append ke prog usai pass 2
  // (terpisah agar pass 2 tak realloc prog.functions saat iterasi).
  std::vector<FunctionDecl> instances_;
  // Nama mangled yang sudah dibuat/sedang dibuat (cache + anti-rekursi).
  std::unordered_set<std::string> instantiated_;
  // Kedalaman instansiasi bersarang (anti-divergensi: f<T> → f<[T]> …).
  int instDepth_ = 0;
  static constexpr int kMaxInstDepth = 64;
  std::unordered_map<std::string, StructInfo> structs_; // semua struct
  // Template struct generik (Phase 21d): nama → deklarasi (layout
  // dibangun per-instance). Pointer stabil seperti GenericSig::tmpl.
  std::unordered_map<std::string, const StructDecl *> structTemplates_;
  // Template struct rusak (error dilaporkan saat registrasi; pakai diam).
  std::unordered_set<std::string> structBad_;
  // Instance struct konkret menunggu masuk prog (cermin instances_).
  std::vector<StructDecl> structInstDecls_;
  // Nama mangled instance MILIK checker (beda dari struct user dengan
  // nama sama — tabrakan harus error, bukan cache-hit diam).
  std::unordered_set<std::string> structInstNames_;
  // Instance GAGAL (error dilaporkan sekali; pakai berikutnya diam).
  std::unordered_set<std::string> structBadInst_;
  // Nama struct/fungsi user (Phase 21d): deteksi tabrakan nama mangled.
  // Diisi saat pass 1a/1b (prog asli, sebelum instance di-append).
  std::unordered_set<std::string> structNames_;
  std::unordered_set<std::string> funcNames_;
  // Nama trait (Phase 22) + signature-nya.
  std::unordered_map<std::string, TraitSig> traitSigs_;
  // Registry metode (Phase 22): (kunci-tipe, metode) → implementasi.
  // Kunci-tipe: nama struct / mangled aplikasi / keyword primitif.
  std::map<std::pair<std::string, std::string>, std::vector<MethodImpl>>
      methodImpls_;
  // Metode per kunci-tipe (untuk daftar "has methods:" di diagnostik).
  std::map<std::string, std::vector<std::string>> typeMethods_;
  // Trait yang diimplementasikan per kunci-tipe (untuk cek bounds).
  std::map<std::string, std::unordered_set<std::string>> typeTraits_;
  // Metode impl menunggu masuk prog (cermin instances_: agar pass 2
  // tak realloc prog.functions saat iterasi).
  std::vector<FunctionDecl> pendingMethods_;
  // Fungsi eksternal C (Phase 32): tanpa body; panggilan langsung saja
  // (tak bisa dijadikan value — ABI trampolin tak berlaku).
  std::unordered_set<std::string> externFns_;
  // Nama mangled struct yang sedang diinstansiasi (anti ukuran-infinit).
  std::unordered_set<std::string> instantiating_;  // Definisi enum per nama dasar (Phase 26): hidden struct menyusul ke
  // prog.structs sebelum pass 1a sehingga seluruh mesin struct
  // (resolve/layout/trait/print) berlaku gratis.
  std::unordered_map<std::string, EnumInfo> enums_;
  // Staging lift lambda (Phase 27): fungsi + struct env hidden,
  // di-append ke prog usai pass 2 (cermin instances_/structInstDecls_).
  std::vector<FunctionDecl> lambdaFuncs_;
  std::vector<StructDecl> lambdaStructs_;
  int lambdaSeq_ = 0;
  // Mode toleran return lambda (expected-ret mengandung Param):
  // return dikumpulkan, diunifikasi di akhir (bukan error langsung).
  bool collectLambdaRet_ = false;
  std::vector<std::pair<Location, Type>> lambdaRetActuals_;
  // Target alias mentah per nama (Phase 21a); di-resolve lazy oleh
  // resolveType (mendukung forward reference antar-alias/struct).
  std::unordered_map<std::string, Type> aliases_;
  // Konstanta waktu-kompilasi (Phase 70, RFC-0009 slice 1):
  // nilai tervalidasi per nama; substitusi desugar ke body fungsi.
  struct ConstVal {
    Type type;
    int64_t ival = 0;
    bool bval = false;
    std::string sval;
  };
  // Evaluasi semua ConstDecl (fixpoint; urutan-bebas; siklus = error)
  // + substitusi scope-aware ke body fungsi dan metode pending.
  void processConsts(Program &prog);
  // Evaluasi satu konstanta (rekursif + guard siklus). True bila ok.
  bool evalConst(const std::string &name, const Location &useLoc);
  // Evaluasi pohon ekspresi konstan. Gagal (false) = bukan-const /
  // div-nol / overflow / mismatch — error dilaporkan di sini.
  bool evalConstExpr(const Expr &e, const Location &errLoc, ConstVal &out);
  // Substitusi VarRef→literal untuk semua const valid ke prog.
  void substConsts(Program &prog);
  void substBlock(Block &block, std::unordered_set<std::string> &bound);
  void substStmt(Stmt &stmt, std::unordered_set<std::string> &bound);
  void substExpr(std::unique_ptr<Expr> &e,
                 std::unordered_set<std::string> &bound);
  static std::unique_ptr<Expr> makeConstLit(const ConstVal &v,
                                            const Location &loc);
  // True bila error sudah dilaporkan untuk const ini (anti-beruntun).
  std::unordered_map<std::string, ConstVal> constVals_;
  std::unordered_set<std::string> constBad_;
  std::unordered_set<std::string> constVisiting_;
  // Deklarasi const per nama (diisi processConsts; pointer stabil
  // selama pass — prog.consts tak direalokasi di tengah pass).
  std::unordered_map<std::string, ConstDecl *> constDecls_;
  // Alias yang resolusinya sudah GAGAL (error dilaporkan sekali di situs
  // pertama; situs berikutnya diam via cabang alias — cegah duplikat).
  std::unordered_set<std::string> aliasBad_;
  // Tumpukan resolusi alias aktif (deteksi siklus + rantai dalam).
  std::vector<std::string> resolving_;
  // Batas rantai alias (cth. A0=A1=...; preseden: rantai import = 100).
  static constexpr size_t kMaxAliasDepth = 100;
  std::vector<SemanticError> errors_;
  // Batas rekursi checkExpr (v0.9.5, lih. Semantic.h).
  static constexpr int kMaxExprDepth = 500;
  int exprDepth_ = 0;
  Type mainFuncType_ = Type::error();
  Type currentRet_ = Type::error(); // return type fungsi aktif
  std::string currentFunc_;         // nama fungsi aktif (untuk pesan)
};

} // namespace nusa
