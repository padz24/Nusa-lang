#pragma once
#include "compiler/ast/AST.h"
#include "compiler/semantic/Scopes.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>

namespace llvm {
class DIBuilder;
class DICompileUnit;
class DICompositeType;
class DIDescriptor;
class DIFile;
class DISubprogram;
class DIType;
} // namespace llvm
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nusa {

// LLVM backend: AST -> LLVM IR (LLVM 21.1.8, opaque pointers),
// lalu diverifikasi dengan verifyModule sebelum di-emit.
//
// Status v0.5.0: functions.
//  - Setiap `fn` Nusa → LLVM function (parameter i64/i1/ptr, return
//    mapping sama; `void` → LLVM void). `fn main` → `i32 @main()`
//    yang mengembalikan 0 (entry point executable).
//  - Two-pass: deklarasikan semua signature dulu (rekursi & forward
//    reference), lalu bangkitkan bodies.
//  - Local scope per fungsi: parameter + `let` → alloca entry-block.
//  - Calls → LLVM `call`; `return` → `ret`/`ret void`.
//  - Representasi nilai v0.3.0 tak berubah (i64/i1/ptr, puts/printf/select).
//  - Status v0.6.0: control flow via basic blocks — `if` (then/else/merge),
//    `while` (cond/body/after). Tiap Block = scope leksikal (shadowing
//    via tumpukan bindings; semua alloca di entry block).
//  - Status v0.7.0: arrays sebagai alloca statis `[N x T]` di entry.
//    Nilai array = POINTER ke alloca (tanpa load); skalar = loaded value.
//    Indexing memuat runtime bounds check (puts + exit(1)); `len()`
//    adalah konstanta kompilasi. Array immutable, tanpa param/return.
//  - Prasyarat: semantic + typecheck sudah lolos; guard di sini murni
//    pertahanan anti-crash (dilaporkan sebagai internal error).
class Codegen {
public:
  Codegen(std::string moduleName, std::string sourceFile);
  ~Codegen();

  // Menghasilkan LLVM module dari Program. false jika gagal; errOut
  // berisi pesan yang sudah diformat (termasuk file:line:col bila ada).
  // Prasyarat: semantic + type checking sudah lolos.
  bool generate(const Program &prog, std::string &errOut);

  // Triple target cross-compilation (Phase 33, mis.
  // "x86_64-linux-gnu"). Kosong = host (tak disentuh — perilaku lama).
  // Dipanggil sebelum generate().
  void setTargetTriple(const std::string &triple) {
    targetTriple_ = triple;
  }

  // Debug info DWARF (Phase 37): aktif via -g sebelum generate().
  void setDebug(bool on) { debug_ = on; }

  // Tulis LLVM IR (.ll) ke path. false jika gagal.
  bool emitIRToFile(const std::string &path, std::string &errOut);

  // Kembalikan IR sebagai string (untuk test / inspeksi).
  std::string emitIRToString();

private:
  enum class ValueKind { Int, Bool, String, Void, Array, Struct, Func };
  struct Binding {
    llvm::AllocaInst *slot = nullptr;
    ValueKind kind = ValueKind::Int;
    // Bermakna bila kind == Array: jenis + panjang elemen.
    ValueKind arrayElem = ValueKind::Int;
    uint64_t arrayLen = 0;
    // Bermakna bila kind == Struct: nama struct (untuk field layout).
    std::optional<std::string> structName;
    // Bermakna bila kind == Func (Phase 21b): signature untuk indirect call.
    std::optional<Type> funcType;
  };
  struct GenValue {
    llvm::Value *value = nullptr;
    ValueKind kind = ValueKind::Int;
    // Bermakna bila kind == Array (value = pointer ke alloca [N x T]).
    ValueKind arrayElem = ValueKind::Int;
    uint64_t arrayLen = 0;
    // Bermakna bila kind == Struct (value = pointer ke storage struct).
    std::optional<std::string> structName;
    // Bermakna bila kind == Func (value = function pointer).
    std::optional<Type> funcType;
  };
  // Layout satu struct: LLVM type + field terurut.
  struct FieldLayout {
    std::string name;
    ValueKind kind = ValueKind::Int;
    std::optional<std::string> structName; // bila kind == Struct
    llvm::Type *ty = nullptr;
    // Bila kind == Func: signature untuk indirect call (capture fn
    // di struct env closure, Phase 27).
    std::optional<Type> funcType;
  };
  struct StructLayout {
    llvm::StructType *ty = nullptr;
    std::vector<FieldLayout> fields;
    // Hidden struct enum (Phase 26): konstruktor hanya mengisi `tag` +
    // slot varian aktif; field hilang = slot tak-terinisialisasi (undef,
    // tak pernah dibaca karena match selalu via `tag`).
    bool hiddenEnum = false;
    // Struct env closure (Phase 27): field bertipe fn diizinkan
    // (function pointer, copy dangkal).
    bool closureEnv = false;
  };

  bool genFunction(const FunctionDecl &fn, std::string &errOut);
  // Bangkitkan Block; terminated=true bila diakhiri terminator.
  bool genBlock(const Block &block, bool &terminated, std::string &errOut);
  bool genStmt(const Stmt &stmt, std::string &errOut);
  bool genIf(const IfStmt &stmt, std::string &errOut);
  bool genWhile(const WhileStmt &stmt, std::string &errOut);
  bool genExpr(const Expr &expr, GenValue &out, std::string &errOut);
  // Emisi panggilan builtin runtime v0.9.0 (libc/LLVM langsung).
  bool genBuiltinCall(const Expr::Call &n, const Location &loc, GenValue &out,
                      std::string &errOut);
  bool emitPrint(const PrintStmt &stmt, std::string &errOut);
  bool emitReturn(const ReturnStmt &stmt, std::string &errOut);
  llvm::Value *getStringPtr(const std::string &text);
  llvm::AllocaInst *entryAlloca(llvm::Type *ty, const std::string &name);
  llvm::Type *llvmType(const Type &ty);
  // Mapping kind skalar → LLVM type (array/struct ditangani terpisah).
  llvm::Type *llvmScalar(ValueKind kind);
    // Pass 0: bangun named LLVM struct types + bodies (two-phase agar
  // forward reference antar-struct bisa).
  bool buildStructLayouts(const Program &prog, std::string &errOut);  const StructLayout *findLayout(const std::string &name) const;
  // Trampolin closure (Phase 27): `(ptr env, params...) -> ret` yang
  // meneruskan ke fungsi target (lifted: cast env + panggil dengan env
  // by-value; plain: abaikan env). Satu per nama target (memoized).
  // envStruct kosong = fungsi biasa. Null + errOut bila gagal.
  llvm::Function *getTrampoline(const std::string &target, const Type &sig,
                                const std::string &envStruct,
                                std::string &errOut);
  // malloc(sizeof(T)) + OOM-check; heap program-lifetime (model memori).
  llvm::Value *mallocFor(llvm::Type *ty, const std::string &name);
  // Tipe agregat ([N x T] / named struct) untuk copy; null bila tak dikenal.
  llvm::Type *aggregateType(const GenValue &val);
  // Cabang darurat bila pointer `p` null (malloc gagal): pesan + exit(1).
  // Melanjutkan pembangunan di block baru yang sehat.
  void emitOomCheck(llvm::Value *p, const std::string &what);
  // Store GenValue ke slot alloca (skalar langsung; agregat via load+store
  // dengan verifikasi tipe slot).
  bool storeGenValue(const GenValue &val, llvm::AllocaInst *slot,
                     const Location &loc, std::string &errOut);

  static std::string at(const Location &loc);
  static ValueKind kindOf(const Type &ty);

  std::string moduleName_;
  std::string targetTriple_; // "" = host (Phase 33)
  // Debug DWARF (Phase 37, aktif bila -g).
  bool debug_ = false;
  std::unique_ptr<llvm::DIBuilder> dib_;
  llvm::DICompileUnit *diCU_ = nullptr;
  llvm::DISubprogram *diSP_ = nullptr; // fungsi aktif
  std::map<std::string, llvm::DIFile *> diFiles_; // per file sumber
  llvm::DIType *diI64_ = nullptr;
  llvm::DIType *diBool_ = nullptr;
  llvm::DIType *diStr_ = nullptr;
  // Cache tipe struct DWARF per nama + guard rekursi (Phase 64).
  std::map<std::string, llvm::DICompositeType *> diStructs_;
  std::set<std::string> diStructActive_;
  // Mulai fungsi (CU/SP/file malas) + set lokasi definisi.
  void diBeginFunction(const FunctionDecl &fn);
  // Set lokasi sumber instruksi berikutnya (per statement).
  void diSetLoc(const Location &loc);
  // Deklarasikan variabel lokal (Phase 64: skalar + struct + array;
  // isParam memilih DW_TAG_formal_parameter dengan argNo 1-based).
  void diDeclare(llvm::AllocaInst *slot, const std::string &name,
                 const Type &ty, const Location &loc, bool isParam = false,
                 unsigned argNo = 0);
  llvm::DIType *diType(const Type &ty); // null bila tak terwakili
  // Tipe struct DWARF (cache + guard siklus; null bila gagal).
  llvm::DICompositeType *diStructType(const std::string &name);
  void diFinalize();
  std::string sourceFile_;
  std::unique_ptr<llvm::LLVMContext> ctx_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;
  llvm::Function *curFn_ = nullptr;
  llvm::Value *intFmt_ = nullptr; // global "%lld\n" (dibuat lazily)
  Scopes<Binding> bindings_; // tumpukan scope fungsi aktif
  std::unordered_map<std::string, llvm::Function *> functions_;
  std::unordered_map<std::string, ValueKind> funcRetKind_;
  // Signature fungsi per nama (Phase 21b): untuk materialisasi function
  // reference (`let f = add;`) dan indirect call. Dibangun di generate().
  std::unordered_map<std::string, Type> funcTypes_;
  std::unordered_map<std::string, StructLayout> structLayouts_;
  // Fungsi eksternal C (Phase 32): deklarasi saja (tanpa body);
  // pemetaan tipe C (bool = i32) dipakai saat deklarasi + panggil.
  std::unordered_set<std::string> externFns_;
  llvm::Type *ffiType(const Type &ty);
  // Trampolin per nama target (Phase 27, memoized) + tipe pair closure
  // `{ptr, ptr}` bersama (`{trampoline, env}`).
  std::unordered_map<std::string, llvm::Function *> tramps_;
  llvm::StructType *closurePairTy_ = nullptr;
  // Batas rekursi genExpr (v0.9.5, lih. Semantic.h).
  static constexpr int kMaxExprDepth = 500;
  int exprDepth_ = 0;
};

} // namespace nusa
