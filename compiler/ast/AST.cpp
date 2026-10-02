#include "compiler/ast/AST.h"

namespace nusa {

std::string binOpName(BinOp op) {
  switch (op) {
  case BinOp::Add: return "+";
  case BinOp::Sub: return "-";
  case BinOp::Mul: return "*";
  case BinOp::Div: return "/";
  case BinOp::Eq: return "==";
  case BinOp::NotEq: return "!=";
  case BinOp::Lt: return "<";
  case BinOp::Gt: return ">";
  case BinOp::LtEq: return "<=";
  case BinOp::GtEq: return ">=";
  }
  return "?";
}

const Location &Expr::loc() const {
  return std::visit(
      [](const auto &n) -> const Location & { return n.loc; }, node);
}

static std::string escapeForDump(const std::string &s) {
  std::string out;
  for (char c : s) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\t': out += "\\t"; break;
    default: out.push_back(c); break;
    }
  }
  return out;
}

std::string Expr::dump() const {
  struct Visitor {
    std::string operator()(const LiteralInt &n) const {
      return std::to_string(n.value);
    }
    std::string operator()(const LiteralBool &n) const {
      return n.value ? "true" : "false";
    }
    std::string operator()(const LiteralString &n) const {
      return "\"" + escapeForDump(n.value) + "\"";
    }
    std::string operator()(const VarRef &n) const { return n.name; }
    std::string operator()(const Call &n) const {
      std::string out = "(call " + n.callee;
      for (const auto &a : n.args)
        out += " " + a->dump();
      return out + ")";
    }
    std::string operator()(const MethodCall &n) const {
      std::string out =
          "(call-method " + n.receiver->dump() + " " + n.method;
      if (!n.targs.empty()) {
        out += "<";
        for (size_t i = 0; i < n.targs.size(); i++) {
          if (i)
            out += ", ";
          out += n.targs[i].name();
        }
        out += ">";
      }
      for (const auto &a : n.args)
        out += " " + a->dump();
      return out + ")";
    }
    std::string operator()(const Propagate &n) const {
      return "(propagate " + n.expr->dump() + ")";
    }
    std::string operator()(const ArrayLit &n) const {
      std::string out = "(array";
      for (const auto &e : n.elems)
        out += " " + e->dump();
      return out + ")";
    }
    std::string operator()(const Index &n) const {
      return "(index " + n.base->dump() + " " + n.index->dump() + ")";
    }
    std::string operator()(const StructLit &n) const {
      std::string out = "(" + n.name;
      if (!n.targs.empty()) {
        out += "<";
        for (size_t i = 0; i < n.targs.size(); i++) {
          if (i)
            out += ", ";
          out += n.targs[i].name();
        }
        out += ">";
      }
      for (const auto &f : n.fields)
        out += " (" + f.name + " " + f.value->dump() + ")";
      return out + ")";
    }
    std::string operator()(const FieldAccess &n) const {
      return "(field " + n.base->dump() + " " + n.field + ")";
    }
    std::string operator()(const Unary &n) const {
      return std::string("(") + (n.op == UnOp::Neg ? "-" : "+") + " " +
             n.operand->dump() + ")";
    }
    std::string operator()(const Binary &n) const {
      return "(" + binOpName(n.op) + " " + n.lhs->dump() + " " +
             n.rhs->dump() + ")";
    }
    std::string operator()(const Lambda &n) const {
      std::string out = "(lambda (";
      for (size_t i = 0; i < n.params.size(); i++) {
        if (i)
          out += " ";
        out += n.params[i].name;
        if (n.params[i].annot)
          out += ": " + n.params[i].annot->name();
      }
      return out + ") " + n.body->dump() + ")";
    }
    std::string operator()(const Closure &n) const {
      return "(closure " + n.lifted + " " + n.envStruct + ")";
    }
  };
  return std::visit(Visitor{}, node);
}

std::string LetStmt::dump() const {
  std::string head = "(let " + name;
  if (declaredType)
    head += ": " + declaredType->name();
  return head + " " + value->dump() + ")";
}

std::string PrintStmt::dump() const { return "(print " + arg->dump() + ")"; }

std::string Pat::dump() const {
  struct Visitor {
    std::string operator()(const Pat::Wild &) const { return "_"; }
    std::string operator()(const Pat::Bind &n) const { return n.name; }
    std::string operator()(const Pat::LitInt &n) const {
      return std::to_string(n.value);
    }
    std::string operator()(const Pat::LitBool &n) const {
      return n.value ? "true" : "false";
    }
    std::string operator()(const Pat::LitStr &n) const {
      return "\"" + n.value + "\"";
    }
    std::string operator()(const Pat::Struct &n) const {
      std::string out = n.name;
      if (!n.targs.empty()) {
        out += "<";
        for (size_t i = 0; i < n.targs.size(); i++) {
          if (i)
            out += ", ";
          out += n.targs[i].name();
        }
        out += ">";
      }
      out += " { ";
      for (size_t i = 0; i < n.fields.size(); i++) {
        if (i)
          out += ", ";
        out += n.fields[i].name + ": " + n.fields[i].pat->dump();
      }
      return out + " }";
    }
    std::string operator()(const Pat::Variant &n) const {
      if (n.args.empty())
        return n.name;
      std::string out = n.name + "(";
      for (size_t i = 0; i < n.args.size(); i++) {
        if (i)
          out += ", ";
        out += n.args[i]->dump();
      }
      return out + ")";
    }
  };
  return std::visit(Visitor{}, node);
}

std::string MatchStmt::dump() const {
  std::string out = "(match " + subject->dump();
  for (const auto &a : arms) {
    out += " (" + a.pat.dump();
    if (a.guard)
      out += " (if " + a.guard->dump() + ")";
    out += " " + a.body->dump() + ")";
  }
  return out + ")";
}

std::string ReturnStmt::dump() const {
  if (value)
    return "(return " + value->dump() + ")";
  return "(return)";
}

std::string ExprStmt::dump() const { return "(do " + expr->dump() + ")"; }

std::string IfStmt::dump() const {
  std::string out = "(if " + cond->dump() + " (" + thenBlock->dump() + ")";
  if (elseBlock)
    out += " (" + elseBlock->dump() + ")";
  return out + ")";
}

std::string WhileStmt::dump() const {
  return "(while " + cond->dump() + " (" + body->dump() + "))";
}

std::string AssignStmt::dump() const {
  return "(set " + name + " " + value->dump() + ")";
}

std::string Block::dump() const {
  std::string out;
  for (size_t i = 0; i < stmts.size(); i++) {
    if (i)
      out += " ";
    out += std::visit([](const auto &s) { return s.dump(); }, stmts[i]);
  }
  return out;
}

std::string Param::dump() const {
  // Receiver `self` (Phase 22): marker tipe tak ditampilkan.
  if (name == "self" && type.kind == TypeKind::Struct && type.sname &&
      *type.sname == "self" && type.sargs.empty())
    return name;
  return name + ": " + type.name();
}

std::string StructField::dump() const { return name + ": " + type.name(); }

std::string StructDecl::dump() const {
  std::string out = "(struct " + name;
  if (!tparams.empty()) {
    out += "<";
    for (size_t i = 0; i < tparams.size(); i++) {
      if (i)
        out += ", ";
      out += tparams[i];
    }
    out += ">";
  }
  for (const auto &f : fields)
    out += " (" + f.dump() + ")";
  return out + ")";
}

std::string Import::dump() const { return "(import " + module + ")"; }

std::string TraitDecl::dump() const {
  std::string out = "(trait " + name;
  for (const auto &m : methods)
    out += " " + m.dump();
  return out + ")";
}

std::string ImplBlock::dump() const {
  std::string out =
      "(impl " + trait + " for " + target.name();
  for (const auto &m : methods)
    out += " " + m.dump();
  return out + ")";
}

std::string TypeAliasDecl::dump() const {
  return "(type " + name + " " + target.name() + ")";
}

std::string EnumVariant::dump() const {
  if (payload.empty())
    return name;
  std::string out = name + "(";
  for (size_t i = 0; i < payload.size(); i++) {
    if (i)
      out += ", ";
    out += payload[i].name();
  }
  return out + ")";
}

std::string EnumDecl::dump() const {
  std::string out = "(enum " + name;
  if (!tparams.empty()) {
    out += "<";
    for (size_t i = 0; i < tparams.size(); i++) {
      if (i)
        out += ", ";
      out += tparams[i];
    }
    out += ">";
  }
  for (const auto &v : variants)
    out += " (" + v.dump() + ")";
  return out + ")";
}

std::string ConstDecl::dump() const {
  std::string out = "(const " + name;
  if (declaredType)
    out += ": " + declaredType->name();
  return out + " " + value->dump() + ")";
}

std::string FunctionDecl::dump() const {
  std::string out = "(fn " + name;
  if (!tparams.empty()) {
    out += "<";
    for (size_t i = 0; i < tparams.size(); i++) {
      if (i)
        out += ", ";
      out += tparams[i];
      auto bit = tparamBounds.find(tparams[i]);
      if (bit != tparamBounds.end()) {
        out += ": ";
        for (size_t j = 0; j < bit->second.size(); j++) {
          if (j)
            out += " + ";
          out += bit->second[j];
        }
      }
    }
    out += ">";
  }
  out += "(";
  for (size_t i = 0; i < params.size(); i++) {
    if (i)
      out += ", ";
    out += params[i].dump();
  }
  return out + ") -> " + retType.name() + " " + body.dump() + ")";
}

std::string Program::dump() const {
  if (imports.empty() && structs.empty() && aliases.empty() &&
      traits.empty() && impls.empty() && enums.empty() &&
      consts.empty() && functions.empty())
    return "(empty)";
  std::string out = "(program";
  for (const auto &i : imports)
    out += " " + i.dump();
  for (const auto &a : aliases)
    out += " " + a.dump();
  for (const auto &t : traits)
    out += " " + t.dump();
  for (const auto &ib : impls)
    out += " " + ib.dump();
  for (const auto &s : structs)
    out += " " + s.dump();
  for (const auto &e : enums)
    out += " " + e.dump();
  for (const auto &c : consts)
    out += " " + c.dump();
  for (const auto &f : functions)
    out += " " + f.dump();
  return out + ")";
}

std::optional<size_t> Program::mainIndex() const {
  for (size_t i = 0; i < functions.size(); i++)
    if (functions[i].name == "main")
      return i;
  return std::nullopt;
}

std::unique_ptr<Expr> cloneExpr(const Expr &e) {
  auto out = std::make_unique<Expr>();
  // (Eksplisit per varian — tanpa helper macro agar tiap field
  // terlihat di review.)
  if (const auto *n = std::get_if<Expr::LiteralInt>(&e.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Expr::LiteralBool>(&e.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Expr::LiteralString>(&e.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Expr::VarRef>(&e.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Expr::Call>(&e.node)) {
    Expr::Call c;
    c.callee = n->callee;
    c.loc = n->loc;
    for (const auto &a : n->args)
      c.args.push_back(cloneExpr(*a));
    out->node = std::move(c);
  } else if (const auto *n = std::get_if<Expr::MethodCall>(&e.node)) {
    Expr::MethodCall m;
    m.receiver = cloneExpr(*n->receiver);
    m.method = n->method;
    m.loc = n->loc;
    m.targs = n->targs;
    for (const auto &a : n->args)
      m.args.push_back(cloneExpr(*a));
    out->node = std::move(m);
  } else if (const auto *n = std::get_if<Expr::Propagate>(&e.node)) {
    Expr::Propagate p;
    p.expr = cloneExpr(*n->expr);
    p.loc = n->loc;
    p.okTag = n->okTag;
    p.okSlot = n->okSlot;
    p.errSlot = n->errSlot;
    out->node = std::move(p);
  } else if (const auto *n = std::get_if<Expr::ArrayLit>(&e.node)) {
    Expr::ArrayLit a;
    a.loc = n->loc;
    for (const auto &el : n->elems)
      a.elems.push_back(cloneExpr(*el));
    out->node = std::move(a);
  } else if (const auto *n = std::get_if<Expr::Index>(&e.node)) {
    Expr::Index idx;
    idx.base = cloneExpr(*n->base);
    idx.index = cloneExpr(*n->index);
    idx.loc = n->loc;
    out->node = std::move(idx);
  } else if (const auto *n = std::get_if<Expr::StructLit>(&e.node)) {
    Expr::StructLit lit;
    lit.name = n->name;
    lit.loc = n->loc;
    lit.targs = n->targs;
    lit.enumCtor = n->enumCtor;
    lit.enumType = n->enumType;
    for (const auto &f : n->fields) {
      Expr::StructLit::FieldInit fi;
      fi.name = f.name;
      fi.value = cloneExpr(*f.value);
      fi.loc = f.loc;
      lit.fields.push_back(std::move(fi));
    }
    out->node = std::move(lit);
  } else if (const auto *n = std::get_if<Expr::FieldAccess>(&e.node)) {
    Expr::FieldAccess fa;
    fa.base = cloneExpr(*n->base);
    fa.field = n->field;
    fa.loc = n->loc;
    out->node = std::move(fa);
  } else if (const auto *n = std::get_if<Expr::Unary>(&e.node)) {
    Expr::Unary u;
    u.op = n->op;
    u.operand = cloneExpr(*n->operand);
    u.loc = n->loc;
    out->node = std::move(u);
  } else if (const auto *n = std::get_if<Expr::Binary>(&e.node)) {
    Expr::Binary b;
    b.op = n->op;
    b.lhs = cloneExpr(*n->lhs);
    b.rhs = cloneExpr(*n->rhs);
    b.loc = n->loc;
    out->node = std::move(b);
  } else if (const auto *n = std::get_if<Expr::Lambda>(&e.node)) {
    Expr::Lambda l;
    l.params = n->params;
    l.body = std::make_unique<Block>(cloneBlock(*n->body));
    l.loc = n->loc;
    l.expectParams = n->expectParams;
    l.expectRet = n->expectRet;
    l.expected = n->expected;
    l.bad = n->bad;
    out->node = std::move(l);
  } else if (const auto *n = std::get_if<Expr::Closure>(&e.node)) {
    Expr::Closure c;
    c.lifted = n->lifted;
    c.envStruct = n->envStruct;
    for (const auto &cap : n->captures)
      c.captures.push_back(cloneExpr(*cap));
    c.funcType = n->funcType;
    c.loc = n->loc;
    out->node = std::move(c);
  }
  return out;
}

Stmt cloneStmt(const Stmt &s) {
  if (const auto *n = std::get_if<LetStmt>(&s)) {
    LetStmt ls;
    ls.name = n->name;
    ls.loc = n->loc;
    ls.declaredType = n->declaredType;
    ls.value = cloneExpr(*n->value);
    return Stmt{std::move(ls)};
  }
  if (const auto *n = std::get_if<PrintStmt>(&s)) {
    PrintStmt ps;
    ps.arg = cloneExpr(*n->arg);
    ps.loc = n->loc;
    return Stmt{std::move(ps)};
  }
  if (const auto *n = std::get_if<ReturnStmt>(&s)) {
    ReturnStmt rs;
    rs.loc = n->loc;
    if (n->value)
      rs.value = cloneExpr(*n->value);
    return Stmt{std::move(rs)};
  }
  if (const auto *n = std::get_if<ExprStmt>(&s)) {
    ExprStmt es;
    es.expr = cloneExpr(*n->expr);
    es.loc = n->loc;
    return Stmt{std::move(es)};
  }
  if (const auto *n = std::get_if<IfStmt>(&s)) {
    IfStmt is;
    is.cond = cloneExpr(*n->cond);
    is.thenBlock = std::make_unique<Block>(cloneBlock(*n->thenBlock));
    if (n->elseBlock)
      is.elseBlock = std::make_unique<Block>(cloneBlock(*n->elseBlock));
    is.loc = n->loc;
    return Stmt{std::move(is)};
  }
  if (const auto *n = std::get_if<WhileStmt>(&s)) {
    WhileStmt ws;
    ws.cond = cloneExpr(*n->cond);
    ws.body = std::make_unique<Block>(cloneBlock(*n->body));
    ws.loc = n->loc;
    return Stmt{std::move(ws)};
  }
  const auto *n = std::get_if<AssignStmt>(&s);
  if (n) {
    AssignStmt as;
    as.name = n->name;
    as.value = cloneExpr(*n->value);
    as.loc = n->loc;
    return Stmt{std::move(as)};
  }
  const auto *m = std::get_if<MatchStmt>(&s);
  MatchStmt ms;
  ms.subject = cloneExpr(*m->subject);
  ms.loc = m->loc;
  for (const auto &a : m->arms) {
    MatchArm arm;
    arm.pat = std::move(*clonePat(a.pat));
    if (a.guard)
      arm.guard = cloneExpr(*a.guard);
    arm.body = std::make_unique<Block>(cloneBlock(*a.body));
    arm.loc = a.loc;
    ms.arms.push_back(std::move(arm));
  }
  return Stmt{std::move(ms)};
}

Block cloneBlock(const Block &b) {
  Block out;
  for (const auto &s : b.stmts)
    out.stmts.push_back(cloneStmt(s));
  return out;
}

std::unique_ptr<Pat> clonePat(const Pat &p) {
  auto out = std::make_unique<Pat>();
  if (const auto *n = std::get_if<Pat::Wild>(&p.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Pat::Bind>(&p.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Pat::LitInt>(&p.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Pat::LitBool>(&p.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Pat::LitStr>(&p.node)) {
    out->node = *n;
  } else if (const auto *n = std::get_if<Pat::Struct>(&p.node)) {
    Pat::Struct s;
    s.name = n->name;
    s.targs = n->targs;
    s.loc = n->loc;
    for (const auto &f : n->fields) {
      Pat::Struct::Field fi;
      fi.name = f.name;
      fi.pat = clonePat(*f.pat);
      fi.loc = f.loc;
      s.fields.push_back(std::move(fi));
    }
    out->node = std::move(s);
  } else if (const auto *n = std::get_if<Pat::Variant>(&p.node)) {
    Pat::Variant v;
    v.name = n->name;
    v.loc = n->loc;
    v.tag = n->tag;
    for (const auto &a : n->args)
      v.args.push_back(clonePat(*a));
    out->node = std::move(v);
  }
  return out;
}

} // namespace nusa
