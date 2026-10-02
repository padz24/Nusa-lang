#include "compiler/types/Type.h"

namespace nusa {

std::string Type::name() const {
  switch (kind) {
  case TypeKind::I64: return "i64";
  case TypeKind::Bool: return "bool";
  case TypeKind::String: return "string";
  case TypeKind::Void: return "void";
  case TypeKind::Error: return "<error>";
  case TypeKind::Param: return pname ? *pname : "<param>";
  case TypeKind::Array:
    return "[" + (elem ? elem->name() : "<error>") + "]";
  case TypeKind::Struct: {
    std::string base = sname ? *sname : "<unknown>";
    if (sargs.empty())
      return base;
    std::string s = base + "<";
    for (size_t i = 0; i < sargs.size(); i++) {
      if (i)
        s += ", ";
      s += sargs[i].name();
    }
    return s + ">";
  }
  case TypeKind::Func: {
    std::string s = "fn(";
    for (size_t i = 0; i < params.size(); i++) {
      if (i)
        s += ", ";
      s += params[i].name();
    }
    s += ") -> ";
    s += ret ? ret->name() : "<error>";
    return s;
  }
  }
  return "<error>";
}

bool Type::operator==(const Type &other) const {
  if (kind != other.kind)
    return false;
  if (kind == TypeKind::Func) {
    if (params.size() != other.params.size())
      return false;
    for (size_t i = 0; i < params.size(); i++)
      if (params[i] != other.params[i])
        return false;
    if (bool(ret) != bool(other.ret))
      return false;
    if (ret && *ret != *other.ret)
      return false;
  }
  if (kind == TypeKind::Array) {
    if (bool(elem) != bool(other.elem))
      return false;
    if (elem && *elem != *other.elem)
      return false;
    return true;
  }
  if (kind == TypeKind::Struct) {
    if (sname != other.sname)
      return false;
    if (sargs.size() != other.sargs.size())
      return false;
    for (size_t i = 0; i < sargs.size(); i++) {
      if (sargs[i] != other.sargs[i])
        return false;
    }
    return true;
  }
  if (kind == TypeKind::Param) {
    return pname == other.pname;
  }
  return true;
}

} // namespace nusa
