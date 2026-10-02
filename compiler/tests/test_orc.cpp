// Kunci kapabilitas ORC LLJIT (Phase 105, milestone 1): LLVM 21.1.8 di
// Termux AArch64 mampu JIT in-process + resolusi simbol proses (libc).
// BUKAN subsystem JIT (tanpa API produk, tanpa pewarisan Nusa) — pagar
// toolchain agar desain JIT/REPL (Phase 105/106) berpijak pada bukti.
// AOT tetap satu-satunya jalur produk (lih. docs/execution-3.6.md).
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

namespace {
// Modul: fn forty_two() -> i64 { 40 + 2 }; fn main() -> i32
// { puts("jit-ok"); return forty_two() == 42 ? 0 : 1; }
bool buildSpike(std::unique_ptr<llvm::LLVMContext> &ctx,
                std::unique_ptr<llvm::Module> &mod) {
  ctx = std::make_unique<llvm::LLVMContext>();
  mod = std::make_unique<llvm::Module>("spike", *ctx);
  llvm::IRBuilder<> b(*ctx);
  llvm::Function *f42 = llvm::Function::Create(
      llvm::FunctionType::get(b.getInt64Ty(), false),
      llvm::Function::ExternalLinkage, "forty_two", *mod);
  llvm::BasicBlock *bb42 =
      llvm::BasicBlock::Create(*ctx, "entry", f42);
  b.SetInsertPoint(bb42);
  b.CreateRet(b.CreateAdd(b.getInt64(40), b.getInt64(2)));
  llvm::FunctionCallee puts = mod->getOrInsertFunction(
      "puts", llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()},
                                     false));
  llvm::Function *mainFn = llvm::Function::Create(
      llvm::FunctionType::get(b.getInt32Ty(), false),
      llvm::Function::ExternalLinkage, "main", *mod);
  llvm::BasicBlock *bb =
      llvm::BasicBlock::Create(*ctx, "entry", mainFn);
  b.SetInsertPoint(bb);
  b.CreateCall(puts, {b.CreateGlobalString("jit-ok")});
  llvm::Value *got = b.CreateCall(f42);
  llvm::Value *ok = b.CreateICmpEQ(got, b.getInt64(42));
  b.CreateRet(b.CreateSelect(ok, b.getInt32(0), b.getInt32(1)));
  return true;
}
} // namespace

static int testOrcJitRuns() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  std::unique_ptr<llvm::LLVMContext> ctx;
  std::unique_ptr<llvm::Module> mod;
  CHECK(buildSpike(ctx, mod));
  auto jit = llvm::cantFail(llvm::orc::LLJITBuilder().create());
  // Simbol proses (libc puts) — titik risiko Bionic, terbukti di sini.
  auto gen = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
      jit->getDataLayout().getGlobalPrefix());
  CHECK(static_cast<bool>(gen));
  jit->getMainJITDylib().addGenerator(std::move(*gen));
  llvm::cantFail(jit->addIRModule(
      llvm::orc::ThreadSafeModule(std::move(mod), std::move(ctx))));
  auto sym = llvm::cantFail(jit->lookup("main"));
  int (*mainJit)() = sym.toPtr<int (*)()>();
  CHECK(mainJit() == 0); // puts jalan + forty_two() == 42 di dalam JIT
  std::cout << "testOrcJitRuns OK\n";
  return 0;
}

int main() {
  if (testOrcJitRuns())
    return 1;
  std::cout << "ALL ORC TESTS PASSED\n";
  return 0;
}
