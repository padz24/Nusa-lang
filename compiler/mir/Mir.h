#pragma once
// Nusa MIR — fondasi IR internal (Phase 52, milestone 1).
//
// Aturan lapisan: header ini + Mir.cpp HANYA boleh include header
// standar C++ (tanpa compiler/* lain, tanpa LLVM). Namespace
// `nusa::mir`. Pipeline existing belum di-rewire (nol perilaku).
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace nusa {
namespace mir {

// ---- Typed values -------------------------------------------------

enum class TypeKind { I64, Bool, Str, Void, Ptr };

struct MirType {
  TypeKind kind = TypeKind::Void;
  static MirType i64() { return MirType{TypeKind::I64}; }
  static MirType boolean() { return MirType{TypeKind::Bool}; }
  static MirType str() { return MirType{TypeKind::Str}; }
  static MirType void_() { return MirType{TypeKind::Void}; }
  static MirType ptr() { return MirType{TypeKind::Ptr}; }
  bool operator==(const MirType &o) const { return kind == o.kind; }
  bool operator!=(const MirType &o) const { return kind != o.kind; }
  std::string name() const;
};

// Nilai SSA-seperti: `%id : type`. Parameter fungsi dihitung sebagai
// definisi awal (id 0..N-1 sesuai urutan parameter).
struct Value {
  uint32_t id = 0;
  MirType type;
};

// ---- Instructions -------------------------------------------------

enum class Op {
  ConstI64,
  ConstBool,
  ConstStr,
  Add,
  Sub,
  Mul,
  Div,
  CmpLt,
  CmpGt,
  CmpLtEq,
  CmpGtEq,
  CmpEq,
  CmpNotEq,
  Call,
  Alloc,
  Load,
  Store,
  Branch,
  CondBranch,
  Return,
  Unreachable,
  Print,
};

inline bool isTerminator(Op op) {
  return op == Op::Branch || op == Op::CondBranch || op == Op::Return ||
         op == Op::Unreachable;
}

struct Instr {
  bool hasResult = false;
  uint32_t result = 0;
  MirType resultType;
  Op op = Op::Unreachable;
  std::vector<uint32_t> operands;
  // Immediates / nama (bermakna tergantung op):
  int64_t intVal = 0;      // ConstI64
  bool boolVal = false;    // ConstBool
  std::string strVal;      // ConstStr
  std::string callee;      // Call
  std::string slot;        // Alloc / Load / Store
  std::string target;      // Branch
  std::string trueTarget;  // CondBranch
  std::string falseTarget; // CondBranch
};

struct BasicBlock {
  std::string label;
  std::vector<Instr> instrs;
};

struct MirFunction {
  std::string name;
  std::vector<std::pair<std::string, MirType>> params;
  MirType ret;
  std::string entry;
  // deque: referensi block stabil saat addBlock susulan (kontrak builder).
  std::deque<BasicBlock> blocks;
  // Counter id nilai berikutnya (internal builder; dirawat otomatis —
  // lazy-init dari isi bila fungsi dibangun manual).
  uint32_t nextValueId = 0;
};

struct MirModule {
  // deque: referensi fungsi stabil saat newFunction susulan.
  std::deque<MirFunction> functions;
};

// ---- Builder ------------------------------------------------------
// Id nilai dialokasikan per fungsi (parameter = 0..N-1). Referensi
// MirFunction/BasicBlock yang dikembalikan stabil (deque) — aman
// dipegang lintas addBlock/newFunction susulan.
class MirBuilder {
public:
  explicit MirBuilder(MirModule &mod) : mod_(mod) {}

  MirFunction &newFunction(const std::string &name,
                           const std::vector<std::pair<std::string, MirType>> &params,
                           MirType ret, const std::string &entry);
  BasicBlock &addBlock(MirFunction &fn, const std::string &label);

  Value constI64(MirFunction &fn, BasicBlock &bb, int64_t v);
  Value constBool(MirFunction &fn, BasicBlock &bb, bool v);
  Value constStr(MirFunction &fn, BasicBlock &bb, const std::string &v);
  Value add(MirFunction &fn, BasicBlock &bb, Value a, Value b);
  Value sub(MirFunction &fn, BasicBlock &bb, Value a, Value b);
  Value mul(MirFunction &fn, BasicBlock &bb, Value a, Value b);
  Value div(MirFunction &fn, BasicBlock &bb, Value a, Value b);
  Value cmp(MirFunction &fn, BasicBlock &bb, Op cmpOp, Value a, Value b);
  Value call(MirFunction &fn, BasicBlock &bb, const std::string &callee,
             const std::vector<Value> &args, MirType retType);
  Value alloc(MirFunction &fn, BasicBlock &bb, const std::string &slot);
  Value load(MirFunction &fn, BasicBlock &bb, const std::string &slot,
             MirType type);
  void store(BasicBlock &bb, const std::string &slot, Value v);
  void br(BasicBlock &bb, const std::string &target);
  void condBr(BasicBlock &bb, Value cond, const std::string &trueTarget,
              const std::string &falseTarget);
  void ret(MirFunction &fn, BasicBlock &bb);
  void ret(MirFunction &fn, BasicBlock &bb, Value v);
  void unreachable(BasicBlock &bb);
  void print(BasicBlock &bb, Value v);

private:
  uint32_t fresh(MirFunction &fn);
  Value binop(MirFunction &fn, BasicBlock &bb, Op op, Value a, Value b);
  MirModule &mod_;
};

// ---- Verifier -----------------------------------------------------

struct VerifyError {
  std::string message;
};

// `builtins` = nama callee eksternal yang diizinkan (mis. dari
// `frontend::isBuiltin`; MIR sendiri tidak bergantung ke frontend).
bool verify(const MirModule &mod, std::vector<VerifyError> &errors,
            const std::vector<std::string> &builtins = {});

// ---- Printer (dump teks deterministik; untuk test/debug) -----------

std::string dump(const MirModule &mod);

} // namespace mir
} // namespace nusa
