#include "compiler/codegen/Codegen.h"
#include "compiler/frontend/Builtin.h" // isBuiltin (Phase 51: tanpa semantic/)

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

#include <functional>

namespace nusa {

Codegen::Codegen(std::string moduleName, std::string sourceFile)
    : moduleName_(std::move(moduleName)), sourceFile_(std::move(sourceFile)),
      ctx_(std::make_unique<llvm::LLVMContext>()),
      module_(std::make_unique<llvm::Module>(moduleName_, *ctx_)),
      builder_(*ctx_) {
  module_->setSourceFileName(sourceFile_);
}

Codegen::~Codegen() = default;

std::string Codegen::at(const Location &loc) {
  return loc.file + ":" + std::to_string(loc.line) + ":" +
         std::to_string(loc.column);
}

llvm::Value *Codegen::getStringPtr(const std::string &text) {
  llvm::Constant *strConst =
      llvm::ConstantDataArray::getString(*ctx_, text, true);
  auto *gv = new llvm::GlobalVariable(
      *module_, strConst->getType(), true,
      llvm::GlobalValue::PrivateLinkage, strConst, ".str");
  gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  llvm::Value *zero = builder_.getInt32(0);
  llvm::Value *idx[] = {zero, zero};
  return builder_.CreateInBoundsGEP(gv->getValueType(), gv, idx);
}

llvm::AllocaInst *Codegen::entryAlloca(llvm::Type *ty,
                                       const std::string &name) {
  llvm::IRBuilder<> tmp(&curFn_->getEntryBlock(),
                        curFn_->getEntryBlock().begin());
  return tmp.CreateAlloca(ty, nullptr, name);
}

llvm::Type *Codegen::llvmType(const Type &ty) {
  switch (ty.kind) {
  case TypeKind::I64: return builder_.getInt64Ty();
  case TypeKind::Bool: return builder_.getInt1Ty();
  case TypeKind::String: return builder_.getPtrTy();
  case TypeKind::Void: return builder_.getVoidTy();
  case TypeKind::Func: return builder_.getPtrTy(); // function pointer
  case TypeKind::Struct: {
    // Tipe record (Phase 21d: by-value params/returns). Layout sudah
    // dibangun di Pass 0; fallback i64 tak terjangkau via driver.
    const StructLayout *layout =
        ty.sname ? findLayout(*ty.sname) : nullptr;
    if (layout)
      return layout->ty;
    return builder_.getInt64Ty();
  }
  default: return builder_.getInt64Ty(); // Error: tak terjangkau
  }
}

Codegen::ValueKind Codegen::kindOf(const Type &ty) {
  switch (ty.kind) {
  case TypeKind::Bool: return ValueKind::Bool;
  case TypeKind::String: return ValueKind::String;
  case TypeKind::Void: return ValueKind::Void;
  case TypeKind::Array: return ValueKind::Array;
  case TypeKind::Struct: return ValueKind::Struct;
  case TypeKind::Func: return ValueKind::Func;
  default: return ValueKind::Int;
  }
}

llvm::Type *Codegen::llvmScalar(ValueKind kind) {
  switch (kind) {
  case ValueKind::Bool: return builder_.getInt1Ty();
  case ValueKind::String: return builder_.getPtrTy();
  case ValueKind::Int:
  default: return builder_.getInt64Ty();
  }
}

static llvm::ArrayType *arrayTypeOf(llvm::Type *elemTy, uint64_t len) {
  return llvm::ArrayType::get(elemTy, len);
}

const Codegen::StructLayout *
Codegen::findLayout(const std::string &name) const {
  auto it = structLayouts_.find(name);
  if (it == structLayouts_.end())
    return nullptr;
  return &it->second;
}

bool Codegen::buildStructLayouts(const Program &prog, std::string &errOut) {
  structLayouts_.clear();
  // Fase A: buat named opaque types (forward reference antar-struct OK).
  for (const auto &sd : prog.structs) {
    if (structLayouts_.count(sd.name)) {
      errOut = at(sd.loc) + ": internal error: duplicate struct '" +
               sd.name + "'";
      return false;
    }
    llvm::StructType *ty =
        llvm::StructType::create(*ctx_, sd.name);
    structLayouts_[sd.name] = StructLayout{ty, {}};
  }
  // Fase B: isi bodies + catat layout field.
  for (const auto &sd : prog.structs) {
    StructLayout &layout = structLayouts_[sd.name];
    layout.hiddenEnum = sd.hiddenEnum;
    layout.closureEnv = sd.closureEnv;
    std::vector<llvm::Type *> fieldTys;
    for (const auto &f : sd.fields) {
      if (f.type.kind == TypeKind::Param) {
        // Template generik lolos checker — pertahanan anti-crash.
        errOut = at(f.loc) + ": internal error: unresolved generic field '" +
                 f.name + "'";
        return false;
      }
      ValueKind kind = kindOf(f.type);
      llvm::Type *ty = nullptr;
      std::optional<std::string> nested;
      if (kind == ValueKind::Struct && f.type.sname) {
        auto nit = structLayouts_.find(*f.type.sname);
        if (nit == structLayouts_.end()) {
          errOut = at(f.loc) + ": internal error: unknown struct '" +
                   *f.type.sname + "'";
          return false;
        }
        ty = nit->second.ty;
        nested = *f.type.sname;
      } else if (kind == ValueKind::Array || kind == ValueKind::Void) {
        errOut = at(f.loc) + ": internal error: unsupported struct field '" +
                 f.name + "'";
        return false;
      } else if (kind == ValueKind::Func) {
        if (layout.closureEnv) {
          // Capture fn di env closure (Phase 27): function pointer.
          ty = builder_.getPtrTy();
        } else {
          // Seharusnya ditolak typechecker; pertahanan anti-crash.
          errOut = at(f.loc) + ": internal error: unsupported function field '" +
                   f.name + "'";
          return false;
        }
      } else {
        ty = llvmScalar(kind);
      }
      std::optional<Type> fieldFunc;
      if (kind == ValueKind::Func)
        fieldFunc = f.type;
      layout.fields.push_back(
          {f.name, kind, nested, ty, fieldFunc});
      fieldTys.push_back(ty);
    }
    layout.ty->setBody(fieldTys);
  }
  return true;
}

// Pemetaan tipe Nusa → tipe C untuk deklarasi/panggilan extern
// (Phase 32): bool Nusa (i1) menjadi C int (i32, truthy); sisanya
// sama (i64, string = ptr, void).
llvm::Type *Codegen::ffiType(const Type &ty) {
  if (ty.kind == TypeKind::Bool)
    return builder_.getInt32Ty();
  return llvmType(ty);
}

// ===== Debug info DWARF (Phase 37, aktif bila -g) =====
// Bahasa host DWARF = C (tak ada tag Nusa; didokumentasikan).
// Cakupan v2 (Phase 64): + tipe struct (member + offset) + formal
// parameter + array bound. Agregat lain/closure dilewati.

llvm::DIType *Codegen::diType(const Type &ty) {
  if (!dib_)
    return nullptr;
  if (ty.kind == TypeKind::I64) {
    if (!diI64_)
      diI64_ = dib_->createBasicType("i64", 64, llvm::dwarf::DW_ATE_signed);
    return diI64_;
  }
  if (ty.kind == TypeKind::Bool) {
    if (!diBool_)
      diBool_ = dib_->createBasicType("bool", 1, llvm::dwarf::DW_ATE_boolean);
    return diBool_;
  }
  if (ty.kind == TypeKind::String) {
    if (!diStr_) {
      llvm::DIType *ch = dib_->createBasicType(
          "char", 8, llvm::dwarf::DW_ATE_signed_char);
      diStr_ = dib_->createPointerType(ch, 64);
    }
    return diStr_;
  }
  if (ty.kind == TypeKind::Struct && ty.sname) {
    return diStructType(*ty.sname);
  }
  return nullptr; // array (butuh bound slot)/fn/void/param: kontekstual
}

// Ukuran/align Nusa (byte): EKSAS di semua target LP64 (asumsi global
// Phase 33; field array ditolak checker). Cermin layout LLVM struct.
static bool nusaSizeAlign(const Type &ty, uint64_t &size, uint64_t &align) {
  using Kind = TypeKind;
  switch (ty.kind) {
  case Kind::I64:
    size = 8;
    align = 8;
    return true;
  case Kind::Bool:
    size = 1;
    align = 1;
    return true;
  case Kind::String:
    size = 8;
    align = 8;
    return true;
  default:
    return false;
  }
}

llvm::DICompositeType *Codegen::diStructType(const std::string &name) {
  if (!dib_)
    return nullptr;
  auto cached = diStructs_.find(name);
  if (cached != diStructs_.end())
    return cached->second;
  if (!diStructActive_.insert(name).second)
    return nullptr; // siklus: fallback null (checker menolak ukuran-infinit)
  const StructLayout *layout = findLayout(name);
  llvm::DICompositeType *out = nullptr;
  // Scope file: entry dulu, lalu file mana pun yang sudah ada (struct
  // bisa dideklarasikan di modul impor; presisi penuh = follow-up).
  llvm::DIFile *file = nullptr;
  auto it = diFiles_.find(sourceFile_);
  if (it != diFiles_.end())
    file = it->second;
  else if (!diFiles_.empty())
    file = diFiles_.begin()->second;
  if (layout && file) {
    // Resolver tipe member + ukuran/align; false = tak terwakili
    // (slot fn/env) → seluruh struct dilewati (fallback null).
    struct Member {
      std::string name;
      llvm::DIType *ty;
      uint64_t off;
      uint64_t size;
      uint64_t align;
    };
    std::function<bool(const FieldLayout &, llvm::DIType *&, uint64_t &,
                       uint64_t &)>
        resolve = [&](const FieldLayout &f, llvm::DIType *&mt, uint64_t &sz,
                      uint64_t &al) -> bool {
      Type ft;
      if (f.kind == ValueKind::Int)
        ft.kind = TypeKind::I64;
      else if (f.kind == ValueKind::Bool)
        ft.kind = TypeKind::Bool;
      else if (f.kind == ValueKind::String)
        ft.kind = TypeKind::String;
      else if (f.kind == ValueKind::Struct && f.structName) {
        // Rekursi: ukuran dari layout nested (aturan sama).
        const StructLayout *nl = findLayout(*f.structName);
        if (!nl)
          return false;
        uint64_t noff = 0, nmax = 1;
        for (const auto &nf : nl->fields) {
          llvm::DIType *nmt = nullptr;
          uint64_t nsz = 0, nal = 1;
          if (!resolve(nf, nmt, nsz, nal))
            return false;
          noff = ((noff + nal - 1) / nal) * nal + nsz;
          nmax = nal > nmax ? nal : nmax;
        }
        mt = diStructType(*f.structName);
        if (!mt)
          return false;
        sz = ((noff + nmax - 1) / nmax) * nmax;
        al = nmax;
        return true;
      } else {
        return false; // Func/env-slot: tak terwakili
      }
      if (!nusaSizeAlign(ft, sz, al))
        return false;
      mt = diType(ft);
      return mt != nullptr;
    };
    std::vector<Member> members;
    uint64_t off = 0, maxAlign = 1;
    bool ok = true;
    for (const auto &f : layout->fields) {
      llvm::DIType *mt = nullptr;
      uint64_t sz = 0, al = 1;
      if (!resolve(f, mt, sz, al)) {
        ok = false;
        break;
      }
      off = ((off + al - 1) / al) * al;
      members.push_back({f.name, mt, off, sz, al});
      maxAlign = al > maxAlign ? al : maxAlign;
      off += sz;
    }
    if (ok) {
      uint64_t total = ((off + maxAlign - 1) / maxAlign) * maxAlign;
      llvm::SmallVector<llvm::Metadata *, 8> elts;
      for (const auto &m : members) {
        elts.push_back(dib_->createMemberType(
            nullptr, m.name, file, 0, m.size * 8, m.align * 8, m.off * 8,
            llvm::DINode::FlagZero, m.ty));
      }
      out = dib_->createStructType(nullptr, name, file, 0, total * 8,
                                   maxAlign * 8, llvm::DINode::FlagZero,
                                   nullptr, dib_->getOrCreateArray(elts));
      diStructs_[name] = out;
    }
  }
  diStructActive_.erase(name);
  return out;
}

void Codegen::diBeginFunction(const FunctionDecl &fn) {
  if (!dib_)
    return;
  auto it = diFiles_.find(fn.loc.file);
  llvm::DIFile *file = nullptr;
  if (it != diFiles_.end()) {
    file = it->second;
  } else {
    // Pisah direktori + basename agar DW_AT_comp_dir benar.
    std::string dir = ".";
    std::string base = fn.loc.file;
    size_t slash = fn.loc.file.find_last_of("/\\");
    if (slash != std::string::npos) {
      dir = fn.loc.file.substr(0, slash);
      base = fn.loc.file.substr(slash + 1);
      if (dir.empty())
        dir = "/";
      if (base.empty())
        base = fn.loc.file;
    }
    file = dib_->createFile(base, dir);
    diFiles_[fn.loc.file] = file;
  }
  if (!diCU_) {
    diCU_ = dib_->createCompileUnit(
        llvm::dwarf::DW_LANG_C, file, "nusa",
        /*isOptimized=*/false, /*Flags=*/"", /*RV=*/0);
  }
  // Tipe subroutine: ret + params (gunakan pemetaan skalar; fallback
  // i64 untuk yang tak terwakili agar linkage tetap benar).
  llvm::SmallVector<llvm::Metadata *, 8> tys;
  tys.push_back(diType(fn.retType) ? diType(fn.retType)
                                   : dib_->createBasicType(
                                         "i64", 64,
                                         llvm::dwarf::DW_ATE_signed));
  for (const auto &p : fn.params) {
    llvm::DIType *pt = diType(p.type);
    if (!pt)
      pt = dib_->createBasicType("i64", 64, llvm::dwarf::DW_ATE_signed);
    tys.push_back(pt);
  }
  llvm::DISubroutineType *subTy =
      dib_->createSubroutineType(dib_->getOrCreateTypeArray(tys));
  diSP_ = dib_->createFunction(
      file, fn.name, fn.name, file, (unsigned)fn.loc.line, subTy,
      (unsigned)fn.loc.line, llvm::DINode::FlagZero,
      llvm::DISubprogram::SPFlagDefinition);
  curFn_->setSubprogram(diSP_);
  diSetLoc(fn.loc);
}

void Codegen::diSetLoc(const Location &loc) {
  if (!dib_ || !diSP_)
    return;
  builder_.SetCurrentDebugLocation(
      llvm::DILocation::get(*ctx_, (unsigned)loc.line,
                            (unsigned)loc.column, diSP_));
}

void Codegen::diDeclare(llvm::AllocaInst *slot, const std::string &name,
                        const Type &ty, const Location &loc, bool isParam,
                        unsigned argNo) {
  if (!dib_ || !diSP_)
    return;
  llvm::DIType *dt = nullptr;
  if (ty.kind == TypeKind::Array && ty.elem) {
    // Bound dari tipe alokasi LLVM (ukuran literal, dikenal statis).
    llvm::Type *allocTy = slot ? slot->getAllocatedType() : nullptr;
    if (auto *arrTy = llvm::dyn_cast_or_null<llvm::ArrayType>(allocTy)) {
      llvm::DIType *elemTy = diType(*ty.elem);
      uint64_t esz = 0, eal = 8;
      if (ty.elem->kind == TypeKind::I64) {
        esz = 64;
        eal = 64;
      } else if (ty.elem->kind == TypeKind::Bool) {
        esz = 8;
        eal = 8;
      } else if (ty.elem->kind == TypeKind::String) {
        esz = 64;
        eal = 64;
      }
      if (elemTy && esz > 0) {
        uint64_t n = arrTy->getNumElements();
        llvm::SmallVector<llvm::Metadata *, 1> subs;
        subs.push_back(dib_->getOrCreateSubrange(0, (int64_t)n));
        dt = dib_->createArrayType(n * esz, (uint32_t)eal, elemTy,
                                   dib_->getOrCreateArray(subs));
      }
    }
    if (!dt)
      return; // elemen tak terwakili: lewati (konsisten skalar)
  } else {
    dt = diType(ty);
    if (!dt)
      return; // tak terwakili: lewati
  }
  auto it = diFiles_.find(loc.file);
  llvm::DIFile *file = (it == diFiles_.end()) ? nullptr : it->second;
  if (!file)
    return;
  llvm::DILocalVariable *var = nullptr;
  if (isParam) {
    var = dib_->createParameterVariable(
        diSP_, name, argNo, file, (unsigned)loc.line, dt, true);
  } else {
    var = dib_->createAutoVariable(diSP_, name, file, (unsigned)loc.line, dt,
                                   true);
  }
  dib_->insertDeclare(slot, var, dib_->createExpression(),
                      llvm::DILocation::get(*ctx_, (unsigned)loc.line,
                                            (unsigned)loc.column, diSP_),
                      builder_.GetInsertBlock());
}

void Codegen::diFinalize() {
  if (!dib_)
    return;
  dib_->finalize();
  // Sabuk + suspender: pastikan flag versi ada (tanpanya clang
  // mengabaikan seluruh debug info sebagai "invalid version (0)").
  if (!module_->getModuleFlag("Debug Info Version")) {
    module_->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                           llvm::DEBUG_METADATA_VERSION);
  }
}

llvm::Type *Codegen::aggregateType(const GenValue &val) {  if (val.kind == ValueKind::Array)
    return arrayTypeOf(llvmScalar(val.arrayElem), val.arrayLen);
  if (val.kind == ValueKind::Struct && val.structName) {
    const StructLayout *layout = findLayout(*val.structName);
    if (layout)
      return layout->ty;
  }
  return nullptr;
}

bool Codegen::storeGenValue(const GenValue &val, llvm::AllocaInst *slot,
                            const Location &loc, std::string &errOut) {
  if (val.kind == ValueKind::Array || val.kind == ValueKind::Struct) {
    llvm::Type *aggTy = aggregateType(val);
    if (!aggTy || slot->getAllocatedType() != aggTy) {
      errOut = at(loc) + ": internal error: aggregate type mismatch";
      return false;
    }
    llvm::Value *loaded = builder_.CreateLoad(aggTy, val.value, "aggcpy");
    builder_.CreateStore(loaded, slot);
    return true;
  }
  if (val.value->getType() != slot->getAllocatedType()) {
    errOut = at(loc) + ": internal error: type mismatch on store";
    return false;
  }
  builder_.CreateStore(val.value, slot);
  return true;
}

bool Codegen::generate(const Program &prog, std::string &errOut) {
  if (!prog.hasMain()) {
    errOut = sourceFile_ + ": error: no 'fn main' found";
    return false;
  }
  // Triple eksplisit untuk cross-compilation (Phase 33). Default
  // kosong = clang menyimpulkan host (perilaku lama, tak tersentuh).
  if (!targetTriple_.empty())
    module_->setTargetTriple(llvm::Triple(targetTriple_));
  bindings_.clear();
  functions_.clear();
  funcRetKind_.clear();
  funcTypes_.clear();
  tramps_.clear();
  // State debug (Phase 37): segar per generate.
  dib_.reset();
  diCU_ = nullptr;
  diSP_ = nullptr;
  diFiles_.clear();
  diI64_ = diBool_ = diStr_ = nullptr;
  if (debug_)
    dib_ = std::make_unique<llvm::DIBuilder>(*module_);
  closurePairTy_ = llvm::StructType::create(
      *ctx_, {builder_.getPtrTy(), builder_.getPtrTy()}, "__nusa_closure");
  intFmt_ = nullptr;
  curFn_ = nullptr;

  // Pass 0: named struct types (v0.8.0).
  if (!buildStructLayouts(prog, errOut))
    return false;
  // declare i32 @puts(ptr) — opaque pointer (LLVM 21).
  llvm::FunctionType *putsTy = llvm::FunctionType::get(
      builder_.getInt32Ty(), {builder_.getPtrTy()}, false);
  module_->getOrInsertFunction("puts", putsTy);
  // declare i32 @printf(ptr, ...) untuk mencetak integer.
  llvm::FunctionType *printfTy = llvm::FunctionType::get(
      builder_.getInt32Ty(), {builder_.getPtrTy()}, true);
  module_->getOrInsertFunction("printf", printfTy);
  // declare void @exit(i32) untuk bounds-check failure (v0.7.0).
  llvm::FunctionType *exitTy = llvm::FunctionType::get(
      builder_.getVoidTy(), {builder_.getInt32Ty()}, false);
  module_->getOrInsertFunction("exit", exitTy);
  // Global argv (v1.10.0): diisi saat main mulai; dibaca builtin
  // process_argc/argv. Dibuat di sini (bukan saat gen main) karena
  // fungsi std di-generate SEBELUM main.
  new llvm::GlobalVariable(
      *module_, builder_.getInt32Ty(), false,
      llvm::GlobalValue::InternalLinkage,
      llvm::ConstantInt::get(builder_.getInt32Ty(), 0), "nusa_argc");
  new llvm::GlobalVariable(
      *module_, builder_.getPtrTy(), false,
      llvm::GlobalValue::InternalLinkage,
      llvm::ConstantPointerNull::get(builder_.getPtrTy()), "nusa_argv");

  // Pass 1: deklarasikan semua fungsi (rekursi & forward reference).
  for (const auto &fn : prog.functions) {
    llvm::Function *llvmFn = nullptr;
    if (fn.name == "main") {      // Entry point executable: i32 @main(i32, ptr) (return 0 dari driver).
      // argv disimpan ke global untuk builtin process_argc/argv (v1.10.0).
      llvm::FunctionType *mainTy = llvm::FunctionType::get(
          builder_.getInt32Ty(),
          {builder_.getInt32Ty(), builder_.getPtrTy()}, false);
      llvmFn = llvm::Function::Create(mainTy, llvm::Function::ExternalLinkage,
                                      "main", module_.get());
      funcRetKind_["main"] = ValueKind::Void; // Nusa: void (tak callable)
    } else {
      // v0.7.0: tanpa parameter/return array
      // (ditolak typechecker; guard anti-crash bila dipanggil langsung).
      // Struct by-value didukung (Phase 21d); layout harus ada.
      for (const auto &p : fn.params) {
        if (p.type.kind == TypeKind::Array) {
          errOut = at(p.loc) + ": internal error: array parameter '" +
                   p.name + "'";
          return false;
        }
        if (p.type.kind == TypeKind::Struct &&
            (!p.type.sname || !findLayout(*p.type.sname))) {
          errOut = at(p.loc) + ": internal error: unknown struct layout";
          return false;
        }
      }
      if (fn.retType.kind == TypeKind::Array) {
        errOut = at(fn.loc) + ": internal error: array return type";
        return false;
      }
      if (fn.retType.kind == TypeKind::Struct &&
          (!fn.retType.sname || !findLayout(*fn.retType.sname))) {
        errOut = at(fn.loc) + ": internal error: unknown struct layout";
        return false;
      }
      std::vector<llvm::Type *> paramTys;
      for (const auto &p : fn.params)
        paramTys.push_back(fn.isExtern ? ffiType(p.type) : llvmType(p.type));
      llvm::FunctionType *fnTy = llvm::FunctionType::get(
          fn.isExtern ? ffiType(fn.retType) : llvmType(fn.retType), paramTys,
          false);
      llvmFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                      fn.name, module_.get());
      funcRetKind_[fn.name] = kindOf(fn.retType);
      if (fn.isExtern)
        externFns_.insert(fn.name);
    }
    functions_[fn.name] = llvmFn;
    // Signature Nusa untuk function reference (Phase 21b).
    std::vector<Type> pTys;
    for (const auto &p : fn.params)
      pTys.push_back(p.type);
    funcTypes_[fn.name] = Type::func(std::move(pTys), fn.retType);
  }

  // Pass 2: bangkitkan bodies (extern dilewati: deklarasi saja).
  for (const auto &fn : prog.functions) {
    if (fn.isExtern)
      continue;
    if (!genFunction(fn, errOut))
      return false;
  }

  // WAJIB: verifikasi module sebelum menghasilkan machine code.
  std::string verifyErr;
  diFinalize(); // debug info harus final sebelum verifikasi
  llvm::raw_string_ostream verifyStream(verifyErr);
  if (llvm::verifyModule(*module_, &verifyStream)) {
    verifyStream.flush();
    errOut = sourceFile_ + ": error: LLVM module verification failed:\n" +
             verifyErr;
    return false;
  }
  return true;
}

bool Codegen::genFunction(const FunctionDecl &fn, std::string &errOut) {
  auto it = functions_.find(fn.name);
  if (it == functions_.end()) {
    errOut = at(fn.loc) + ": internal error: missing LLVM declaration for '" +
             fn.name + "'";
    return false;
  }
  curFn_ = it->second;
  bindings_.clear();
  bindings_.push(); // scope fungsi

  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(*ctx_, "entry", curFn_);
  builder_.SetInsertPoint(entry);
  diBeginFunction(fn);

  if (fn.name == "main") {
    // Simpan argc/argv ke global (builtin process_argc/argv, v1.10.0).
    auto argIt = curFn_->arg_begin();
    llvm::Value *argc = &*argIt++;
    llvm::Value *argv = &*argIt;
    llvm::GlobalVariable *gargc = module_->getNamedGlobal("nusa_argc");
    llvm::GlobalVariable *gargv = module_->getNamedGlobal("nusa_argv");
    // (Dibuat di generate(); pertahanan bila dipanggil langsung.)
    if (gargc)
      builder_.CreateStore(argc, gargc);
    if (gargv)
      builder_.CreateStore(argv, gargv);
    bool terminated = false;
    if (!genBlock(fn.body, terminated, errOut))
      return false;
    // `return;` polos di main legal → ret 0 hanya bila belum terminate.
    if (!terminated)
      builder_.CreateRet(builder_.getInt32(0));
    return true;
  }

  // Parameter → alloca + store (pola standar; debugging & mutasi aman).
  size_t i = 0;
  for (auto &arg : curFn_->args()) {
    const Param &param = fn.params[i++];
    arg.setName(param.name);
    llvm::AllocaInst *slot = entryAlloca(llvmType(param.type), param.name);
    builder_.CreateStore(&arg, slot);
    Binding pb{slot, kindOf(param.type)};
    if (pb.kind == ValueKind::Func)
      pb.funcType = param.type; // signature untuk indirect call
    if (pb.kind == ValueKind::Struct && param.type.sname)
      pb.structName = param.type.sname; // layout untuk field access
    bindings_.insert(param.name, pb);
    diDeclare(slot, param.name, param.type, param.loc, true,
              (unsigned)i); // formal parameter 1-based
  }
  bool terminated = false;
  if (!genBlock(fn.body, terminated, errOut))
    return false;
  // Fungsi void tanpa `return;` eksplisit: tutup dengan ret void.
  // (Non-void dijamin typechecker selalu me-return; merge yang tak
  // terjangkau ditutup unreachable agar IR tetap valid.)
  if (!builder_.GetInsertBlock()->getTerminator()) {
    if (fn.retType.kind == TypeKind::Void)
      builder_.CreateRetVoid();
    else
      builder_.CreateUnreachable();
  }
  return true;
}

// Bangkitkan satu Block; tiap Block = scope leksikal baru.
// terminated=true bila block diakhiri terminator (return).
bool Codegen::genBlock(const Block &block, bool &terminated,
                       std::string &errOut) {
  bindings_.push();
  terminated = false;
  for (const auto &stmt : block.stmts) {
    // Berhenti diam-diam di terminator: diagnostic unreachable milik
    // typechecker (driver tak pernah sampai sini bila checker gagal).
    if (builder_.GetInsertBlock()->getTerminator()) {
      terminated = true;
      break;
    }
    if (!genStmt(stmt, errOut)) {
      bindings_.pop();
      return false;
    }
  }
  if (builder_.GetInsertBlock()->getTerminator())
    terminated = true;
  bindings_.pop();
  return true;
}

bool Codegen::genIf(const IfStmt &stmt, std::string &errOut) {
  GenValue cond;
  if (!genExpr(*stmt.cond, cond, errOut))
    return false;
  if (cond.kind != ValueKind::Bool ||
      !cond.value->getType()->isIntegerTy(1)) {
    // Seharusnya ditolak typechecker; pertahanan anti-crash.
    errOut = at(stmt.cond->loc()) +
             ": internal error: if condition is not boolean";
    return false;
  }
  const bool hasElse = (stmt.elseBlock != nullptr);
  llvm::BasicBlock *thenBB =
      llvm::BasicBlock::Create(*ctx_, "then", curFn_);
  llvm::BasicBlock *elseBB =
      hasElse ? llvm::BasicBlock::Create(*ctx_, "else", curFn_) : nullptr;
  llvm::BasicBlock *mergeBB =
      llvm::BasicBlock::Create(*ctx_, "merge", curFn_);
  builder_.CreateCondBr(cond.value, thenBB, hasElse ? elseBB : mergeBB);

  builder_.SetInsertPoint(thenBB);
  bool thenTerm = false;
  if (!genBlock(*stmt.thenBlock, thenTerm, errOut))
    return false;
  if (!thenTerm)
    builder_.CreateBr(mergeBB);

  if (hasElse) {
    builder_.SetInsertPoint(elseBB);
    bool elseTerm = false;
    if (!genBlock(*stmt.elseBlock, elseTerm, errOut))
      return false;
    if (!elseTerm)
      builder_.CreateBr(mergeBB);
  }
  builder_.SetInsertPoint(mergeBB);
  return true;
}

bool Codegen::genWhile(const WhileStmt &stmt, std::string &errOut) {
  llvm::BasicBlock *condBB =
      llvm::BasicBlock::Create(*ctx_, "loop.cond", curFn_);
  llvm::BasicBlock *bodyBB =
      llvm::BasicBlock::Create(*ctx_, "loop.body", curFn_);
  llvm::BasicBlock *afterBB =
      llvm::BasicBlock::Create(*ctx_, "loop.after", curFn_);
  builder_.CreateBr(condBB);

  builder_.SetInsertPoint(condBB);
  GenValue cond;
  if (!genExpr(*stmt.cond, cond, errOut))
    return false;
  if (cond.kind != ValueKind::Bool ||
      !cond.value->getType()->isIntegerTy(1)) {
    // Seharusnya ditolak typechecker; pertahanan anti-crash.
    errOut = at(stmt.cond->loc()) +
             ": internal error: while condition is not boolean";
    return false;
  }
  builder_.CreateCondBr(cond.value, bodyBB, afterBB);

  builder_.SetInsertPoint(bodyBB);
  bool bodyTerm = false;
  if (!genBlock(*stmt.body, bodyTerm, errOut))
    return false;
  if (!bodyTerm)
    builder_.CreateBr(condBB);

  builder_.SetInsertPoint(afterBB);
  return true;
}

bool Codegen::genStmt(const Stmt &stmt, std::string &errOut) {
  if (builder_.GetInsertBlock() && builder_.GetInsertBlock()->getTerminator()) {
    // Seharusnya ditolak typechecker (unreachable); pertahanan anti-crash.
    const Location &loc = std::visit(
        [](const auto &s) -> const Location & { return s.loc; }, stmt);
    errOut = at(loc) + ": internal error: unreachable code";
    return false;
  }
  // Lokasi sumber per statement (Phase 37): line table + breakpoint.
  {
    const Location &loc = std::visit(
        [](const auto &s) -> const Location & { return s.loc; }, stmt);
    diSetLoc(loc);
  }
  if (std::holds_alternative<LetStmt>(stmt)) {
    const auto &let = std::get<LetStmt>(stmt);
    GenValue val;
    if (!genExpr(*let.value, val, errOut))
      return false;
    // Slot: tipe agregat untuk array/struct, tipe value untuk skalar.
    llvm::Type *slotTy = (val.kind == ValueKind::Array ||
                          val.kind == ValueKind::Struct)
                             ? aggregateType(val)
                             : val.value->getType();
    if (!slotTy) {
      errOut = at(let.loc) + ": internal error: cannot type variable '" +
               let.name + "'";
      return false;
    }
    llvm::AllocaInst *slot = entryAlloca(slotTy, let.name);
    if (!storeGenValue(val, slot, let.loc, errOut))
      return false;
    Binding b{slot, val.kind, val.arrayElem, val.arrayLen, val.structName};
    if (val.kind == ValueKind::Func)
      b.funcType = val.funcType; // signature untuk indirect call
    // Shadowing antar-scope didukung Scopes; duplikat se-scope sudah
    // ditolak semantic (set = pertahanan bila checker dilewati).
    if (!bindings_.insert(let.name, b))
      bindings_.set(let.name, b);
    // Variabel lokal untuk inspeksi debugger (Phase 64: skalar +
    // struct + array; Callable/fn dilewati).
    {
      Type dt = Type::error();
      if (val.kind == ValueKind::Int)
        dt = Type::i64();
      else if (val.kind == ValueKind::Bool)
        dt = Type::boolean();
      else if (val.kind == ValueKind::String)
        dt = Type::string();
      else if (val.kind == ValueKind::Struct && val.structName)
        dt = Type::named(*val.structName);
      else if (val.kind == ValueKind::Array) {
        Type et = Type::i64();
        if (val.arrayElem == ValueKind::Bool)
          et = Type::boolean();
        else if (val.arrayElem == ValueKind::String)
          et = Type::string();
        dt = Type::array(et);
      }
      if (!dt.isError())
        diDeclare(slot, let.name, dt, let.loc);
    }
    return true;
  }
  if (std::holds_alternative<PrintStmt>(stmt))
    return emitPrint(std::get<PrintStmt>(stmt), errOut);
  if (std::holds_alternative<ReturnStmt>(stmt))
    return emitReturn(std::get<ReturnStmt>(stmt), errOut);
  if (std::holds_alternative<IfStmt>(stmt))
    return genIf(std::get<IfStmt>(stmt), errOut);
  if (std::holds_alternative<WhileStmt>(stmt))
    return genWhile(std::get<WhileStmt>(stmt), errOut);
  if (std::holds_alternative<AssignStmt>(stmt)) {
    const auto &as = std::get<AssignStmt>(stmt);
    auto found = bindings_.lookup(as.name);
    if (!found) {
      // Seharusnya sudah ditolak semantic; pertahanan anti-crash.
      errOut = at(as.loc) + ": error: undefined variable '" + as.name + "'";
      return false;
    }
    GenValue val;
    if (!genExpr(*as.value, val, errOut))
      return false;
    if (!storeGenValue(val, found->slot, as.loc, errOut))
      return false;
    return true;
  }
  if (std::holds_alternative<MatchStmt>(stmt)) {
    // Seharusnya didesugar typechecker; pertahanan anti-crash.
    const auto &ms = std::get<MatchStmt>(stmt);
    errOut = at(ms.loc) + ": internal error: undesugared match statement";
    return false;
  }
  // ExprStmt: evaluasi lalu buang nilainya.
  GenValue discarded;
  return genExpr(*std::get<ExprStmt>(stmt).expr, discarded, errOut);
}

bool Codegen::emitReturn(const ReturnStmt &stmt, std::string &errOut) {
  if (!stmt.value) {
    // Bare `return;`: `ret void` — KECUALI di @main yang bertipe i32
    // (entry point executable; Nusa-void). Tanpa ini, `if c { return; }`
    // di main gagal verifikasi IR (ret void vs i32). Temuan Phase 44.
    if (curFn_ && curFn_->getName() == "main") {
      builder_.CreateRet(builder_.getInt32(0));
      return true;
    }
    builder_.CreateRetVoid();
    return true;
  }
  GenValue val;
  if (!genExpr(*stmt.value, val, errOut))
    return false;
  if (val.kind == ValueKind::Void) {
    errOut = at(stmt.loc) + ": internal error: cannot return 'void' value";
    return false;
  }
  if (val.kind == ValueKind::Struct) {
    // Return struct by-value (Phase 21d): load dari storage.
    llvm::Type *aggTy = aggregateType(val);
    if (!aggTy) {
      errOut = at(stmt.loc) + ": internal error: unknown struct layout";
      return false;
    }
    llvm::Value *loaded = builder_.CreateLoad(aggTy, val.value, "retv");
    builder_.CreateRet(loaded);
    return true;
  }
  builder_.CreateRet(val.value);
  return true;
}

bool Codegen::genExpr(const Expr &expr, GenValue &out, std::string &errOut) {
  if (++exprDepth_ > kMaxExprDepth) {
    --exprDepth_;
    errOut = at(expr.loc()) +
             ": error: expression too deeply nested\n"
             "help: simplify this nested expression";
    return false;
  }
  const Location &loc = expr.loc();
  struct Visitor {
    Codegen *self;
    GenValue &out;
    std::string &errOut;
    const Location &loc;
    bool ok = true;

    bool fail(const std::string &msg, const std::string &hint = "") {
      ok = false;
      errOut = Codegen::at(loc) + ": error: " + msg;
      if (!hint.empty())
        errOut += "\nhint: " + hint;
      return false;
    }

    void operator()(const Expr::LiteralInt &n) {
      out.value = llvm::ConstantInt::get(self->builder_.getInt64Ty(),
                                         static_cast<uint64_t>(n.value), true);
      out.kind = ValueKind::Int;
    }
    void operator()(const Expr::LiteralBool &n) {
      out.value = llvm::ConstantInt::get(self->builder_.getInt1Ty(),
                                         n.value ? 1 : 0, false);
      out.kind = ValueKind::Bool;
    }
    void operator()(const Expr::LiteralString &n) {
      out.value = self->getStringPtr(n.value);
      out.kind = ValueKind::String;
    }
    void operator()(const Expr::VarRef &n) {
      auto found = self->bindings_.lookup(n.name);
      if (!found) {
        // Referensi fungsi sebagai value (Phase 21b): function pointer.
        // (Seharusnya lolos semantic/checker; pertahanan anti-crash.)
        auto fnIt = self->functions_.find(n.name);
        auto tyIt = self->funcTypes_.find(n.name);
        if (fnIt == self->functions_.end() ||
            tyIt == self->funcTypes_.end()) {
          fail("undefined variable '" + n.name + "'",
               "declare '" + n.name + "' before using it");
          return;
        }
        // Decay Function* (FunctionType) ke pointer agar bisa di-store/
        // load/call tak langsung secara seragam.
        // Closure pair (Phase 27): `{trampoline, null-env}` di heap
        // program-lifetime; panggil selalu via trampolin `(ptr, ...)`.
        llvm::Function *tramp = self->getTrampoline(n.name, tyIt->second,
                                                    "", errOut);
        if (!tramp) {
          fail(errOut);
          return;
        }
        llvm::Value *pair =
            self->mallocFor(self->closurePairTy_, n.name + ".clos");
        if (!pair) {
          fail("internal error: cannot allocate closure");
          return;
        }
        llvm::Value *tptr = self->builder_.CreateStructGEP(
            self->closurePairTy_, pair, 0, n.name + ".tptr");
        self->builder_.CreateStore(tramp, tptr);
        llvm::Value *eptr = self->builder_.CreateStructGEP(
            self->closurePairTy_, pair, 1, n.name + ".eptr");
        self->builder_.CreateStore(
            llvm::ConstantPointerNull::get(self->builder_.getPtrTy()), eptr);
        out.value = pair;
        out.kind = ValueKind::Func;
        out.funcType = tyIt->second;
        return;
      }
      if (found->kind == ValueKind::Array) {
        // Array = pointer ke alloca (tanpa load).
        out.value = found->slot;
        out.kind = ValueKind::Array;
        out.arrayElem = found->arrayElem;
        out.arrayLen = found->arrayLen;
        return;
      }
      if (found->kind == ValueKind::Struct) {
        // Struct = pointer ke storage (tanpa load).
        out.value = found->slot;
        out.kind = ValueKind::Struct;
        out.structName = found->structName;
        return;
      }
      out.value = self->builder_.CreateLoad(
          found->slot->getAllocatedType(), found->slot, n.name);
      out.kind = found->kind;
      // Signature fn ikut (Phase 27; cermin FieldAccess) agar
      // `let g = f; g(...)` tetap callable.
      if (found->kind == ValueKind::Func)
        out.funcType = found->funcType;
    }
    void operator()(const Expr::ArrayLit &n) {
      // Materialisasi: alloca [N x T] di entry + store tiap elemen.
      // (Literal kosong tak sampai sini: ditolak typechecker.)
      if (n.elems.empty()) {
        fail("internal error: empty array literal needs annotation");
        return;
      }
      struct ElemVal {
        llvm::Value *v;
        ValueKind kind;
      };
      std::vector<ElemVal> vals;
      for (const auto &e : n.elems) {
        GenValue g;
        if (!self->genExpr(*e, g, errOut)) {
          ok = false;
          return;
        }
        if (g.kind == ValueKind::Array || g.kind == ValueKind::Void ||
            g.kind == ValueKind::Func) {
          fail("internal error: invalid array element");
          return;
        }
        vals.push_back({g.value, g.kind});
      }
      llvm::Type *elemTy = vals[0].v->getType();
      for (size_t i = 1; i < vals.size(); i++) {
        if (vals[i].v->getType() != elemTy) {
          fail("mismatched array element types",
               "type checker should have rejected this program");
          return;
        }
      }
      llvm::ArrayType *arrTy = arrayTypeOf(
          elemTy, static_cast<uint64_t>(vals.size()));
      llvm::AllocaInst *slot = self->entryAlloca(arrTy, "arr");
      llvm::Value *zero = self->builder_.getInt32(0);
      for (size_t i = 0; i < vals.size(); i++) {
        llvm::Value *idx = self->builder_.getInt32(
            static_cast<uint32_t>(i));
        llvm::Value *slotPtr = self->builder_.CreateInBoundsGEP(
            arrTy, slot, {zero, idx});
        self->builder_.CreateStore(vals[i].v, slotPtr);
      }
      out.value = slot;
      out.kind = ValueKind::Array;
      out.arrayElem = vals[0].kind;
      out.arrayLen = static_cast<uint64_t>(vals.size());
    }
    void operator()(const Expr::Index &n) {
      GenValue base, idx;
      if (!self->genExpr(*n.base, base, errOut) ||
          !self->genExpr(*n.index, idx, errOut)) {
        ok = false;
        return;
      }
      if (base.kind != ValueKind::Array) {
        fail("internal error: indexing non-array value");
        return;
      }
      if (idx.kind != ValueKind::Int) {
        fail("internal error: non-integer array index");
        return;
      }
      // Runtime bounds check: idx < 0 || idx >= len → pesan + exit(1).
      uint64_t len = base.arrayLen;
      llvm::Value *lenVal = llvm::ConstantInt::get(
          self->builder_.getInt64Ty(), len, false);
      llvm::Value *neg = self->builder_.CreateICmpSLT(
          idx.value, self->builder_.getInt64(0), "negidx");
      llvm::Value *oob = self->builder_.CreateICmpSGE(
          idx.value, lenVal, "oob");
      llvm::Value *bad =
          self->builder_.CreateOr(neg, oob, "badidx");
      llvm::Function *curFn = self->builder_.GetInsertBlock()->getParent();
      llvm::BasicBlock *failBB = llvm::BasicBlock::Create(
          self->builder_.getContext(), "oob.fail", curFn);
      llvm::BasicBlock *okBB = llvm::BasicBlock::Create(
          self->builder_.getContext(), "oob.ok", curFn);
      self->builder_.CreateCondBr(bad, failBB, okBB);
      self->builder_.SetInsertPoint(failBB);
      self->builder_.CreateCall(self->module_->getFunction("puts"),
                                {self->getStringPtr(
                                    "runtime error: index out of bounds")});
      self->builder_.CreateCall(self->module_->getFunction("exit"),
                                {self->builder_.getInt32(1)});
      self->builder_.CreateUnreachable();
      self->builder_.SetInsertPoint(okBB);
      llvm::Type *elemTy = self->llvmScalar(base.arrayElem);
      llvm::ArrayType *arrTy = arrayTypeOf(elemTy, len);
      llvm::Value *zero = self->builder_.getInt32(0);
      llvm::Value *elemPtr = self->builder_.CreateInBoundsGEP(
          arrTy, base.value, {zero, idx.value});
      out.value = self->builder_.CreateLoad(elemTy, elemPtr, "idx");
      out.kind = base.arrayElem;
    }
    void operator()(const Expr::StructLit &n) {
      // Materialisasi: alloca named struct + store tiap field (diurutkan
      // deklarasi). Field agregat (nested struct) via load+store.
      const Codegen::StructLayout *layout = self->findLayout(n.name);
      if (!layout) {
        // Seharusnya ditolak semantic; pertahanan anti-crash.
        fail("undefined struct '" + n.name + "'");
        return;
      }
      llvm::AllocaInst *slot = self->entryAlloca(layout->ty, n.name);
      for (const auto &field : layout->fields) {
        const Expr::StructLit::FieldInit *init = nullptr;
        for (const auto &f : n.fields) {
          if (f.name == field.name) {
            init = &f;
            break;
          }
        }
        if (!init) {
          // Hidden-enum (Phase 26): slot varian non-aktif tak diisi
          // (undef; sound karena match hanya membaca slot varian yang
          // cocok dengan `tag`, dan `==` struct ditolak checker).
          if (layout->hiddenEnum)
            continue;
          fail("internal error: missing field '" + field.name + "'");
          return;
        }
        GenValue g;
        if (!self->genExpr(*init->value, g, errOut)) {
          ok = false;
          return;
        }
        unsigned fi =
            static_cast<unsigned>(&field - layout->fields.data());
        llvm::Value *fieldPtr =
            self->builder_.CreateStructGEP(layout->ty, slot, fi);
        if (g.kind == ValueKind::Array) {
          fail("internal error: array struct field '" + field.name +
               "' not supported");
          return;
        }
        if (g.kind == ValueKind::Struct) {
          // Nested struct: copy agregat (checker menjamin tipe cocok).
          llvm::Type *aggTy = self->aggregateType(g);
          if (!aggTy || aggTy != field.ty) {
            fail("internal error: field type mismatch for '" + field.name +
                 "'");
            return;
          }
          llvm::Value *loaded =
              self->builder_.CreateLoad(aggTy, g.value, "fldcpy");
          self->builder_.CreateStore(loaded, fieldPtr);
          continue;
        }
        if (g.value->getType() != field.ty) {
          fail("internal error: field type mismatch for '" + field.name +
               "'");
          return;
        }
        self->builder_.CreateStore(g.value, fieldPtr);
      }
      out.value = slot;
      out.kind = ValueKind::Struct;
      out.structName = n.name;
    }
    void operator()(const Expr::FieldAccess &n) {
      GenValue base;
      if (!self->genExpr(*n.base, base, errOut)) {
        ok = false;
        return;
      }
      if (base.kind != ValueKind::Struct || !base.structName) {
        fail("internal error: field access on non-struct value");
        return;
      }
      const Codegen::StructLayout *layout =
          self->findLayout(*base.structName);
      if (!layout) {
        fail("internal error: unknown struct layout");
        return;
      }
      const Codegen::FieldLayout *found = nullptr;
      for (const auto &f : layout->fields) {
        if (f.name == n.field) {
          found = &f;
          break;
        }
      }
      if (!found) {
        fail("internal error: unknown field '" + n.field + "'");
        return;
      }
      unsigned fi =
          static_cast<unsigned>(found - layout->fields.data());
      llvm::Value *fieldPtr =
          self->builder_.CreateStructGEP(layout->ty, base.value, fi);
      if (found->kind == ValueKind::Struct) {
        // Field struct bersarang = pointer (tanpa load) agar `.c` lanjut.
        out.value = fieldPtr;
        out.kind = ValueKind::Struct;
        out.structName = found->structName;
        return;
      }
      out.value =
          self->builder_.CreateLoad(found->ty, fieldPtr, n.field);
      out.kind = found->kind;
      // Capture fn (Phase 27): signature ikut agar callable.
      if (found->kind == ValueKind::Func)
        out.funcType = found->funcType;
    }
    void operator()(const Expr::MethodCall &n) {
      // Seharusnya ditulis ulang checker menjadi Call biasa
      // (dispatch statis); pertahanan anti-crash.
      (void)n;
      fail("internal error: unresolved method call");
      return;
    }
    void operator()(const Expr::Lambda &n) {
      // Seharusnya di-lift checker menjadi Closure; pertahanan anti-crash.
      (void)n;
      fail("internal error: unlifted lambda");
      return;
    }
    void operator()(const Expr::Closure &n) {
      // Materialisasi closure (Phase 27): malloc env + isi capture,
      // lalu pair `{trampoline, env}` (heap program-lifetime).
      const StructLayout *layout = self->findLayout(n.envStruct);
      if (!layout) {
        fail("internal error: unknown closure env '" + n.envStruct + "'");
        return;
      }
      if (n.captures.size() != layout->fields.size()) {
        fail("internal error: closure/env arity mismatch");
        return;
      }
      llvm::Value *envPtr = self->mallocFor(layout->ty, "cenv");
      if (!envPtr) {
        fail("internal error: cannot allocate closure env");
        return;
      }
      for (size_t i = 0; i < n.captures.size(); i++) {
        GenValue g;
        if (!self->genExpr(*n.captures[i], g, errOut)) {
          ok = false;
          return;
        }
        llvm::Value *fieldPtr = self->builder_.CreateStructGEP(
            layout->ty, envPtr, static_cast<unsigned>(i), "cap");
        const FieldLayout &fl = layout->fields[i];
        if (g.kind == ValueKind::Struct) {
          llvm::Type *aggTy = self->aggregateType(g);
          if (!aggTy || aggTy != fl.ty) {
            fail("internal error: closure capture type mismatch");
            return;
          }
          llvm::Value *loaded =
              self->builder_.CreateLoad(aggTy, g.value, "capcpy");
          self->builder_.CreateStore(loaded, fieldPtr);
          continue;
        }
        if (g.value->getType() != fl.ty) {
          fail("internal error: closure capture type mismatch");
          return;
        }
        self->builder_.CreateStore(g.value, fieldPtr);
      }
      llvm::Function *tramp = self->getTrampoline(n.lifted, n.funcType,
                                                  n.envStruct, errOut);
      if (!tramp) {
        fail(errOut);
        return;
      }
      llvm::Value *pair =
          self->mallocFor(self->closurePairTy_, "clos");
      if (!pair) {
        fail("internal error: cannot allocate closure");
        return;
      }
      llvm::Value *tptr = self->builder_.CreateStructGEP(
          self->closurePairTy_, pair, 0, "ctptr");
      self->builder_.CreateStore(tramp, tptr);
      llvm::Value *eptr = self->builder_.CreateStructGEP(
          self->closurePairTy_, pair, 1, "ceptr");
      self->builder_.CreateStore(envPtr, eptr);
      out.value = pair;
      out.kind = ValueKind::Func;
      out.funcType = n.funcType;
    }
    void operator()(const Expr::Propagate &n) {
      // `expr?` (Phase 24; enum natif Phase 45/B1): branch is_ok/Err →
      // return operan utuh (checker menjamin tipe sama dengan return
      // fungsi). Enum: uji `tag == okTag`, unwrap slot Ok.
      GenValue g;
      if (!self->genExpr(*n.expr, g, errOut)) {
        ok = false;
        return;
      }
      if (g.kind != ValueKind::Struct || !g.structName) {
        fail("internal error: '?' on non-struct value");
        return;
      }
      const Codegen::StructLayout *layout =
          self->findLayout(*g.structName);
      if (!layout) {
        fail("internal error: unknown struct layout");
        return;
      }
      if (n.okTag >= 0) {
        int idxTag = -1;
        int idxVal = -1;
        for (size_t fi = 0; fi < layout->fields.size(); fi++) {
          if (layout->fields[fi].name == "tag")
            idxTag = static_cast<int>(fi);
          else if (layout->fields[fi].name == n.okSlot)
            idxVal = static_cast<int>(fi);
        }
        if (idxTag < 0 || idxVal < 0) {
          fail("internal error: '?' needs enum tag/payload fields");
          return;
        }
        llvm::Value *tagPtr = self->builder_.CreateStructGEP(
            layout->ty, g.value, static_cast<unsigned>(idxTag));
        llvm::Value *tagv = self->builder_.CreateLoad(
            self->builder_.getInt64Ty(), tagPtr, "q.tag");
        llvm::Value *isOk = self->builder_.CreateICmpEQ(
            tagv, llvm::ConstantInt::get(self->builder_.getInt64Ty(),
                                         (uint64_t)n.okTag),
            "q.isok");
        llvm::Function *curFn =
            self->builder_.GetInsertBlock()->getParent();
        llvm::BasicBlock *okBB =
            llvm::BasicBlock::Create(*self->ctx_, "q.ok", curFn);
        llvm::BasicBlock *errBB =
            llvm::BasicBlock::Create(*self->ctx_, "q.err", curFn);
        self->builder_.CreateCondBr(isOk, okBB, errBB);
        self->builder_.SetInsertPoint(errBB);
        llvm::Value *whole = self->builder_.CreateLoad(
            layout->ty, g.value, "q.whole");
        self->builder_.CreateRet(whole);
        self->builder_.SetInsertPoint(okBB);
        const Codegen::FieldLayout &fl = layout->fields[idxVal];
        llvm::Value *valPtr = self->builder_.CreateStructGEP(
            layout->ty, g.value, static_cast<unsigned>(idxVal));
        if (fl.kind == ValueKind::Array) {
          fail("internal error: '?' on array payload");
          return;
        }
        out.value = self->builder_.CreateLoad(fl.ty, valPtr, "q.val");
        out.kind = fl.kind;
        out.structName = fl.structName;
        return;
      }
      int idxOk = -1;
      int idxVal = -1;
      for (size_t fi = 0; fi < layout->fields.size(); fi++) {
        if (layout->fields[fi].name == "is_ok")
          idxOk = static_cast<int>(fi);
        else if (layout->fields[fi].name == "value")
          idxVal = static_cast<int>(fi);
      }
      if (idxOk < 0 || idxVal < 0) {
        fail("internal error: '?' needs Result fields");
        return;
      }
      llvm::Value *okPtr = self->builder_.CreateStructGEP(
          layout->ty, g.value, static_cast<unsigned>(idxOk));
      llvm::Value *okv = self->builder_.CreateLoad(
          self->builder_.getInt1Ty(), okPtr, "q.ok");
      llvm::Function *curFn =
          self->builder_.GetInsertBlock()->getParent();
      llvm::BasicBlock *okBB =
          llvm::BasicBlock::Create(*self->ctx_, "q.ok", curFn);
      llvm::BasicBlock *errBB =
          llvm::BasicBlock::Create(*self->ctx_, "q.err", curFn);
      self->builder_.CreateCondBr(okv, okBB, errBB);
      self->builder_.SetInsertPoint(errBB);
      llvm::Value *whole = self->builder_.CreateLoad(
          layout->ty, g.value, "q.whole");
      self->builder_.CreateRet(whole);
      self->builder_.SetInsertPoint(okBB);
      const Codegen::FieldLayout &fl = layout->fields[idxVal];
      llvm::Value *valPtr = self->builder_.CreateStructGEP(
          layout->ty, g.value, static_cast<unsigned>(idxVal));
      if (fl.kind == ValueKind::Array) {
        fail("internal error: '?' on array payload");
        return;
      }
      out.value =
          self->builder_.CreateLoad(fl.ty, valPtr, "q.val");
      out.kind = fl.kind;
      out.structName = fl.structName;
    }
    void operator()(const Expr::Call &n) {
      // Builtin `len(arr)` (v0.7.0): panjang statis → konstanta i64.
      if (n.callee == "len") {        if (n.args.size() != 1) {
          fail("internal error: len() needs exactly 1 argument");
          return;
        }
        GenValue g;
        if (!self->genExpr(*n.args[0], g, errOut)) {
          ok = false;
          return;
        }
        if (g.kind != ValueKind::Array) {
          fail("internal error: len() of non-array");
          return;
        }
        out.value = llvm::ConstantInt::get(self->builder_.getInt64Ty(),
                                           g.arrayLen, false);
        out.kind = ValueKind::Int;
        return;
      }
      // Builtins runtime v0.9.0 (input/assert/string/file/array-*).
      if (isBuiltin(n.callee)) {
        if (!self->genBuiltinCall(n, loc, out, errOut))
          ok = false;
        return;
      }
      auto fnIt = self->functions_.find(n.callee);
      if (fnIt == self->functions_.end()) {
        // Panggilan tak langsung via variabel fungsi (Phase 21b).
        auto bIt = self->bindings_.lookup(n.callee);
        if (!bIt || bIt->kind != ValueKind::Func || !bIt->funcType ||
            bIt->funcType->kind != TypeKind::Func || !bIt->funcType->ret) {
          // Seharusnya sudah ditolak semantic/checker; anti-crash.
          fail("undefined function '" + n.callee + "'");
          return;
        }
        const Type &sig = *bIt->funcType;
        if (sig.params.size() != n.args.size()) {
          fail("wrong number of arguments for '" + n.callee + "'");
          return;
        }
        std::vector<llvm::Value *> argVals;
        std::vector<llvm::Type *> paramTys;
        for (const auto &p : sig.params)
          paramTys.push_back(self->llvmType(p));
        for (const auto &a : n.args) {
          GenValue g;
          if (!self->genExpr(*a, g, errOut)) {
            ok = false;
            return;
          }
          argVals.push_back(g.value);
        }
        size_t j = 0;
        for (auto *pt : paramTys) {
          // Argumen struct by-value (Phase 21d): load dari storage.
          if (pt->isStructTy() && argVals[j]->getType() != pt) {
            if (!argVals[j]->getType()->isPointerTy()) {
              fail("mismatched argument type for '" + n.callee + "'");
              return;
            }
            argVals[j] = self->builder_.CreateLoad(pt, argVals[j], "argv");
          }
          if (argVals[j]->getType() != pt) {
            fail("mismatched argument type for '" + n.callee + "'");
            return;
          }
          j++;
        }
        // Panggilan tak langsung via trampolin (Phase 27): pair
        // `{trampoline, env}` di slot; trampolin `(ptr, ...)` seragam
        // untuk referensi-fungsi maupun closure.
        llvm::Value *pairPtr = self->builder_.CreateLoad(
            self->builder_.getPtrTy(), bIt->slot, n.callee + ".clos");
        llvm::Value *trampPtrPtr = self->builder_.CreateStructGEP(
            self->closurePairTy_, pairPtr, 0, n.callee + ".tptr");
        llvm::Value *trampPtr = self->builder_.CreateLoad(
            self->builder_.getPtrTy(), trampPtrPtr, n.callee + ".tramp");
        llvm::Value *envPtrPtr = self->builder_.CreateStructGEP(
            self->closurePairTy_, pairPtr, 1, n.callee + ".eptr");
        llvm::Value *envPtr = self->builder_.CreateLoad(
            self->builder_.getPtrTy(), envPtrPtr, n.callee + ".env");
        std::vector<llvm::Type *> trampTys;
        trampTys.push_back(self->builder_.getPtrTy());
        for (auto *pt : paramTys)
          trampTys.push_back(pt);
        llvm::FunctionType *trampTy = llvm::FunctionType::get(
            self->llvmType(*sig.ret), trampTys, false);
        std::vector<llvm::Value *> trampArgs;
        trampArgs.push_back(envPtr);
        for (auto *a : argVals)
          trampArgs.push_back(a);
        bool retVoid = sig.ret->kind == TypeKind::Void;
        out.value = self->builder_.CreateCall(trampTy, trampPtr, trampArgs,
                                              retVoid ? "" : "call");
        out.kind = self->kindOf(*sig.ret);
        if (out.kind == ValueKind::Func)
          out.funcType = *sig.ret;
        // Hasil struct: normalisasi ke pointer storage (cermin direct).
        if (out.kind == ValueKind::Struct) {
          llvm::StructType *st =
              llvm::dyn_cast<llvm::StructType>(self->llvmType(*sig.ret));
          const StructLayout *rlayout = nullptr;
          if (st && st->hasName())
            rlayout = self->findLayout(st->getName().str());
          if (!rlayout) {
            fail("internal error: unknown struct layout for call result");
            return;
          }
          llvm::AllocaInst *rslot = self->entryAlloca(rlayout->ty, "ret");
          self->builder_.CreateStore(out.value, rslot);
          out.value = rslot;
          out.structName = st->getName().str();
        }
        return;
      }
      llvm::Function *callee = fnIt->second;
      if (callee->arg_size() != n.args.size()) {
        // Seharusnya sudah ditolak typechecker; pertahanan anti-crash.
        fail("wrong number of arguments for '" + n.callee + "'");
        return;
      }
      std::vector<llvm::Value *> argVals;
      {
        size_t ai = 0;
        for (const auto &a : n.args) {
          GenValue g;
          if (!self->genExpr(*a, g, errOut)) {
            ok = false;
            return;
          }
          llvm::Value *v = g.value;
          // Argumen struct by-value (Phase 21d): load dari storage.
          if (ai < callee->arg_size()) {
            llvm::Type *want = callee->getArg(ai)->getType();
            if (want->isStructTy() && v->getType() != want) {
              if (!v->getType()->isPointerTy()) {
                fail("mismatched argument type for '" + n.callee + "'");
                return;
              }
              v = self->builder_.CreateLoad(want, v, "argv");
            }
            // FFI C (Phase 32): bool Nusa (i1) → C int (i32).
            if (want->isIntegerTy(32) &&
                v->getType()->isIntegerTy(1) &&
                self->externFns_.count(n.callee)) {
              v = self->builder_.CreateZExt(v, want, "argb");
            }
          }
          argVals.push_back(v);
          ai++;
        }
      }
      // Validasi ringan tipe argumen (typechecker sudah menjamin;
      // ini mencegah IR invalid bila generate dipanggil langsung).
      size_t i = 0;
      for (auto &param : callee->args()) {
        if (argVals[i]->getType() != param.getType()) {
          fail("mismatched argument type for '" + n.callee + "'");
          return;
        }
        i++;
      }
      bool isVoid = callee->getReturnType()->isVoidTy();
      out.value = self->builder_.CreateCall(callee, argVals,
                                            isVoid ? "" : "call");
      auto kIt = self->funcRetKind_.find(n.callee);
      out.kind =
          (kIt == self->funcRetKind_.end()) ? ValueKind::Int : kIt->second;
      // FFI C (Phase 32): return C int → bool Nusa (non-nol = true).
      if (!isVoid && out.kind == ValueKind::Bool &&
          self->externFns_.count(n.callee)) {
        llvm::Value *nonzero = self->builder_.CreateICmpNE(
            out.value,
            llvm::ConstantInt::get(out.value->getType(), 0, false), "retb");
        out.value = nonzero;
      }
      // Hasil struct (Phase 21d): call menghasilkan VALUE — normalisasi
      // ke pointer storage agar seragam dengan StructLit/VarRef.
      // Nama layout diambil dari tipe return LLVM (struct bernama).
      if (out.kind == ValueKind::Struct) {
        llvm::Type *retTy = callee->getReturnType();
        llvm::StructType *st = llvm::dyn_cast<llvm::StructType>(retTy);
        const StructLayout *rlayout = nullptr;
        if (st && st->hasName())
          rlayout = self->findLayout(st->getName().str());
        if (!rlayout) {
          fail("internal error: unknown struct layout for call result");
          return;
        }
        llvm::AllocaInst *rslot = self->entryAlloca(rlayout->ty, "ret");
        self->builder_.CreateStore(out.value, rslot);
        out.value = rslot;
        out.structName = st->getName().str();
      }
      // Hasil bertipe fungsi (fn mengembalikan fn): teruskan signature
      // agar bisa dipanggil tak langsung (Phase 21b).
      if (out.kind == ValueKind::Func) {
        auto tyIt = self->funcTypes_.find(n.callee);
        if (tyIt != self->funcTypes_.end() &&
            tyIt->second.kind == TypeKind::Func && tyIt->second.ret)
          out.funcType = *tyIt->second.ret;
      }
    }
    void operator()(const Expr::Unary &n) {
      GenValue in;
      if (!self->genExpr(*n.operand, in, errOut))
        ok = false;
      if (!ok)
        return;
      if (in.kind != ValueKind::Int)
        fail("unary '" + std::string(n.op == UnOp::Neg ? "-" : "+") +
             "' requires an integer operand");
      else if (n.op == UnOp::Neg)
        out = {self->builder_.CreateNeg(in.value, "neg"), ValueKind::Int};
      else
        out = in; // unary plus: identitas
    }
    void operator()(const Expr::Binary &n) {
      GenValue lhs, rhs;
      if (!self->genExpr(*n.lhs, lhs, errOut) ||
          !self->genExpr(*n.rhs, rhs, errOut)) {
        ok = false;
        return;
      }
      const std::string op = binOpName(n.op);
      auto mismatch = [&]() {
        return fail("mismatched operand types for '" + op + "'",
                    "type checker should have rejected this program");
      };
      switch (n.op) {
      case BinOp::Add:
      case BinOp::Sub:
      case BinOp::Mul:
      case BinOp::Div: {
        if (lhs.kind != ValueKind::Int || rhs.kind != ValueKind::Int) {
          mismatch();
          return;
        }
        llvm::Value *v = nullptr;
        switch (n.op) {
        case BinOp::Add: v = self->builder_.CreateAdd(lhs.value, rhs.value, "add"); break;
        case BinOp::Sub: v = self->builder_.CreateSub(lhs.value, rhs.value, "sub"); break;
        case BinOp::Mul: v = self->builder_.CreateMul(lhs.value, rhs.value, "mul"); break;
        default: {
          // v0.9.5 hardening: sdiv diam-diam garbage saat pembagi 0
          // (LLVM fold ke undef / SDIV AArch64 tak trap) → cek eksplisit.
          llvm::Function *curFn =
              self->builder_.GetInsertBlock()->getParent();
          llvm::BasicBlock *failBB = llvm::BasicBlock::Create(
              self->builder_.getContext(), "div.zero", curFn);
          llvm::BasicBlock *okBB = llvm::BasicBlock::Create(
              self->builder_.getContext(), "div.ok", curFn);
          llvm::Value *isZero = self->builder_.CreateICmpEQ(
              rhs.value,
              llvm::ConstantInt::get(self->builder_.getInt64Ty(), 0, false),
              "div.z");
          self->builder_.CreateCondBr(isZero, failBB, okBB);
          self->builder_.SetInsertPoint(failBB);
          self->builder_.CreateCall(self->module_->getFunction("puts"),
                                    {self->getStringPtr(
                                        "runtime error: division by zero")});
          self->builder_.CreateCall(self->module_->getFunction("exit"),
                                    {self->builder_.getInt32(1)});
          self->builder_.CreateUnreachable();
          self->builder_.SetInsertPoint(okBB);
          v = self->builder_.CreateSDiv(lhs.value, rhs.value, "div");
          break;
        }
        }
        out = {v, ValueKind::Int};
        return;
      }
      case BinOp::Lt:
      case BinOp::Gt:
      case BinOp::LtEq:
      case BinOp::GtEq: {
        if (lhs.kind != ValueKind::Int || rhs.kind != ValueKind::Int) {
          mismatch();
          return;
        }
        llvm::CmpInst::Predicate pred = llvm::CmpInst::ICMP_SLT;
        if (n.op == BinOp::Gt) pred = llvm::CmpInst::ICMP_SGT;
        else if (n.op == BinOp::LtEq) pred = llvm::CmpInst::ICMP_SLE;
        else if (n.op == BinOp::GtEq) pred = llvm::CmpInst::ICMP_SGE;
        out = {self->builder_.CreateICmp(pred, lhs.value, rhs.value, "cmp"),
               ValueKind::Bool};
        return;
      }
      case BinOp::Eq:
      case BinOp::NotEq: {
        if (lhs.kind != rhs.kind || lhs.kind == ValueKind::String) {
          if (lhs.kind == ValueKind::String || rhs.kind == ValueKind::String)
            fail("string comparison with '" + op + "' is not supported",
                 "string operations are coming with the standard library");
          else
            mismatch();
          return;
        }
        auto pred = (n.op == BinOp::Eq) ? llvm::CmpInst::ICMP_EQ
                                        : llvm::CmpInst::ICMP_NE;
        out = {self->builder_.CreateICmp(pred, lhs.value, rhs.value, "eq"),
               ValueKind::Bool};
        return;
      }
      }
    }
  };
  Visitor v{this, out, errOut, loc};
  std::visit(v, expr.node);
  --exprDepth_;
  return v.ok;
}

// Emisi builtin runtime v0.9.0: pemanggilan libc langsung.
// Prasyarat: typechecker sudah memvalidasi arity + tipe (guard di sini
// murni pertahanan anti-crash).
void Codegen::emitOomCheck(llvm::Value *p, const std::string &what) {
  llvm::Function *curFn = builder_.GetInsertBlock()->getParent();
  llvm::BasicBlock *failBB =
      llvm::BasicBlock::Create(*ctx_, "oom.fail", curFn);
  llvm::BasicBlock *okBB =
      llvm::BasicBlock::Create(*ctx_, "oom.ok", curFn);
  llvm::Value *isNull = builder_.CreateICmpEQ(
      p,
      llvm::ConstantPointerNull::get(llvm::PointerType::get(*ctx_, 0)),
      "oom.null");
  builder_.CreateCondBr(isNull, failBB, okBB);
  builder_.SetInsertPoint(failBB);
  builder_.CreateCall(
      module_->getFunction("puts"),
      {getStringPtr("runtime error: out of memory (" + what + ")")});
  builder_.CreateCall(module_->getFunction("exit"),
                      {builder_.getInt32(1)});
  builder_.CreateUnreachable();
  builder_.SetInsertPoint(okBB);
}

llvm::Value *Codegen::mallocFor(llvm::Type *ty, const std::string &name) {
  // sizeof via GEP-null (target-independent): `&((T*)0)[1]`.
  llvm::Type *i64 = builder_.getInt64Ty();
  llvm::FunctionCallee mallocFn = module_->getOrInsertFunction(
      "nusa_alloc", llvm::FunctionType::get(builder_.getPtrTy(), {i64}, false));
  // sizeof via GEP-null satu-indeks (target-independent):
  // `&((T*)0)[1]` — satu elemen penuh = ukuran T.
  llvm::Value *nullP = llvm::ConstantPointerNull::get(
      llvm::PointerType::get(*ctx_, 0));
  llvm::Value *one = llvm::ConstantInt::get(builder_.getInt32Ty(), 1, false);
  llvm::Value *end =
      builder_.CreateGEP(ty, nullP, {one}, "szp");
  llvm::Value *sz = builder_.CreatePtrToInt(end, i64, "sz");
  llvm::Value *mem = builder_.CreateCall(mallocFn, {sz}, name);
  // Heap program-lifetime (model memori §4): tak pernah dibebaskan,
  // OS merebut saat exit (cermin buffer string). Via `nusa_alloc`
  // (seam alokator weak, Phase 123) — BUKAN malloc langsung.
  emitOomCheck(mem, name);
  return mem;
}

llvm::Function *Codegen::getTrampoline(const std::string &target,
                                       const Type &sig,
                                       const std::string &envStruct,
                                       std::string &errOut) {
  auto it = tramps_.find(target);
  if (it != tramps_.end())
    return it->second;
  if (sig.kind != TypeKind::Func || !sig.ret) {
    errOut = "trampoline needs function type for '" + target + "'";
    return nullptr;
  }
  auto fnIt = functions_.find(target);
  if (fnIt == functions_.end()) {
    errOut = "unknown function '" + target + "' for trampoline";
    return nullptr;
  }
  std::vector<llvm::Type *> tparams;
  tparams.push_back(builder_.getPtrTy()); // env opak
  for (const auto &p : sig.params)
    tparams.push_back(llvmType(p));
  llvm::FunctionType *trampTy = llvm::FunctionType::get(
      llvmType(*sig.ret), tparams, false);
  llvm::Function *tramp = llvm::Function::Create(
      trampTy, llvm::Function::InternalLinkage, target + ".tramp",
      module_.get());
  llvm::BasicBlock *saved = builder_.GetInsertBlock();
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(*ctx_, "entry", tramp);
  builder_.SetInsertPoint(bb);
  auto argIt = tramp->arg_begin();
  llvm::Value *envArg = &*argIt++;
  std::vector<llvm::Value *> callArgs;
  llvm::Function *callee = fnIt->second;
  if (!envStruct.empty()) {
    // Lifted: cast env opak ke struct env + panggil by-value di depan.
    const StructLayout *layout = findLayout(envStruct);
    if (!layout) {
      if (saved)
        builder_.SetInsertPoint(saved);
      errOut = "unknown closure env '" + envStruct + "'";
      return nullptr;
    }
    llvm::Value *eptr = builder_.CreateBitCast(
        envArg, llvm::PointerType::get(*ctx_, 0), "eptr");
    llvm::Value *envVal =
        builder_.CreateLoad(layout->ty, eptr, "env");
    callArgs.push_back(envVal);
  }
  for (; argIt != tramp->arg_end(); ++argIt)
    callArgs.push_back(&*argIt);
  if (callee->arg_size() != callArgs.size()) {
    if (saved)
      builder_.SetInsertPoint(saved);
    errOut = "trampoline arity mismatch for '" + target + "'";
    return nullptr;
  }
  if (sig.ret->kind == TypeKind::Void) {
    builder_.CreateCall(callee->getFunctionType(), callee, callArgs);
    builder_.CreateRetVoid();
  } else {
    llvm::Value *r = builder_.CreateCall(callee->getFunctionType(), callee,
                                         callArgs, "tr");
    builder_.CreateRet(r);
  }
  if (saved)
    builder_.SetInsertPoint(saved);
  tramps_[target] = tramp;
  return tramp;
}

bool Codegen::genBuiltinCall(const Expr::Call &n, const Location &loc,
                             GenValue &out, std::string &errOut) {
  auto fail = [&](const std::string &msg) {
    errOut = at(loc) + ": internal error: " + msg;
    return false;
  };
  llvm::Type *i64 = builder_.getInt64Ty();
  llvm::Type *i32 = builder_.getInt32Ty();
  llvm::Type *i8 = builder_.getInt8Ty();
  llvm::Type *i1 = builder_.getInt1Ty();
  llvm::Type *ptr = builder_.getPtrTy();
  auto decl = [&](const std::string &name, llvm::FunctionType *ty) {
    return module_->getOrInsertFunction(name, ty);
  };
  // Evaluasi argumen (checker menjamin jumlah + tipe).
  std::vector<GenValue> args;
  for (const auto &a : n.args) {
    GenValue g;
    if (!genExpr(*a, g, errOut))
      return false;
    args.push_back(g);
  }
  const std::string &c = n.callee;

  // TCP listen/accept 2.0 — ditangani awal (sebelum rantai
  // dispatch panjang) bersama deklarasi langsung ke module.
  if (c == "tcp_listen" || c == "tcp_accept") {
    auto rt0 = [&](const std::string &name, llvm::FunctionType *ty) {
      return module_->getOrInsertFunction(name, ty);
    };
    if (args.size() != 1 || args[0].kind != ValueKind::Int)
      return fail("'" + c + "' needs an 'i64' argument");
    llvm::FunctionCallee f = rt0(
        std::string("nusa_") + c,
        llvm::FunctionType::get(i64, {i64}, false));
    out.value = builder_.CreateCall(f, {args[0].value}, "tl");
    out.kind = ValueKind::Int;
    return true;
  }
  if (c == "input") {
    // getline(&line, &n, stdin); strip '\n'; EOF → "".
    llvm::FunctionCallee getlineFn = decl(
        "getline",
        llvm::FunctionType::get(i64, {ptr, ptr, ptr}, false));
    llvm::GlobalVariable *stdinG = module_->getOrInsertGlobal("stdin", ptr);
    llvm::AllocaInst *lineSlot = entryAlloca(ptr, "in.line");
    llvm::AllocaInst *nSlot = entryAlloca(i64, "in.n");
    llvm::AllocaInst *resSlot = entryAlloca(ptr, "in.res");
    builder_.CreateStore(llvm::ConstantPointerNull::get(
                             llvm::PointerType::get(*ctx_, 0)),
                         lineSlot);
    builder_.CreateStore(llvm::ConstantInt::get(i64, 0, false), nSlot);
    llvm::Value *stdinVal =
        builder_.CreateLoad(ptr, stdinG, "stdin");
    llvm::Value *r = builder_.CreateCall(
        getlineFn, {lineSlot, nSlot, stdinVal}, "in.r");
    llvm::BasicBlock *emptyBB =
        llvm::BasicBlock::Create(*ctx_, "in.empty", curFn_);
    llvm::BasicBlock *stripBB =
        llvm::BasicBlock::Create(*ctx_, "in.strip", curFn_);
    llvm::BasicBlock *contBB =
        llvm::BasicBlock::Create(*ctx_, "in.cont", curFn_);
    llvm::Value *eof = builder_.CreateICmpSLT(
        r, llvm::ConstantInt::get(i64, 0, true), "in.eof");
    builder_.CreateCondBr(eof, emptyBB, stripBB);
    builder_.SetInsertPoint(emptyBB);
    builder_.CreateStore(getStringPtr(""), resSlot);
    builder_.CreateBr(contBB);
    builder_.SetInsertPoint(stripBB);
    llvm::Value *line =
        builder_.CreateLoad(ptr, lineSlot, "in.buf");
    // idx = (r == 0) ? 0 : r - 1 — baca selalu dalam batas.
    llvm::Value *isZero = builder_.CreateICmpEQ(
        r, llvm::ConstantInt::get(i64, 0, false), "in.z");
    llvm::Value *rMinus1 = builder_.CreateSub(
        r, llvm::ConstantInt::get(i64, 1, false), "in.m1");
    llvm::Value *lastIdx = builder_.CreateSelect(
        isZero, llvm::ConstantInt::get(i64, 0, false), rMinus1, "in.li");
    llvm::Value *lastPtr =
        builder_.CreateInBoundsGEP(i8, line, lastIdx, "in.lp");
    llvm::Value *lastCh =
        builder_.CreateLoad(i8, lastPtr, "in.lc");
    llvm::Value *isNL = builder_.CreateICmpEQ(
        lastCh, llvm::ConstantInt::get(i8, 10, false), "in.nl");
    llvm::Value *stripped = builder_.CreateSelect(
        isNL, llvm::ConstantInt::get(i8, 0, false), lastCh, "in.sc");
    builder_.CreateStore(stripped, lastPtr);
    builder_.CreateStore(line, resSlot);
    builder_.CreateBr(contBB);
    builder_.SetInsertPoint(contBB);
    out.value = builder_.CreateLoad(ptr, resSlot, "in");
    out.kind = ValueKind::String;
    return true;
  }

  if (c == "assert") {
    // if (!cond) { puts("assertion failed at F:L"); exit(1); }
    if (args.size() != 1 || args[0].kind != ValueKind::Bool)
      return fail("assert() needs a bool argument");
    llvm::BasicBlock *okBB =
        llvm::BasicBlock::Create(*ctx_, "assert.ok", curFn_);
    llvm::BasicBlock *failBB =
        llvm::BasicBlock::Create(*ctx_, "assert.fail", curFn_);
    builder_.CreateCondBr(args[0].value, okBB, failBB);
    builder_.SetInsertPoint(failBB);
    builder_.CreateCall(module_->getFunction("puts"),
                        {getStringPtr("assertion failed at " + at(loc))});
    builder_.CreateCall(module_->getFunction("exit"),
                        {llvm::ConstantInt::get(i32, 1, false)});
    builder_.CreateUnreachable();
    builder_.SetInsertPoint(okBB);
    out.value = nullptr;
    out.kind = ValueKind::Void;
    return true;
  }

  if (c == "str_len") {
    if (args.size() != 1 || args[0].kind != ValueKind::String)
      return fail("str_len() needs a string argument");
    llvm::FunctionCallee strlenFn =
        decl("strlen", llvm::FunctionType::get(i64, {ptr}, false));
    out.value =
        builder_.CreateCall(strlenFn, {args[0].value}, "slen");
    out.kind = ValueKind::Int;
    return true;
  }

  if (c == "str_eq") {
    // strcmp(a, b) == 0 (v1.6.0: kesetaraan string untuk match).
    if (args.size() != 2 || args[0].kind != ValueKind::String ||
        args[1].kind != ValueKind::String)
      return fail("str_eq() needs two string arguments");
    llvm::Type *i32 = builder_.getInt32Ty();
    llvm::FunctionCallee strcmpFn = decl(
        "strcmp", llvm::FunctionType::get(i32, {ptr, ptr}, false));
    llvm::Value *r = builder_.CreateCall(
        strcmpFn, {args[0].value, args[1].value}, "seq");
    out.value = builder_.CreateICmpEQ(
        r, llvm::ConstantInt::get(i32, 0, false), "seqb");
    out.kind = ValueKind::Bool;
    return true;
  }

  if (c == "str_concat") {
    // malloc(la+lb+1); memcpy; NUL. Heap program-lifetime sesuai
    // model memori (docs/memory-model.md §2-§4): buffer string runtime
    // hidup selama program; OS merebut kembali saat exit.
    if (args.size() != 2 || args[0].kind != ValueKind::String ||
        args[1].kind != ValueKind::String)
      return fail("str_concat() needs two string arguments");
    llvm::FunctionCallee strlenFn =
        decl("strlen", llvm::FunctionType::get(i64, {ptr}, false));
    llvm::FunctionCallee mallocFn =
        decl("nusa_alloc", llvm::FunctionType::get(ptr, {i64}, false));
    llvm::FunctionCallee memcpyFn = decl(
        "memcpy", llvm::FunctionType::get(ptr, {ptr, ptr, i64}, false));
    llvm::Value *la = builder_.CreateCall(strlenFn, {args[0].value}, "cla");
    llvm::Value *lb = builder_.CreateCall(strlenFn, {args[1].value}, "clb");
    llvm::Value *total = builder_.CreateAdd(
        builder_.CreateAdd(la, lb, "ct"), llvm::ConstantInt::get(i64, 1, false),
        "ct1");
    llvm::Value *buf =
        builder_.CreateCall(mallocFn, {total}, "cbuf");
    emitOomCheck(buf, "str_concat");
    builder_.CreateCall(memcpyFn, {buf, args[0].value, la});
    llvm::Value *dst2 =
        builder_.CreateInBoundsGEP(i8, buf, la, "cdst");
    builder_.CreateCall(memcpyFn, {dst2, args[1].value, lb});
    llvm::Value *endp = builder_.CreateInBoundsGEP(
        i8, buf, builder_.CreateAdd(la, lb, "cend"), "cendp");
    builder_.CreateStore(llvm::ConstantInt::get(i8, 0, false), endp);
    out.value = buf;
    out.kind = ValueKind::String;
    return true;
  }

  if (c == "int_to_str") {
    // snprintf(NULL, 0, "%lld", v) → malloc → snprintf (v1.19.0).
    if (args.size() != 1 || args[0].kind != ValueKind::Int)
      return fail("int_to_str() needs an 'i64' argument");
    llvm::FunctionCallee snprintfFn = decl(
        "snprintf",
        llvm::FunctionType::get(builder_.getInt32Ty(), {ptr, i64}, true));
    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(
        llvm::PointerType::get(*ctx_, 0));
    llvm::Value *need = builder_.CreateCall(
        snprintfFn, {nullPtr, llvm::ConstantInt::get(i64, 0, false),
                     getStringPtr("%lld"), args[0].value},
        "its.need");
    llvm::Value *need64 = builder_.CreateSExt(need, i64, "its.need64");
    llvm::Value *total = builder_.CreateAdd(
        need64, llvm::ConstantInt::get(i64, 1, false), "its.total");
    llvm::FunctionCallee mallocFn =
        decl("nusa_alloc", llvm::FunctionType::get(ptr, {i64}, false));
    llvm::Value *buf =
        builder_.CreateCall(mallocFn, {total}, "its.buf");
    emitOomCheck(buf, "int_to_str");
    builder_.CreateCall(snprintfFn,
                        {buf, total, getStringPtr("%lld"), args[0].value});
    out.value = buf;
    out.kind = ValueKind::String;
    return true;
  }

  if (c == "sha256_hex" || c == "crypto_rand_hex") {
    // Runtime malloc; NULL → "" (gagal eksplisit; panjang dicek di
    // level Nusa). Cermin pola null→"" file_read (branch+slot).
    bool wantStr = (c == "sha256_hex");
    if (args.size() != 1)
      return fail("wrong number of arguments for '" + c +
                  "': expected 1");
    if ((wantStr && args[0].kind != ValueKind::String) ||
        (!wantStr && args[0].kind != ValueKind::Int))
      return fail("'" + c + "' needs " +
                  std::string(wantStr ? "a 'string'" : "an 'i64'") +
                  " argument");
    llvm::FunctionCallee cryFn = decl(
        wantStr ? "nusa_sha256_hex" : "nusa_crypto_rand_hex",
        llvm::FunctionType::get(
            ptr, {wantStr ? ptr : i64}, false));
    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(
        llvm::PointerType::get(*ctx_, 0));
    llvm::Value *r = builder_.CreateCall(cryFn, {args[0].value}, "cry.r");
    llvm::Value *isNull = builder_.CreateICmpEQ(r, nullPtr, "cry.null");
    llvm::BasicBlock *okBB =
        llvm::BasicBlock::Create(*ctx_, "cry.ok", curFn_);
    llvm::BasicBlock *contBB =
        llvm::BasicBlock::Create(*ctx_, "cry.cont", curFn_);
    llvm::AllocaInst *resSlot = entryAlloca(ptr, "cry.res");
    builder_.CreateStore(getStringPtr(""), resSlot);
    builder_.CreateCondBr(isNull, contBB, okBB);
    builder_.SetInsertPoint(okBB);
    builder_.CreateStore(r, resSlot);
    builder_.CreateBr(contBB);
    builder_.SetInsertPoint(contBB);
    out.value = builder_.CreateLoad(ptr, resSlot, "cry");
    out.kind = ValueKind::String;
    return true;
  }

  if (c == "parse_int") {    if (args.size() != 1 || args[0].kind != ValueKind::String)
      return fail("parse_int() needs a string argument");
    llvm::FunctionCallee strtollFn = decl(
        "strtoll",
        llvm::FunctionType::get(i64, {ptr, ptr, i32}, false));
    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(
        llvm::PointerType::get(*ctx_, 0));
    out.value = builder_.CreateCall(
        strtollFn, {args[0].value, nullPtr, llvm::ConstantInt::get(i32, 10, false)},
        "pint");
    out.kind = ValueKind::Int;
    return true;
  }

  if (c == "parse_int_valid") {
    // sscanf(s, " %lld %c", &v, &trailer) == 1 → numerik penuh.
    // Koheren dengan parse_int (strtoll): spasi/sign OK, digit ≥1,
    // sisa non-spasi ditolak; overflow ikut valid (nilai terjepit
    // sama seperti parse_int).
    if (args.size() != 1 || args[0].kind != ValueKind::String)
      return fail("parse_int_valid() needs a string argument");
    llvm::FunctionCallee sscanfFn = decl(
        "sscanf", llvm::FunctionType::get(
                      builder_.getInt32Ty(), {ptr, ptr}, true));
    llvm::AllocaInst *vSlot = entryAlloca(
        builder_.getInt64Ty(), "piv.v");
    llvm::AllocaInst *tSlot = entryAlloca(
        builder_.getInt8Ty(), "piv.t");
    llvm::Value *r = builder_.CreateCall(
        sscanfFn,
        {args[0].value, getStringPtr(" %lld %c"), vSlot, tSlot}, "piv.r");
    out.value = builder_.CreateICmpEQ(
        r, llvm::ConstantInt::get(builder_.getInt32Ty(), 1, false), "piv");
    out.kind = ValueKind::Bool;
    return true;
  }

  if (c == "file_read" || c == "file_write" || c == "file_exists") {
    llvm::FunctionCallee fopenFn =
        decl("fopen", llvm::FunctionType::get(ptr, {ptr, ptr}, false));
    llvm::FunctionCallee fcloseFn =
        decl("fclose", llvm::FunctionType::get(i32, {ptr}, false));
    if (c == "file_exists") {
      // f = fopen(p, "r"); ada = (f != null); bila ada fclose.
      if (args.size() != 1 || args[0].kind != ValueKind::String)
        return fail("file_exists() needs a string argument");
      llvm::Value *f = builder_.CreateCall(
          fopenFn, {args[0].value, getStringPtr("r")}, "fe.f");
      llvm::Value *isNull = builder_.CreateICmpEQ(
          f,
          llvm::ConstantPointerNull::get(llvm::PointerType::get(*ctx_, 0)),
          "fe.null");
      llvm::BasicBlock *shutBB =
          llvm::BasicBlock::Create(*ctx_, "fe.shut", curFn_);
      llvm::BasicBlock *contBB =
          llvm::BasicBlock::Create(*ctx_, "fe.cont", curFn_);
      llvm::AllocaInst *resSlot = entryAlloca(i1, "fe.res");
      builder_.CreateStore(llvm::ConstantInt::get(i1, 0, false), resSlot);
      builder_.CreateCondBr(isNull, contBB, shutBB);
      builder_.SetInsertPoint(shutBB);
      builder_.CreateCall(fcloseFn, {f});
      builder_.CreateStore(llvm::ConstantInt::get(i1, 1, false), resSlot);
      builder_.CreateBr(contBB);
      builder_.SetInsertPoint(contBB);
      out.value = builder_.CreateLoad(i1, resSlot, "fe");
      out.kind = ValueKind::Bool;
      return true;
    }
    if (c == "file_write") {
      // f = fopen(p, "w"); null → false; fputs(c, f); fclose → (r >= 0).
      if (args.size() != 2 || args[0].kind != ValueKind::String ||
          args[1].kind != ValueKind::String)
        return fail("file_write() needs two string arguments");
      llvm::FunctionCallee fputsFn =
          decl("fputs", llvm::FunctionType::get(i32, {ptr, ptr}, false));
      llvm::Value *f = builder_.CreateCall(
          fopenFn, {args[0].value, getStringPtr("w")}, "fw.f");
      llvm::Value *isNull = builder_.CreateICmpEQ(
          f,
          llvm::ConstantPointerNull::get(llvm::PointerType::get(*ctx_, 0)),
          "fw.null");
      llvm::BasicBlock *writeBB =
          llvm::BasicBlock::Create(*ctx_, "fw.write", curFn_);
      llvm::BasicBlock *contBB =
          llvm::BasicBlock::Create(*ctx_, "fw.cont", curFn_);
      llvm::AllocaInst *resSlot = entryAlloca(i1, "fw.res");
      builder_.CreateStore(llvm::ConstantInt::get(i1, 0, false), resSlot);
      builder_.CreateCondBr(isNull, contBB, writeBB);
      builder_.SetInsertPoint(writeBB);
      llvm::Value *r = builder_.CreateCall(
          fputsFn, {args[1].value, f}, "fw.r");
      builder_.CreateCall(fcloseFn, {f});
      llvm::Value *okw = builder_.CreateICmpSGE(
          r, llvm::ConstantInt::get(i32, 0, false), "fw.ok");
      builder_.CreateStore(okw, resSlot);
      builder_.CreateBr(contBB);
      builder_.SetInsertPoint(contBB);
      out.value = builder_.CreateLoad(i1, resSlot, "fw");
      out.kind = ValueKind::Bool;
      return true;
    }
    // file_read: fopen "r"; null → ""; fseek END; size = ftell;
    // rewind; buf = malloc(size+1); fread; NUL; fclose.
    if (args.size() != 1 || args[0].kind != ValueKind::String)
      return fail("file_read() needs a string argument");
    llvm::FunctionCallee fseekFn = decl(
        "fseek", llvm::FunctionType::get(i32, {ptr, i64, i32}, false));
    llvm::FunctionCallee ftellFn =
        decl("ftell", llvm::FunctionType::get(i64, {ptr}, false));
    llvm::FunctionCallee rewindFn =
        decl("rewind", llvm::FunctionType::get(
                           llvm::Type::getVoidTy(*ctx_), {ptr}, false));
    llvm::FunctionCallee mallocFn =
        decl("nusa_alloc", llvm::FunctionType::get(ptr, {i64}, false));
    llvm::FunctionCallee freadFn = decl(
        "fread",
        llvm::FunctionType::get(i64, {ptr, i64, i64, ptr}, false));
    llvm::Value *f = builder_.CreateCall(
        fopenFn, {args[0].value, getStringPtr("r")}, "fr.f");
    llvm::Value *isNull = builder_.CreateICmpEQ(
        f,
        llvm::ConstantPointerNull::get(llvm::PointerType::get(*ctx_, 0)),
        "fr.null");
    llvm::BasicBlock *readBB =
        llvm::BasicBlock::Create(*ctx_, "fr.read", curFn_);
    llvm::BasicBlock *contBB =
        llvm::BasicBlock::Create(*ctx_, "fr.cont", curFn_);
    llvm::AllocaInst *resSlot = entryAlloca(ptr, "fr.res");
    builder_.CreateStore(getStringPtr(""), resSlot);
    builder_.CreateCondBr(isNull, contBB, readBB);
    builder_.SetInsertPoint(readBB);
    builder_.CreateCall(fseekFn, {f, llvm::ConstantInt::get(i64, 0, false),
                                  llvm::ConstantInt::get(i32, 2, false)});
    llvm::Value *size = builder_.CreateCall(ftellFn, {f}, "fr.size");
    // ftell gagal (-1, mis. stream tak seekable) → anggap kosong;
    // tutup file dulu agar tak bocor fd.
    llvm::BasicBlock *sizeOkBB =
        llvm::BasicBlock::Create(*ctx_, "fr.sizeok", curFn_);
    llvm::BasicBlock *sizeBadBB =
        llvm::BasicBlock::Create(*ctx_, "fr.sizebad", curFn_);
    llvm::Value *badSize = builder_.CreateICmpSLT(
        size, llvm::ConstantInt::get(i64, 0, true), "fr.badsize");
    builder_.CreateCondBr(badSize, sizeBadBB, sizeOkBB);
    builder_.SetInsertPoint(sizeBadBB);
    builder_.CreateCall(fcloseFn, {f});
    builder_.CreateBr(contBB);
    builder_.SetInsertPoint(sizeOkBB);
    builder_.CreateCall(rewindFn, {f});
    llvm::Value *cap = builder_.CreateAdd(
        size, llvm::ConstantInt::get(i64, 1, false), "fr.cap");
    llvm::Value *buf = builder_.CreateCall(mallocFn, {cap}, "fr.buf");
    emitOomCheck(buf, "file_read");
    llvm::Value *got = builder_.CreateCall(
        freadFn,
        {buf, llvm::ConstantInt::get(i64, 1, false), size, f}, "fr.got");
    llvm::Value *endp =
        builder_.CreateInBoundsGEP(i8, buf, got, "fr.end");
    builder_.CreateStore(llvm::ConstantInt::get(i8, 0, false), endp);
    builder_.CreateCall(fcloseFn, {f});
    builder_.CreateStore(buf, resSlot);
    builder_.CreateBr(contBB);
    builder_.SetInsertPoint(contBB);
    out.value = builder_.CreateLoad(ptr, resSlot, "fr");
    out.kind = ValueKind::String;
    return true;
  }

  if (c == "array_sum" || c == "array_min" || c == "array_max") {
    // Loop inline di atas array statis yang dikenal (ptr + len).
    if (args.size() != 1 || args[0].kind != ValueKind::Array ||
        args[0].arrayElem != ValueKind::Int)
      return fail("'" + c + "' needs an '[i64]' argument");
    const GenValue &arr = args[0];
    llvm::ArrayType *arrTy = arrayTypeOf(llvmScalar(ValueKind::Int),
                                         arr.arrayLen);
    llvm::AllocaInst *accSlot = entryAlloca(i64, "as.acc");
    llvm::AllocaInst *idxSlot = entryAlloca(i64, "as.i");
    llvm::Constant *zero = llvm::ConstantInt::get(i64, 0, false);
    llvm::Value *init = zero;
    if (c == "array_min")
      init = llvm::ConstantInt::get(i64,
                                    static_cast<uint64_t>(INT64_MAX), true);
    else if (c == "array_max")
      init = llvm::ConstantInt::get(i64,
                                    static_cast<uint64_t>(INT64_MIN), true);
    builder_.CreateStore(init, accSlot);
    builder_.CreateStore(zero, idxSlot);
    llvm::Value *lenVal =
        llvm::ConstantInt::get(i64, arr.arrayLen, false);
    llvm::BasicBlock *condBB =
        llvm::BasicBlock::Create(*ctx_, "as.cond", curFn_);
    llvm::BasicBlock *bodyBB =
        llvm::BasicBlock::Create(*ctx_, "as.body", curFn_);
    llvm::BasicBlock *afterBB =
        llvm::BasicBlock::Create(*ctx_, "as.after", curFn_);
    builder_.CreateBr(condBB);
    builder_.SetInsertPoint(condBB);
    llvm::Value *cur =
        builder_.CreateLoad(i64, idxSlot, "as.cur");
    builder_.CreateCondBr(builder_.CreateICmpSLT(cur, lenVal, "as.cmp"),
                          bodyBB, afterBB);
    builder_.SetInsertPoint(bodyBB);
    llvm::Value *elemPtr = builder_.CreateInBoundsGEP(arrTy, arr.value,
                                                      {zero, cur}, "as.ep");
    llvm::Value *elem = builder_.CreateLoad(i64, elemPtr, "as.e");
    llvm::Value *acc = builder_.CreateLoad(i64, accSlot, "as.a");
    llvm::Value *next = nullptr;
    if (c == "array_sum") {
      next = builder_.CreateAdd(acc, elem, "as.add");
    } else {
      llvm::CmpInst::Predicate pred = (c == "array_min")
                                          ? llvm::CmpInst::ICMP_SLT
                                          : llvm::CmpInst::ICMP_SGT;
      llvm::Value *take =
          builder_.CreateICmp(pred, elem, acc, "as.take");
      next = builder_.CreateSelect(take, elem, acc, "as.next");
    }
    builder_.CreateStore(next, accSlot);
    builder_.CreateStore(
        builder_.CreateAdd(cur, llvm::ConstantInt::get(i64, 1, false),
                           "as.inc"),
        idxSlot);
    builder_.CreateBr(condBB);
    builder_.SetInsertPoint(afterBB);
    out.value = builder_.CreateLoad(i64, accSlot, "as");
    out.kind = ValueKind::Int;
    return true;
  }

  // Concurrency (Phase 28, v1.9.0): panggilan ke runtime C
  // (`runtime/nusa_rt.c`, di-link driver dengan -pthread). Handle =
  // i64 opak (pointer heap runtime).
  if (c == "thread_spawn" || c == "thread_join" || c == "mutex_new" ||
      c == "mutex_lock" || c == "mutex_unlock" || c == "mutex_free" ||
      c == "mutex_try_lock" || c == "chan_new" || c == "chan_send" ||
      c == "chan_recv" || c == "chan_free" || c == "chan_len" ||
      c == "chan_new_bounded" || c == "chan_try_send" ||
      c == "atomic_new" ||
      c == "atomic_get" || c == "atomic_set" || c == "atomic_add" ||
      c == "atomic_free" || c == "sleep_ms" || c == "chan_new_bool" ||
      c == "chan_send_bool" || c == "chan_recv_bool" ||
      c == "chan_new_str" || c == "chan_send_str" || c == "chan_recv_str") {
    auto rt = [&](const std::string &name, llvm::FunctionType *ty) {
      return decl("nusa_" + name, ty);
    };
    // Channel bertag 2.0 (bool/string): tag diverifikasi runtime
    // (mismatch = pesan bersih + exit 1, bukan garbage).
    if (c == "chan_new_bool" || c == "chan_new_str") {
      if (!args.empty())
        return fail("'" + c + "' takes no arguments");
      int64_t tag = (c == "chan_new_bool") ? 1 : 2;
      llvm::FunctionCallee f = rt("chan_new_tagged",
                                  llvm::FunctionType::get(i64, {i64}, false));
      out.value = builder_.CreateCall(
          f, {llvm::ConstantInt::get(i64, (uint64_t)tag, false)}, "chnt");
      out.kind = ValueKind::Int;
      return true;
    }
    if (c == "chan_send_bool" || c == "chan_send_str") {
      bool wantStr = (c == "chan_send_str");
      if (args.size() != 2 || args[0].kind != ValueKind::Int ||
          (wantStr ? args[1].kind != ValueKind::String
                   : args[1].kind != ValueKind::Bool))
        return fail("'" + c + "' needs handle/value arguments");
      int64_t tag = wantStr ? 2 : 1;
      llvm::Value *word;
      if (wantStr) {
        word = builder_.CreatePtrToInt(args[1].value, i64, "chs.w");
      } else {
        word = builder_.CreateZExt(args[1].value, i64, "chb.w");
      }
      llvm::FunctionCallee f = rt("chan_send_tagged",
                                  llvm::FunctionType::get(
                                      llvm::Type::getVoidTy(*ctx_),
                                      {i64, i64, i64}, false));
      builder_.CreateCall(
          f,
          {args[0].value,
           llvm::ConstantInt::get(i64, (uint64_t)tag, false), word});
      out.value = nullptr;
      out.kind = ValueKind::Void;
      return true;
    }
    if (c == "chan_recv_bool" || c == "chan_recv_str") {
      bool wantStr = (c == "chan_recv_str");
      if (args.size() != 1 || args[0].kind != ValueKind::Int)
        return fail("'" + c + "' needs an 'i64' handle argument");
      int64_t tag = wantStr ? 2 : 1;
      llvm::FunctionCallee f = rt("chan_recv_tagged",
                                  llvm::FunctionType::get(i64, {i64, i64},
                                                          false));
      llvm::Value *word = builder_.CreateCall(
          f,
          {args[0].value,
           llvm::ConstantInt::get(i64, (uint64_t)tag, false)},
          "chr.w");
      if (wantStr) {
        out.value = builder_.CreateIntToPtr(word, ptr, "chr.s");
        out.kind = ValueKind::String;
      } else {
        out.value =
            builder_.CreateTrunc(word, builder_.getInt1Ty(), "chr.b");
        out.kind = ValueKind::Bool;
      }
      return true;
    }
    if (c == "thread_spawn") {
      // Destruktur pair closure persis panggilan tak langsung.
      if (args.size() != 1 || args[0].kind != ValueKind::Func)
        return fail("'thread_spawn' needs a function argument");
      llvm::FunctionCallee spawnFn =
          rt("thread_spawn", llvm::FunctionType::get(
                                 i64, {ptr, ptr}, false));
      llvm::Value *pairPtr = args[0].value;
      llvm::Value *tptr = builder_.CreateStructGEP(closurePairTy_, pairPtr,
                                                   0, "ts.tptr");
      llvm::Value *tramp =
          builder_.CreateLoad(ptr, tptr, "ts.tramp");
      llvm::Value *eptr = builder_.CreateStructGEP(closurePairTy_, pairPtr,
                                                   1, "ts.eptr");
      llvm::Value *env =
          builder_.CreateLoad(ptr, eptr, "ts.env");
      out.value =
          builder_.CreateCall(spawnFn, {tramp, env}, "ts.h");
      out.kind = ValueKind::Int;
      return true;
    }
    // Sisanya: argumen i64 polos (sudah divalidasi checker).
    auto need = [&](size_t k, const char *what) -> bool {
      if (args.size() <= k || args[k].kind != ValueKind::Int ||
          !args[k].value) {
        fail(std::string("'") + c + "' needs " + what);
        return false;
      }
      return true;
    };
    if (c == "thread_join" || c == "chan_recv" || c == "atomic_get" ||
        c == "mutex_try_lock" || c == "chan_len" || c == "mutex_lock" || c == "mutex_unlock" ||
        c == "mutex_free" || c == "chan_free" || c == "atomic_free" ||
        c == "sleep_ms" || c == "mutex_new" || c == "chan_new" ||
        c == "atomic_new" || c == "chan_new_bounded" ||
        c == "chan_try_send") {
      bool takesArg = (c == "thread_join" || c == "chan_recv" ||
                       c == "atomic_get" || c == "mutex_try_lock" ||
                       c == "chan_len" || c == "mutex_lock" ||
                       c == "atomic_get" || c == "mutex_lock" ||
                       c == "mutex_unlock" || c == "mutex_free" ||
                       c == "chan_free" || c == "atomic_free" ||
                       c == "sleep_ms" || c == "atomic_new" ||
                       c == "chan_new_bounded" || c == "chan_try_send");
      if (takesArg && !need(0, "an 'i64' handle argument"))
        return false;
      if (!takesArg && !args.empty())
        return fail("'" + c + "' takes no arguments");
      llvm::Value *h = takesArg ? args[0].value
                                : llvm::ConstantInt::get(i64, 0, false);
      if (c == "thread_join") {
        llvm::FunctionCallee f = rt(
            "thread_join", llvm::FunctionType::get(i64, {i64}, false));
        out.value = builder_.CreateCall(f, {h}, "tj");
        out.kind = ValueKind::Int;
        return true;
      }
      if (c == "chan_recv" || c == "atomic_get" || c == "mutex_try_lock" ||
          c == "chan_len") {
        const char *rname = (c == "chan_recv")    ? "chan_recv"
                            : (c == "atomic_get") ? "atomic_get"
                            : (c == "chan_len")   ? "chan_len"
                                                  : "mutex_try_lock";
        llvm::FunctionCallee f =
            rt(rname, llvm::FunctionType::get(i64, {i64}, false));
        out.value = builder_.CreateCall(f, {h}, "cg");
        out.kind = ValueKind::Int;
        return true;
      }
      if (c == "mutex_new" || c == "chan_new" || c == "atomic_new" ||
          c == "chan_new_bounded") {
        bool takesCap =
            (c == "atomic_new" || c == "chan_new_bounded");
        llvm::FunctionType *fty =
            takesCap ? llvm::FunctionType::get(i64, {i64}, false)
                     : llvm::FunctionType::get(i64, {}, false);
        const char *rname = (c == "mutex_new")        ? "mutex_new"
                            : (c == "chan_new")       ? "chan_new"
                            : (c == "chan_new_bounded") ? "chan_new_bounded"
                                                       : "atomic_new";
        llvm::FunctionCallee f = rt(rname, fty);
        if (takesCap) {
          if (!need(0, "an 'i64' argument"))
            return false;
          out.value = builder_.CreateCall(f, {args[0].value}, "mn");
        } else {
          if (!args.empty())
            return fail("'" + c + "' takes no arguments");
          out.value = builder_.CreateCall(f, {}, "mn");
        }
        out.kind = ValueKind::Int;
        return true;
      }
      // void: lock/unlock/free/sleep. Guard eksplisit (bukan
      // exhaustiveness): sejak Phase 118 ada builtin bernilai yang
      // lewat sini (try_send) — tanpa guard ia dieksekusi sebagai
      // sleep_ms + Void (bug: print macet, let segfault).
      if (c == "mutex_lock" || c == "mutex_unlock" || c == "mutex_free" ||
          c == "chan_free" || c == "atomic_free" || c == "sleep_ms") {
        const char *rname = (c == "mutex_lock")     ? "mutex_lock"
                            : (c == "mutex_unlock") ? "mutex_unlock"
                            : (c == "mutex_free")   ? "mutex_free"
                            : (c == "chan_free")    ? "chan_free"
                            : (c == "atomic_free")  ? "atomic_free"
                                                   : "sleep_ms";
        llvm::FunctionCallee f = rt(
            rname, llvm::FunctionType::get(
                       llvm::Type::getVoidTy(*ctx_), {i64}, false));
        builder_.CreateCall(f, {h});
        out.value = nullptr;
        out.kind = ValueKind::Void;
        return true;
      }
    }
    if (c == "chan_send" || c == "atomic_set" || c == "atomic_add") {      if (!need(0, "an 'i64' handle argument") ||
          !need(1, "an 'i64' value argument"))
        return false;
      const char *rname = (c == "chan_send")  ? "chan_send"
                          : (c == "atomic_set") ? "atomic_set"
                                               : "atomic_add";
      bool wantRet = (c == "atomic_add");
      llvm::FunctionType *fty =
          wantRet ? llvm::FunctionType::get(i64, {i64, i64}, false)
                  : llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx_),
                                            {i64, i64}, false);
      llvm::FunctionCallee f = rt(rname, fty);
      if (wantRet) {
        out.value = builder_.CreateCall(f, {args[0].value, args[1].value},
                                        "aa");
        out.kind = ValueKind::Int;
      } else {
        builder_.CreateCall(f, {args[0].value, args[1].value});
        out.value = nullptr;
        out.kind = ValueKind::Void;
      }
      return true;
    }
    // Backpressure (Phase 118, v3.19.0): try_send -> bool (i64 0/1
    // runtime dipetakan ke i1 cermin tcp_send).
    if (c == "chan_try_send") {
      if (!need(0, "an 'i64' handle argument") ||
          !need(1, "an 'i64' value argument"))
        return false;
      llvm::FunctionCallee f = rt(
          "chan_try_send", llvm::FunctionType::get(i64, {i64, i64}, false));
      llvm::Value *r = builder_.CreateCall(
          f, {args[0].value, args[1].value}, "cts");
      llvm::Value *okv = builder_.CreateICmpNE(
          r, llvm::ConstantInt::get(i64, 0, false), "cts.ok");
      out.value = builder_.CreateSelect(
          okv, llvm::ConstantInt::get(builder_.getInt1Ty(), 1, false),
          llvm::ConstantInt::get(builder_.getInt1Ty(), 0, false), "cts.b");
      out.kind = ValueKind::Bool;
      return true;
    }
    return fail("unknown builtin '" + c + "'");
  }

  // Stdlib 2.0 (Phase 29, v1.10.0): string/time/random/process/net.
  if (c == "str_char_at" || c == "str_slice" || c == "time_now_ms" ||
      c == "time_now_s" || c == "random_u64" || c == "process_exit" ||
      c == "process_env" || c == "process_cwd" || c == "process_argc" ||
      c == "process_argv" || c == "tcp_connect" || c == "tcp_send" ||
      c == "tcp_recv" || c == "tcp_close" || c == "tcp_listen" ||
      c == "tcp_accept") {
    auto rt2 = [&](const std::string &name, llvm::FunctionType *ty) {
      return decl("nusa_" + name, ty);
    };
    llvm::Value *emptyStr = nullptr; // dibuat lazily ("" global)
    auto getEmpty = [&]() -> llvm::Value * {
      if (!emptyStr)
        emptyStr = getStringPtr("");
      return emptyStr;
    };
    if (c == "str_char_at" || c == "str_slice") {
      // strlen + clamp + malloc + salin (cermin str_concat).
      if ((c == "str_char_at" &&
           (args.size() != 2 || args[0].kind != ValueKind::String ||
            args[1].kind != ValueKind::Int)) ||
          (c == "str_slice" &&
           (args.size() != 3 || args[0].kind != ValueKind::String ||
            args[1].kind != ValueKind::Int ||
            args[2].kind != ValueKind::Int)))
        return fail("'" + c + "' needs string/int arguments");
      llvm::FunctionCallee strlenFn =
          decl("strlen", llvm::FunctionType::get(i64, {ptr}, false));
      llvm::FunctionCallee mallocFn =
          decl("nusa_alloc", llvm::FunctionType::get(ptr, {i64}, false));
      llvm::FunctionCallee memcpyFn = decl(
          "memcpy", llvm::FunctionType::get(ptr, {ptr, ptr, i64}, false));
      llvm::Value *len =
          builder_.CreateCall(strlenFn, {args[0].value}, "sc.len");
      llvm::Value *zero = llvm::ConstantInt::get(i64, 0, false);
      llvm::Value *one = llvm::ConstantInt::get(i64, 1, false);
      llvm::Value *lo = args[1].value;
      llvm::Value *hi = (c == "str_char_at")
                            ? builder_.CreateAdd(lo, one, "sc.hi")
                            : args[2].value;
      // clamp lo ke [0, len], hi ke [lo, len].
      auto clampLo = [&](llvm::Value *v, llvm::Value *upper,
                         const char *nm) {
        llvm::Value *neg = builder_.CreateICmpSLT(v, zero, "sc.neg");
        llvm::Value *v0 =
            builder_.CreateSelect(neg, zero, v, "sc.v0");
        llvm::Value *over =
            builder_.CreateICmpSGT(v0, upper, "sc.over");
        return builder_.CreateSelect(over, upper, v0, nm);
      };
      lo = clampLo(lo, len, "sc.lo");
      hi = clampLo(hi, len, "sc.hi2");
      llvm::Value *n = builder_.CreateSub(hi, lo, "sc.n");
      llvm::Value *total =
          builder_.CreateAdd(n, one, "sc.total");
      llvm::Value *buf = builder_.CreateCall(mallocFn, {total}, "sc.buf");
      emitOomCheck(buf, c);
      llvm::Value *src =
          builder_.CreateInBoundsGEP(i8, args[0].value, lo, "sc.src");
      builder_.CreateCall(memcpyFn, {buf, src, n});
      llvm::Value *endp =
          builder_.CreateInBoundsGEP(i8, buf, n, "sc.end");
      builder_.CreateStore(llvm::ConstantInt::get(i8, 0, false), endp);
      out.value = buf;
      out.kind = ValueKind::String;
      return true;
    }
    if (c == "time_now_ms" || c == "time_now_s" || c == "random_u64") {
      if (!args.empty())
        return fail("'" + c + "' takes no arguments");
      llvm::FunctionCallee f = rt2(
          c == "time_now_ms" ? "time_now_ms"
                             : (c == "time_now_s" ? "time_now_s"
                                                  : "random_u64"),
          llvm::FunctionType::get(i64, {}, false));
      out.value = builder_.CreateCall(f, {}, "tm");
      out.kind = ValueKind::Int;
      return true;
    }
    if (c == "process_exit") {
      if (args.size() != 1 || args[0].kind != ValueKind::Int)
        return fail("'process_exit' needs an 'i64' argument");
      llvm::Value *code = builder_.CreateTrunc(args[0].value, i32, "px");
      builder_.CreateCall(module_->getFunction("exit"), {code});
      builder_.CreateUnreachable();
      out.value = nullptr;
      out.kind = ValueKind::Void;
      return true;
    }
    if (c == "process_env") {
      if (args.size() != 1 || args[0].kind != ValueKind::String)
        return fail("'process_env' needs a 'string' argument");
      llvm::FunctionCallee getenvFn =
          decl("getenv", llvm::FunctionType::get(ptr, {ptr}, false));
      llvm::Value *got =
          builder_.CreateCall(getenvFn, {args[0].value}, "pe");
      llvm::Value *isNull = builder_.CreateICmpEQ(
          got, llvm::ConstantPointerNull::get(
                   llvm::PointerType::get(*ctx_, 0)),
          "pe.null");
      out.value = builder_.CreateSelect(isNull, getEmpty(), got, "pe.s");
      out.kind = ValueKind::String;
      return true;
    }
    if (c == "process_cwd") {
      if (!args.empty())
        return fail("'process_cwd' takes no arguments");
      llvm::FunctionCallee getcwdFn =
          decl("getcwd", llvm::FunctionType::get(ptr, {ptr, i64}, false));
      llvm::Value *got = builder_.CreateCall(
          getcwdFn,
          {llvm::ConstantPointerNull::get(
               llvm::PointerType::get(*ctx_, 0)),
           llvm::ConstantInt::get(i64, 0, false)},
          "pc");
      llvm::Value *isNull = builder_.CreateICmpEQ(
          got, llvm::ConstantPointerNull::get(
                   llvm::PointerType::get(*ctx_, 0)),
          "pc.null");
      out.value = builder_.CreateSelect(isNull, getEmpty(), got, "pc.s");
      out.kind = ValueKind::String;
      return true;
    }
    if (c == "process_argc") {
      if (!args.empty())
        return fail("'process_argc' takes no arguments");
      llvm::GlobalVariable *g = module_->getNamedGlobal("nusa_argc");
      if (!g)
        return fail("missing argv globals");
      llvm::Value *v = builder_.CreateLoad(
          builder_.getInt32Ty(), g, "pa");
      out.value = builder_.CreateSExt(v, i64, "pa64");
      out.kind = ValueKind::Int;
      return true;
    }
    if (c == "process_argv") {
      if (args.size() != 1 || args[0].kind != ValueKind::Int)
        return fail("'process_argv' needs an 'i64' argument");
      llvm::GlobalVariable *gc = module_->getNamedGlobal("nusa_argc");
      llvm::GlobalVariable *gv = module_->getNamedGlobal("nusa_argv");
      if (!gc || !gv)
        return fail("missing argv globals");
      llvm::Function *curFn = builder_.GetInsertBlock()->getParent();
      llvm::BasicBlock *okBB =
          llvm::BasicBlock::Create(*ctx_, "pa.ok", curFn);
      llvm::BasicBlock *badBB =
          llvm::BasicBlock::Create(*ctx_, "pa.bad", curFn);
      llvm::BasicBlock *contBB =
          llvm::BasicBlock::Create(*ctx_, "pa.cont", curFn);
      llvm::Value *cnt = builder_.CreateLoad(builder_.getInt32Ty(), gc,
                                             "pa.cnt");
      llvm::Value *cnt64 = builder_.CreateSExt(cnt, i64, "pa.cnt64");
      llvm::Value *idx = args[0].value;
      llvm::Value *neg = builder_.CreateICmpSLT(
          idx, llvm::ConstantInt::get(i64, 0, false), "pa.neg");
      llvm::Value *over =
          builder_.CreateICmpSGE(idx, cnt64, "pa.over");
      llvm::Value *bad =
          builder_.CreateOr(neg, over, "pa.bad");
      builder_.CreateCondBr(bad, badBB, okBB);
      builder_.SetInsertPoint(badBB);
      builder_.CreateBr(contBB);
      builder_.SetInsertPoint(okBB);
      llvm::Value *argv = builder_.CreateLoad(ptr, gv, "pa.argv");
      llvm::Value *slot = builder_.CreateInBoundsGEP(ptr, argv, idx,
                                                     "pa.slot");
      llvm::Value *s = builder_.CreateLoad(ptr, slot, "pa.s");
      builder_.CreateBr(contBB);
      builder_.SetInsertPoint(contBB);
      llvm::PHINode *phi = builder_.CreatePHI(ptr, 2, "pa.phi");
      phi->addIncoming(getEmpty(), badBB);
      phi->addIncoming(s, okBB);
      out.value = phi;
      out.kind = ValueKind::String;
      return true;
    }
    if (c == "tcp_connect" || c == "tcp_send" || c == "tcp_recv" ||
        c == "tcp_close" || c == "tcp_listen" || c == "tcp_accept") {
      if (c == "tcp_connect") {
        if (args.size() != 2 || args[0].kind != ValueKind::String ||
            args[1].kind != ValueKind::Int)
          return fail("'tcp_connect' needs string/int arguments");
        llvm::FunctionCallee f = rt2(
            "tcp_connect", llvm::FunctionType::get(i64, {ptr, i64}, false));
        out.value = builder_.CreateCall(
            f, {args[0].value, args[1].value}, "tc");
        out.kind = ValueKind::Int;
        return true;
      }
      if (c == "tcp_send") {
        if (args.size() != 2 || args[0].kind != ValueKind::Int ||
            args[1].kind != ValueKind::String)
          return fail("'tcp_send' needs int/string arguments");
        llvm::FunctionCallee f = rt2(
            "tcp_send", llvm::FunctionType::get(i64, {i64, ptr}, false));
        llvm::Value *r = builder_.CreateCall(
            f, {args[0].value, args[1].value}, "ts");
        llvm::Value *okv = builder_.CreateICmpNE(
            r, llvm::ConstantInt::get(i64, 0, false), "ts.ok");
        out.value = builder_.CreateSelect(
            okv, llvm::ConstantInt::get(builder_.getInt1Ty(), 1, false),
            llvm::ConstantInt::get(builder_.getInt1Ty(), 0, false), "ts.b");
        out.kind = ValueKind::Bool;
        return true;
      }
      if (c == "tcp_recv") {
        if (args.size() != 2 || args[0].kind != ValueKind::Int ||
            args[1].kind != ValueKind::Int)
          return fail("'tcp_recv' needs int/int arguments");
        llvm::FunctionCallee f = rt2(
            "tcp_recv",
            llvm::FunctionType::get(ptr, {i64, i64}, false));
        out.value = builder_.CreateCall(
            f, {args[0].value, args[1].value}, "tr");
        out.kind = ValueKind::String;
        return true;
      }
      if (args.size() != 1 || args[0].kind != ValueKind::Int)
        return fail("'tcp_close' needs an 'i64' argument");
      llvm::FunctionCallee f = rt2(
          "tcp_close",
          llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx_), {i64},
                                  false));
      builder_.CreateCall(f, {args[0].value});
      out.value = nullptr;
      out.kind = ValueKind::Void;
      return true;
    }
    return fail("unknown builtin '" + c + "'");
  }

  return fail("unknown builtin '" + c + "'");
}

bool Codegen::emitPrint(const PrintStmt &stmt, std::string &errOut) {
  GenValue arg;
  if (!genExpr(*stmt.arg, arg, errOut))
    return false;
  llvm::FunctionCallee putsFn = module_->getFunction("puts");
  switch (arg.kind) {
  case ValueKind::String:
    builder_.CreateCall(putsFn, {arg.value});
    return true;
  case ValueKind::Int: {
    if (!intFmt_)
      intFmt_ = getStringPtr("%lld\n");
    llvm::FunctionCallee printfFn = module_->getFunction("printf");
    builder_.CreateCall(printfFn, {intFmt_, arg.value});
    return true;
  }
  case ValueKind::Bool: {
    llvm::Value *t = getStringPtr("true");
    llvm::Value *f = getStringPtr("false");
    llvm::Value *s = builder_.CreateSelect(arg.value, t, f, "boolstr");
    builder_.CreateCall(putsFn, {s});
    return true;
  }
  case ValueKind::Void:
    errOut = at(stmt.loc) + ": internal error: cannot print 'void' value";
    return false;
  case ValueKind::Struct:
    // Seharusnya ditolak typechecker; pertahanan anti-crash.
    errOut = at(stmt.loc) + ": internal error: cannot print struct value";
    return false;
  case ValueKind::Array:
    // Seharusnya ditolak typechecker; pertahanan anti-crash.
    errOut = at(stmt.loc) + ": internal error: cannot print array value";
    return false;
  case ValueKind::Func:
    // Seharusnya ditolak typechecker; pertahanan anti-crash.
    errOut = at(stmt.loc) + ": internal error: cannot print function value";
    return false;
  }
  return false; // tak terjangkau
}

bool Codegen::emitIRToFile(const std::string &path, std::string &errOut) {
  std::error_code ec;
  llvm::raw_fd_ostream out(path, ec);
  if (ec) {
    errOut = "error: cannot write IR file '" + path + "': " + ec.message();
    return false;
  }
  module_->print(out, nullptr);
  return true;
}

std::string Codegen::emitIRToString() {
  std::string s;
  llvm::raw_string_ostream os(s);
  module_->print(os, nullptr);
  return os.str();
}

} // namespace nusa
