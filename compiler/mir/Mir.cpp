// Nusa MIR — implementasi fondasi (Phase 52). Hanya header standar C++.
#include "compiler/mir/Mir.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace nusa {
namespace mir {

std::string MirType::name() const {
  switch (kind) {
  case TypeKind::I64: return "i64";
  case TypeKind::Bool: return "bool";
  case TypeKind::Str: return "string";
  case TypeKind::Void: return "void";
  case TypeKind::Ptr: return "ptr";
  }
  return "void";
}

// ---- Builder ------------------------------------------------------

MirFunction &MirBuilder::newFunction(
    const std::string &name,
    const std::vector<std::pair<std::string, MirType>> &params, MirType ret,
    const std::string &entry) {
  MirFunction fn;
  fn.name = name;
  fn.params = params;
  fn.ret = ret;
  fn.entry = entry;
  mod_.functions.push_back(fn);
  mod_.functions.back().nextValueId =
      static_cast<uint32_t>(params.size());
  return mod_.functions.back();
}

BasicBlock &MirBuilder::addBlock(MirFunction &fn, const std::string &label) {
  BasicBlock bb;
  bb.label = label;
  fn.blocks.push_back(bb);
  return fn.blocks.back();
}

uint32_t MirBuilder::fresh(MirFunction &fn) {
  // Counter hidup di fungsi (bukan vector paralel di builder) sehingga
  // builder baru di atas modul tak-kosong tetap benar. Lazy-init dari
  // isi bila fungsi dibangun manual (nextValueId == 0 + ada isi).
  if (fn.nextValueId == 0 &&
      (!fn.blocks.empty() || !fn.params.empty())) {
    uint32_t maxId = static_cast<uint32_t>(fn.params.size());
    for (const auto &bb : fn.blocks) {
      for (const auto &in : bb.instrs) {
        if (in.hasResult && in.result >= maxId)
          maxId = in.result + 1;
        for (uint32_t op : in.operands) {
          if (op >= maxId)
            maxId = op + 1;
        }
      }
    }
    fn.nextValueId = maxId;
  }
  return fn.nextValueId++;
}

Value MirBuilder::constI64(MirFunction &fn, BasicBlock &bb, int64_t v) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = MirType::i64();
  in.op = Op::ConstI64;
  in.intVal = v;
  bb.instrs.push_back(in);
  return Value{id, MirType::i64()};
}

Value MirBuilder::constBool(MirFunction &fn, BasicBlock &bb, bool v) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = MirType::boolean();
  in.op = Op::ConstBool;
  in.boolVal = v;
  bb.instrs.push_back(in);
  return Value{id, MirType::boolean()};
}

Value MirBuilder::constStr(MirFunction &fn, BasicBlock &bb,
                           const std::string &v) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = MirType::str();
  in.op = Op::ConstStr;
  in.strVal = v;
  bb.instrs.push_back(in);
  return Value{id, MirType::str()};
}

Value MirBuilder::binop(MirFunction &fn, BasicBlock &bb, Op op, Value a,
                        Value b) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = MirType::i64();
  in.op = op;
  in.operands = {a.id, b.id};
  bb.instrs.push_back(in);
  return Value{id, MirType::i64()};
}

Value MirBuilder::add(MirFunction &fn, BasicBlock &bb, Value a, Value b) {
  return binop(fn, bb, Op::Add, a, b);
}
Value MirBuilder::sub(MirFunction &fn, BasicBlock &bb, Value a, Value b) {
  return binop(fn, bb, Op::Sub, a, b);
}
Value MirBuilder::mul(MirFunction &fn, BasicBlock &bb, Value a, Value b) {
  return binop(fn, bb, Op::Mul, a, b);
}
Value MirBuilder::div(MirFunction &fn, BasicBlock &bb, Value a, Value b) {
  return binop(fn, bb, Op::Div, a, b);
}

Value MirBuilder::cmp(MirFunction &fn, BasicBlock &bb, Op cmpOp, Value a,
                      Value b) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = MirType::boolean();
  in.op = cmpOp;
  in.operands = {a.id, b.id};
  bb.instrs.push_back(in);
  return Value{id, MirType::boolean()};
}

Value MirBuilder::call(MirFunction &fn, BasicBlock &bb,
                       const std::string &callee,
                       const std::vector<Value> &args, MirType retType) {
  Instr in;
  in.op = Op::Call;
  in.callee = callee;
  for (const auto &a : args)
    in.operands.push_back(a.id);
  if (retType != MirType::void_()) {
    uint32_t id = fresh(fn);
    in.hasResult = true;
    in.result = id;
    in.resultType = retType;
    bb.instrs.push_back(in);
    return Value{id, retType};
  }
  bb.instrs.push_back(in);
  return Value{0, MirType::void_()};
}

Value MirBuilder::alloc(MirFunction &fn, BasicBlock &bb,
                        const std::string &slot) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = MirType::ptr();
  in.op = Op::Alloc;
  in.slot = slot;
  bb.instrs.push_back(in);
  return Value{id, MirType::ptr()};
}

Value MirBuilder::load(MirFunction &fn, BasicBlock &bb, const std::string &slot,
                       MirType type) {
  uint32_t id = fresh(fn);
  Instr in;
  in.hasResult = true;
  in.result = id;
  in.resultType = type;
  in.op = Op::Load;
  in.slot = slot;
  bb.instrs.push_back(in);
  return Value{id, type};
}

void MirBuilder::store(BasicBlock &bb, const std::string &slot, Value v) {
  Instr in;
  in.op = Op::Store;
  in.slot = slot;
  in.operands = {v.id};
  bb.instrs.push_back(in);
}

void MirBuilder::br(BasicBlock &bb, const std::string &target) {
  Instr in;
  in.op = Op::Branch;
  in.target = target;
  bb.instrs.push_back(in);
}

void MirBuilder::condBr(BasicBlock &bb, Value cond,
                        const std::string &trueTarget,
                        const std::string &falseTarget) {
  Instr in;
  in.op = Op::CondBranch;
  in.operands = {cond.id};
  in.trueTarget = trueTarget;
  in.falseTarget = falseTarget;
  bb.instrs.push_back(in);
}

void MirBuilder::ret(MirFunction &fn, BasicBlock &bb) {
  (void)fn;
  Instr in;
  in.op = Op::Return;
  bb.instrs.push_back(in);
}

void MirBuilder::ret(MirFunction &fn, BasicBlock &bb, Value v) {
  (void)fn;
  Instr in;
  in.op = Op::Return;
  in.operands = {v.id};
  bb.instrs.push_back(in);
}

void MirBuilder::unreachable(BasicBlock &bb) {
  Instr in;
  in.op = Op::Unreachable;
  bb.instrs.push_back(in);
}

void MirBuilder::print(BasicBlock &bb, Value v) {
  Instr in;
  in.op = Op::Print;
  in.operands = {v.id};
  bb.instrs.push_back(in);
}

// ---- Verifier -----------------------------------------------------

bool verify(const MirModule &mod, std::vector<VerifyError> &errors,
            const std::vector<std::string> &builtins) {
  std::unordered_set<std::string> builtinSet(builtins.begin(), builtins.end());
  std::unordered_map<std::string, const MirFunction *> funcs;
  for (const auto &fn : mod.functions)
    funcs[fn.name] = &fn;

  for (const auto &fn : mod.functions) {
    std::string ctx = "func @" + fn.name + ": ";
    if (fn.blocks.empty()) {
      errors.push_back({ctx + "no basic blocks"});
      continue;
    }
    std::unordered_set<std::string> labels;
    for (const auto &bb : fn.blocks) {
      if (!labels.insert(bb.label).second)
        errors.push_back({ctx + "duplicate block '" + bb.label + "'"});
    }
    if (!labels.count(fn.entry))
      errors.push_back({ctx + "entry '" + fn.entry + "' missing"});

    // Definisi yang terlihat: parameter + hasil instruksi (urut).
    std::unordered_map<uint32_t, MirType> defined;
    for (uint32_t i = 0; i < fn.params.size(); i++)
      defined[i] = fn.params[i].second;
    std::unordered_set<uint32_t> allResults;
    for (const auto &bb : fn.blocks) {
      for (const auto &in : bb.instrs) {
        if (in.hasResult) {
          if (!allResults.insert(in.result).second)
            errors.push_back({ctx + "duplicate value %" +
                              std::to_string(in.result)});
          defined[in.result] = in.resultType;
        }
      }
    }

    auto operandType = [&](uint32_t id, bool &ok) -> MirType {
      auto it = defined.find(id);
      if (it == defined.end()) {
        ok = false;
        return MirType::void_();
      }
      return it->second;
    };

    for (const auto &bb : fn.blocks) {
      std::string bctx = ctx + "block '" + bb.label + "': ";
      if (bb.instrs.empty()) {
        errors.push_back({bctx + "empty (missing terminator)"});
        continue;
      }
      for (size_t i = 0; i < bb.instrs.size(); i++) {
        const Instr &in = bb.instrs[i];
        bool last = (i + 1 == bb.instrs.size());
        if (isTerminator(in.op) && !last)
          errors.push_back({bctx + "terminator not last"});
        if (!isTerminator(in.op) && last)
          errors.push_back({bctx + "missing terminator"});
        bool ok = true;
        for (uint32_t op : in.operands) {
          bool fok = true;
          operandType(op, fok);
          if (!fok) {
            errors.push_back({bctx + "undefined operand %" +
                              std::to_string(op)});
            ok = false;
          }
        }
        if (!ok)
          continue;
        auto ty = [&](uint32_t id) { return defined[id]; };
        switch (in.op) {
        case Op::ConstI64:
          if (in.resultType != MirType::i64())
            errors.push_back({bctx + "const i64 mistyped"});
          break;
        case Op::ConstBool:
          if (in.resultType != MirType::boolean())
            errors.push_back({bctx + "const bool mistyped"});
          break;
        case Op::ConstStr:
          if (in.resultType != MirType::str())
            errors.push_back({bctx + "const string mistyped"});
          break;
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
          if (in.operands.size() != 2 || ty(in.operands[0]) != MirType::i64() ||
              ty(in.operands[1]) != MirType::i64() ||
              in.resultType != MirType::i64())
            errors.push_back({bctx + "arith requires i64"});
          break;
        case Op::CmpLt:
        case Op::CmpGt:
        case Op::CmpLtEq:
        case Op::CmpGtEq:
        case Op::CmpEq:
        case Op::CmpNotEq:
          if (in.operands.size() != 2 ||
              ty(in.operands[0]) != ty(in.operands[1]) ||
              ty(in.operands[0]) == MirType::void_() ||
              in.resultType != MirType::boolean())
            errors.push_back({bctx + "cmp requires same non-void operands"});
          break;
        case Op::Call: {
          if (!funcs.count(in.callee) && !builtinSet.count(in.callee))
            errors.push_back({bctx + "unknown callee '" + in.callee + "'"});
          auto it = funcs.find(in.callee);
          if (it != funcs.end()) {
            const MirFunction *callee = it->second;
            if (callee->params.size() != in.operands.size())
              errors.push_back({bctx + "arity mismatch calling @" +
                                in.callee + "'"});
            else {
              for (size_t k = 0; k < in.operands.size(); k++) {
                if (ty(in.operands[k]) != callee->params[k].second)
                  errors.push_back({bctx + "arg type mismatch calling @" +
                                    in.callee + "'"});
              }
            }
            if (in.hasResult) {
              if (callee->ret == MirType::void_() ||
                  in.resultType != callee->ret)
                errors.push_back({bctx + "return type mismatch calling @" +
                                  in.callee + "'"});
            } else if (callee->ret != MirType::void_()) {
              errors.push_back({bctx + "missing result calling @" +
                                in.callee + "'"});
            }
          } else if (in.hasResult && in.resultType == MirType::void_()) {
            errors.push_back({bctx + "void call result"});
          }
          break;
        }
        case Op::Alloc:
          if (!in.hasResult || in.resultType != MirType::ptr() ||
              in.slot.empty())
            errors.push_back({bctx + "bad alloc"});
          break;
        case Op::Load:
          if (!in.hasResult || in.slot.empty() ||
              in.resultType == MirType::void_())
            errors.push_back({bctx + "bad load"});
          break;
        case Op::Store:
          if (in.hasResult || in.operands.size() != 1 || in.slot.empty())
            errors.push_back({bctx + "bad store"});
          break;
        case Op::Branch:
          if (!labels.count(in.target))
            errors.push_back({bctx + "unknown target '" + in.target + "'"});
          break;
        case Op::CondBranch:
          if (in.operands.size() != 1 || ty(in.operands[0]) != MirType::boolean())
            errors.push_back({bctx + "cond must be bool"});
          if (!labels.count(in.trueTarget) || !labels.count(in.falseTarget))
            errors.push_back({bctx + "unknown branch target"});
          break;
        case Op::Return:
          if (fn.ret == MirType::void_()) {
            if (!in.operands.empty())
              errors.push_back({bctx + "void return with value"});
          } else if (in.operands.size() != 1 ||
                     ty(in.operands[0]) != fn.ret) {
            errors.push_back({bctx + "return type mismatch"});
          }
          break;
        case Op::Unreachable:
        case Op::Print:
          break;
        }
      }
    }
  }
  return errors.empty();
}

// ---- Printer ------------------------------------------------------

static std::string opStr(Op op) {
  switch (op) {
  case Op::ConstI64: return "const";
  case Op::ConstBool: return "const";
  case Op::ConstStr: return "const";
  case Op::Add: return "add";
  case Op::Sub: return "sub";
  case Op::Mul: return "mul";
  case Op::Div: return "div";
  case Op::CmpLt: return "cmplt";
  case Op::CmpGt: return "cmpgt";
  case Op::CmpLtEq: return "cmplte";
  case Op::CmpGtEq: return "cmpgte";
  case Op::CmpEq: return "cmpeq";
  case Op::CmpNotEq: return "cmpne";
  case Op::Call: return "call";
  case Op::Alloc: return "alloc";
  case Op::Load: return "load";
  case Op::Store: return "store";
  case Op::Branch: return "br";
  case Op::CondBranch: return "condbr";
  case Op::Return: return "ret";
  case Op::Unreachable: return "unreachable";
  case Op::Print: return "print";
  }
  return "?";
}

std::string dump(const MirModule &mod) {
  std::ostringstream out;
  for (const auto &fn : mod.functions) {
    out << "func @" << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); i++) {
      if (i)
        out << ", ";
      out << "%" << i << ": " << fn.params[i].second.name();
    }
    out << ") -> " << fn.ret.name() << " {\n";
    for (const auto &bb : fn.blocks) {
      out << bb.label << ":\n";
      for (const auto &in : bb.instrs) {
        out << "  ";
        if (in.hasResult)
          out << "%" << in.result << ": " << in.resultType.name() << " = ";
        out << opStr(in.op);
        switch (in.op) {
        case Op::ConstI64:
          out << " " << in.intVal;
          break;
        case Op::ConstBool:
          out << (in.boolVal ? " true" : " false");
          break;
        case Op::ConstStr:
          out << " \"" << in.strVal << "\"";
          break;
        case Op::Call:
          out << " @" << in.callee;
          for (uint32_t op : in.operands)
            out << " %" << op;
          break;
        case Op::Alloc:
        case Op::Load:
          out << " $" << in.slot;
          break;
        case Op::Store:
          out << " $" << in.slot << " %" << in.operands[0];
          break;
        case Op::Branch:
          out << " " << in.target;
          break;
        case Op::CondBranch:
          out << " %" << in.operands[0] << " " << in.trueTarget << " "
              << in.falseTarget;
          break;
        case Op::Return:
          if (!in.operands.empty())
            out << " %" << in.operands[0];
          break;
        default:
          for (uint32_t op : in.operands)
            out << " %" << op;
          break;
        }
        out << "\n";
      }
    }
    out << "}\n";
  }
  return out.str();
}

} // namespace mir
} // namespace nusa
