#pragma once
#include "compiler/lexer/Token.h"
#include "compiler/types/Type.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nusa {

// AST v0.5.0 — functions; v0.6.0 — control flow; v0.8.0 — struct & import:
//
//   Program    := (Import | StructDecl | FuncDecl)*
//   Import     := "import" Ident [";"]   (modul = file .ns tetangga)
//   StructDecl := "struct" Ident "{" (FieldDecl [";"])* "}"
//   FieldDecl  := Ident ":" Type
//   FuncDecl   := "fn" Ident "(" [Param ("," Param)*] ")" ["->" Type] Block
//   Param      := Ident ":" Type
//   Block      := "{" Stmt* "}"
//   Stmt       := LetStmt | PrintStmt | ReturnStmt | ExprStmt | IfStmt
//              | WhileStmt | AssignStmt
//   LetStmt    := "let" Ident [":" Type] "=" Expr [";"]
//   AssignStmt := Ident "=" Expr [";"]   (v0.6.0: mutasi untuk loop;
//              variabel harus sudah dideklarasikan; tipe harus sama)
//   PrintStmt  := "print" "(" Expr ")" [";"]
//   ReturnStmt := "return" [Expr] [";"]
//   ExprStmt   := Call [";"]   (panggilan void sebagai statement)
//   IfStmt     := "if" Expr Block ["else" (Block | IfStmt)]
//   WhileStmt  := "while" Expr Block
//
//   Expr (precedence, tertinggi terakhir):
//     equality   := comparison (("==" | "!=") comparison)*
//     comparison := term (("<" | ">" | "<=" | ">=") term)*
//     term       := factor (("+" | "-") factor)*
//     factor     := unary (("*" | "/") unary)*
//     unary      := ("-" | "+") unary | primary
//   primary    := IntLit | StringLit | "true" | "false" | Call | Ident
//                 | StructLit | ArrayLit | "(" Expr ")"
//   StructLit  := Ident "{" [FieldInit ("," FieldInit)*] "}"
//   FieldInit  := Ident ":" Expr
//   ArrayLit   := "[" [Expr ("," Expr)*] "]"
//   Call       := Ident "(" [Expr ("," Expr)*] ")"
//   Postfix    := ("[" Expr "]" | "." Ident)*   (index & field, berantai)
//
// Aturan bentuk (dicek semantic/typecheck, bukan parser):
//  - `main`: tanpa parameter, return void (absen atau `-> void`).
//  - `return` harus menjadi statement terakhir body (v0.5.0: bodies
//    straight-line; generalisasi menyusul di v0.6.0 control flow).

enum class BinOp { Add, Sub, Mul, Div, Eq, NotEq, Lt, Gt, LtEq, GtEq };
enum class UnOp { Neg, Pos };

struct Block; // didefinisikan di bawah; If/While memegang unique_ptr

struct Expr {
  struct LiteralInt {
    int64_t value = 0;
    Location loc;
  };
  struct LiteralBool {
    bool value = false;
    Location loc;
  };
  struct LiteralString {
    std::string value;
    Location loc;
  };
  struct VarRef {
    std::string name;
    Location loc;
  };
  struct Call {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
    Location loc; // lokasi nama fungsi
  };
  // Panggilan method (Phase 22): `p.print(x)`. Checker menulis ulang
  // menjadi Call biasa ke nama mangled (dispatch statis).
  struct MethodCall {
    std::unique_ptr<Expr> receiver;
    std::string method;
    std::vector<std::unique_ptr<Expr>> args;
    Location loc; // lokasi nama method
    // Targs eksplisit konstruktor enum `E<T>.V()` (Phase 26);
    // kosong = inferensi (atau method biasa yang mengabaikannya).
    std::vector<Type> targs;
  };
  // Propagasi error (Phase 24): `expr?` — unwrap Ok atau return Err.
  struct Propagate {
    std::unique_ptr<Expr> expr;
    Location loc; // lokasi `?`
    // Anotasi checker untuk enum Result natif (Phase 45/B1):
    // tag varian Ok + nama slot payload Ok/Err. okTag < 0 = encoding
    // struct lama (is_ok/value) — dipertahankan untuk struct `Result`
    // definisi-pengguna yang meniru bentuk std v1.x.
    int okTag = -1;
    std::string okSlot;
    std::string errSlot;
  };
  // Array literal `[1, 2, 3]` (v0.7.0). Elemen homogen; dicek checker.
  struct ArrayLit {
    std::vector<std::unique_ptr<Expr>> elems;
    Location loc; // lokasi `[`
  };
  // Index `base[idx]` (v0.7.0). Base umumnya variabel array.
  struct Index {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;
    Location loc; // lokasi `[`
  };
  // Konstruksi struct `Person { name: ..., age: ... }` (v0.8.0).
  struct StructLit {
    std::string name; // nama struct (mangled usai instansiasi generik)
    // Argumen tipe eksplisit (Phase 21d): `Point<i64> { ... }`.
    // Kosong = inferensi dari nilai field.
    std::vector<Type> targs;
    struct FieldInit {
      std::string name;
      std::unique_ptr<Expr> value;
      Location loc; // lokasi nama field
    };
    std::vector<FieldInit> fields;
    Location loc; // lokasi nama struct
    // Penanda internal (Phase 26): literal hasil rewrite konstruktor
    // enum — dilewati blokir-tempa checker; tak di-dump/di-parse.
    bool enumCtor = false;
    // Tipe applied enum asal rewrite (Phase 45/B1): check ulang
    // (desugar match memeriksa ulang body arm) WAJIB menghasilkan
    // tipe applied yang sama, bukan `named(mangled)`. Error = tak-diisi.
    Type enumType = Type::error();
  };
  // Akses field `person.age` (v0.8.0; berantai: `a.b.c`, `a.b[0]`).
  struct FieldAccess {
    std::unique_ptr<Expr> base;
    std::string field;
    Location loc; // lokasi `.`
  };
  struct Unary {
    UnOp op;
    std::unique_ptr<Expr> operand;
    Location loc;
  };
  struct Binary {
    BinOp op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    Location loc;
  };
  // Lambda `|x: i64| x + 1` (Phase 27, v1.8.0): di-lifting checker
  // menjadi fungsi hidden + struct env; tak pernah mencapai codegen.
  struct Lambda {
    struct LParam {
      std::string name;
      std::optional<Type> annot; // `|x: i64|`; nullopt = inferensi konteks
      Location loc; // lokasi nama parameter
    };
    std::vector<LParam> params;
    std::unique_ptr<Block> body;
    Location loc; // lokasi `|` pembuka
    // Diisi checker via propagasi konteks (call/let/return): tipe
    // parameter + return harapan. Kosong = belum terinferensi.
    std::vector<Type> expectParams;
    Type expectRet = Type::error();
    bool expected = false;
    bool bad = false; // arity/annotation gagal (sudah dilaporkan)
  };
  // Closure hasil lift (Phase 27): diisi checker, dibaca codegen
  // (malloc env + pair, panggil via trampolin). Tak ditulis parser.
  struct Closure {
    std::string lifted;   // nama fungsi hidden (`$lambda_N`)
    std::string envStruct; // nama hidden struct env (`$env_N`)
    std::vector<std::unique_ptr<Expr>> captures; // VarRef nilai tangkapan
    Type funcType; // signature fn (untuk tipe trampolin)
    Location loc; // lokasi lambda asal
  };
  using Node = std::variant<LiteralInt, LiteralBool, LiteralString, VarRef,
                            Call, MethodCall, Propagate, ArrayLit, Index,
                            StructLit, FieldAccess, Unary, Binary, Lambda,
                            Closure>;

  Node node;

  const Location &loc() const;
  // Representasi s-expr untuk test & debugging, mis. (+ 10 (* 20 3)).
  std::string dump() const;
};

// Pola match (Phase 25, v1.6.0). `Variant` menyelesaikan type-directed:
// `Some`/`None`/`Ok`/`Err` untuk std Option/Result, varian enum (26)
// untuk tipe enum; sisanya invalid.
struct Pat {
  struct Wild {
    Location loc;
  };
  struct Bind {
    std::string name;
    Location loc;
  };
  struct LitInt {
    int64_t value = 0;
    Location loc;
  };
  struct LitBool {
    bool value = false;
    Location loc;
  };
  struct LitStr {
    std::string value;
    Location loc;
  };
  struct Struct {
    std::string name;
    std::vector<Type> targs; // eksplisit (`P<i64> {...}`); kosong = inferensi
    struct Field {
      std::string name;
      std::unique_ptr<Pat> pat;
      Location loc;
    };
    std::vector<Field> fields;
    Location loc;
  };
  struct Variant {
    std::string name; // `Some`, `Ok`, atau varian enum
    std::vector<std::unique_ptr<Pat>> args;
    Location loc;
    // Indeks varian enum pengguna (Phase 26): diisi checker saat
    // validasi (checkEnumVariantPattern); -1 = std/tak-terresolve.
    int tag = -1;
  };
  using Node =
      std::variant<Wild, Bind, LitInt, LitBool, LitStr, Struct, Variant>;
  Node node;
  std::string dump() const;
};

struct LetStmt {
  std::string name;
  Location loc; // lokasi keyword `let`
  std::optional<Type> declaredType; // `let x: i64 = ...` (v0.4.0)
  std::unique_ptr<Expr> value;
  std::string dump() const;
};

struct PrintStmt {
  std::unique_ptr<Expr> arg;
  Location loc; // lokasi keyword `print`
  std::string dump() const;
};

struct ReturnStmt {
  std::unique_ptr<Expr> value; // nullopt = `return;` (fungsi void)
  Location loc; // lokasi keyword `return`
  std::string dump() const;
};

// Statement ekspresi (v0.5.0): panggilan fungsi void sebagai statement,
// mis. `greet("Nusa");`. Parser hanya memproduksi Call di sini.
struct ExprStmt {
  std::unique_ptr<Expr> expr;
  Location loc;
  std::string dump() const;
};

struct IfStmt {
  std::unique_ptr<Expr> cond;
  // unique_ptr: memutus siklus definisi IfStmt ↔ Block.
  std::unique_ptr<Block> thenBlock;
  // else polos maupun `else if` (disimpan sebagai Block satu-statement).
  // null = tanpa else.
  std::unique_ptr<Block> elseBlock;
  Location loc; // lokasi keyword `if`
  std::string dump() const;
};

struct WhileStmt {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Block> body;
  Location loc; // lokasi keyword `while`
  std::string dump() const;
};

struct AssignStmt {
  std::string name;
  std::unique_ptr<Expr> value;
  Location loc; // lokasi nama variabel
  std::string dump() const;
};

struct MatchArm {
  Pat pat;
  // Guard opsional (`n if n > 0 => ...`); null = tanpa guard.
  std::unique_ptr<Expr> guard;
  // unique_ptr: sama seperti IfStmt/WhileStmt (Block didefinisikan
  // di bawah; single-stmt dinormalisasi parser menjadi Block).
  std::unique_ptr<Block> body;
  Location loc; // lokasi pola
};

struct MatchStmt {
  std::unique_ptr<Expr> subject;
  std::vector<MatchArm> arms;
  Location loc; // lokasi keyword `match`
  std::string dump() const;
};

using Stmt = std::variant<LetStmt, PrintStmt, ReturnStmt, ExprStmt, IfStmt,
                          WhileStmt, AssignStmt, MatchStmt>;

struct Block {
  std::vector<Stmt> stmts;
  std::string dump() const;
};

struct Param {
  std::string name;
  Type type;
  Location loc; // lokasi nama parameter
  std::string dump() const;
};

struct StructField {
  std::string name;
  Type type;
  Location loc; // lokasi nama field
  std::string dump() const;
};

struct StructDecl {
  std::string name;
  Location loc; // lokasi keyword `struct`
  // Parameter tipe generik (Phase 21d, v1.2.0): `struct P<T> { ... }`.
  std::vector<std::string> tparams;
  // Batasan trait per tparam (Phase 22): `struct B<T: Printable>`.
  std::unordered_map<std::string, std::vector<std::string>> tparamBounds;
  std::vector<StructField> fields;
  // Hidden struct hasil desugar enum (Phase 26): tak ditulis user;
  // codegen mentoleransi field hilang (slot non-aktif = undef),
  // DocGen melewatinya (dokumen via `enums`, bukan `structs`).
  bool hiddenEnum = false;
  // Struct env closure (Phase 27): field fn diizinkan (function
  // pointer). Tak ditulis user; dibuat checker saat lift lambda.
  bool closureEnv = false;
  std::string dump() const;
};

struct Import {
  std::string module; // `math` → `math.ns` tetangga
  Location loc; // lokasi keyword `import`
  std::string dump() const;
};

// Varian enum (Phase 26, v1.7.0): payload POSISIONAL ala MASTER
// (`Ok(T)`, bukan `Ok(value: T)`); kosong = varian unit (`Red`).
struct EnumVariant {
  std::string name;
  std::vector<Type> payload;
  Location loc; // lokasi nama varian
  std::string dump() const;
};

// Deklarasi enum (Phase 26): direpresentasikan checker sebagai hidden
// struct (`tag` + field payload per varian); lihat docs/enums.md.
struct EnumDecl {
  std::string name;
  Location loc; // lokasi keyword `enum`
  // Parameter tipe generik: `enum Result<T, E> { ... }` (cermin struct).
  std::vector<std::string> tparams;
  // Batasan trait per tparam (cermin struct).
  std::unordered_map<std::string, std::vector<std::string>> tparamBounds;
  std::vector<EnumVariant> variants;
  std::string dump() const;
};

// Alias tipe (Phase 21a, v1.2.0): `type Nums = [i64];` — transparan,
// dikanonikalisasi typechecker ke tipe target sebelum codegen.
struct TypeAliasDecl {
  std::string name;
  Type target;  // mentah saat parse; kanonis sesudah check lolos
  Location loc; // lokasi keyword `type`
  std::string dump() const;
};

// Konstanta waktu-kompilasi (Phase 70, v2.21.0, RFC-0009 slice 1):
// `const NAME [: T] = <expr-konstan>;` — dievaluasi + disubstitusi
// typechecker (desugar); codegen tak melihatnya.
struct ConstDecl {
  std::string name;
  Location loc; // lokasi keyword `const`
  std::optional<Type> declaredType;
  std::unique_ptr<Expr> value;
  std::string dump() const;
};

struct FunctionDecl {
  std::string name;
  Location loc; // lokasi keyword `fn`
  // Deklarasi eksternal C (Phase 32, v1.13.0): `extern fn puts(...);`
  // tanpa body; signature dibatasi (i64/string/bool/void); definisi
  // di-link dari library native.
  bool isExtern = false;
  // Parameter tipe generik (Phase 21c, v1.2.0): `fn f<T, U>(...)`.
  // Kosong = fungsi biasa. `main` tak boleh generik (ditolak parser).
  std::vector<std::string> tparams;
  // Batasan trait per tparam (Phase 22, v1.3.0): `fn f<T: Printable>`.
  // Hanya berisi entri bertatasan; lookup per nama.
  std::unordered_map<std::string, std::vector<std::string>> tparamBounds;
  std::vector<Param> params;
  Type retType; // void bila `-> T` absen
  Block body;
  std::string dump() const;
};

// Deklarasi trait (Phase 22, v1.3.0): metode tanpa body.
// (Setelah FunctionDecl karena memegang metode.)
struct TraitDecl {
  std::string name;
  Location loc; // lokasi keyword `trait`
  std::vector<FunctionDecl> methods;
  std::string dump() const;
};

// Blok implementasi (Phase 22): `impl Trait for Type { ... }`.
struct ImplBlock {
  std::string trait;
  Location traitLoc; // lokasi nama trait
  Type target;
  Location loc; // lokasi keyword `impl`
  std::vector<FunctionDecl> methods;
  std::string dump() const;
};

struct Program {
  std::string file; // file sumber (untuk diagnostic program-level)
  std::vector<Import> imports;       // v0.8.0
  std::vector<StructDecl> structs;   // v0.8.0
  std::vector<TypeAliasDecl> aliases; // Phase 21a (v1.2.0)
  std::vector<TraitDecl> traits;     // Phase 22 (v1.3.0)
  std::vector<ImplBlock> impls;      // Phase 22 (v1.3.0)
  std::vector<EnumDecl> enums;       // Phase 26 (v1.7.0)
  std::vector<ConstDecl> consts;     // Phase 70 (v2.21.0)
  std::vector<FunctionDecl> functions;
  std::string dump() const;
  // Index `fn main` bila ada.
  std::optional<size_t> mainIndex() const;
  bool hasMain() const { return mainIndex().has_value(); }
};

std::string binOpName(BinOp op);

// Deep-clone subtree (Phase 21c): monomorphisasi menginstansiasi body
// template per kombinasi tipe argumen; template asli dipertahankan
// untuk instansiasi lain. Semua lokasi dipertahankan (diagnostic
// instance menunjuk baris template).
std::unique_ptr<Expr> cloneExpr(const Expr &e);
Stmt cloneStmt(const Stmt &s);
Block cloneBlock(const Block &b);
std::unique_ptr<Pat> clonePat(const Pat &p);

} // namespace nusa
