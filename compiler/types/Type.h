#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nusa {

// Sistem tipe statis Nusa — v0.4.0 + Array v0.7.0 + Struct v0.8.0
// (tanpa generics / advanced types).
//
// Tipe dasar: i64, bool, string, void (+ Func untuk function types,
// + Array untuk fixed-size arrays (elemen homogen, ukuran dari literal),
// + Struct untuk user structs (field layout di checker/codegen),
// + Error untuk pemulihan setelah error agar tidak ada error beruntun).
//
// `Type` adalah value type yang dapat dicopy (Func/Array memakai
// shared_ptr). Perbandingan dengan operator==. Ukuran array BUKAN
// bagian tipe (`[i64]` cocok untuk semua panjang) — panjang
// dilacak codegen dari situs alokasi.
enum class TypeKind {
  I64,
  Bool,
  String,
  Void,
  Func,
  Array,
  Struct,
  // Parameter tipe generik (Phase 21c): `T` dalam `fn f<T>(x: T)`.
  // Rigid selama inferensi/instansiasi; tak pernah mencapai codegen
  // (instance selalu konkret; template dibuang checker).
  Param,
  Error
};

struct Type {
  TypeKind kind = TypeKind::Error;
  // Hanya bermakna untuk Func: fn(param...) -> ret.
  std::vector<Type> params;
  std::shared_ptr<Type> ret;
  // Hanya bermakna untuk Array: tipe elemen (non-void, non-array).
  std::shared_ptr<Type> elem;
  // Hanya bermakna untuk Struct: nama struct (layout di checker/codegen).
  std::optional<std::string> sname;
  // Hanya bermakna untuk Struct generik (Phase 21d): argumen tipe
  // (`Point<i64>`). Kosong = struct biasa. Bentuk konkret dijamin
  // punya instance layout (checker); bentuk rigid (berisi Param) hanya
  // hidup di signature template.
  std::vector<Type> sargs;
  // Hanya bermakna untuk Param: nama parameter tipe (`T`).
  std::optional<std::string> pname;

  Type() = default; // Error (untuk container seperti unordered_map)

  static Type i64() { return Type{TypeKind::I64}; }
  static Type boolean() { return Type{TypeKind::Bool}; }
  static Type string() { return Type{TypeKind::String}; }
  static Type void_() { return Type{TypeKind::Void}; }
  static Type error() { return Type{TypeKind::Error}; }
  static Type func(std::vector<Type> params, Type ret) {
    Type t{TypeKind::Func};
    t.params = std::move(params);
    t.ret = std::make_shared<Type>(ret);
    return t;
  }
  static Type array(Type elem) {
    Type t{TypeKind::Array};
    t.elem = std::make_shared<Type>(elem);
    return t;
  }
  static Type named(std::string name) {
    Type t{TypeKind::Struct};
    t.sname = std::move(name);
    return t;
  }
  static Type applied(std::string name, std::vector<Type> args) {
    Type t{TypeKind::Struct};
    t.sname = std::move(name);
    t.sargs = std::move(args);
    return t;
  }
  static Type param(std::string name) {
    Type t{TypeKind::Param};
    t.pname = std::move(name);
    return t;
  }

  bool isError() const { return kind == TypeKind::Error; }
  std::string name() const;
  bool operator==(const Type &other) const;
  bool operator!=(const Type &other) const { return !(*this == other); }

private:
  explicit Type(TypeKind k) : kind(k) {}
};

} // namespace nusa
