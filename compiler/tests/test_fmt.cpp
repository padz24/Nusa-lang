// Unit test formatter v0.9.0: golden output + idempotensi.
#include "compiler/fmt/Formatter.h"
#include <iostream>

using namespace nusa;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int testGoldenBasic() {
  FormatResult r = formatSource(
      "t.ns", "fn   main(){print(   \"hi\" );let x=1+2*3;print(x);}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n"
                  "    print(\"hi\");\n"
                  "    let x = 1 + 2 * 3;\n"
                  "    print(x);\n"
                  "}\n");
  std::cout << "testGoldenBasic OK\n";
  return 0;
}

static int testGoldenControlStruct() {
  FormatResult r = formatSource(
      "t.ns",
      "struct P{name:string;age:i64;}\nfn f(c:bool)->i64{if c{return 1;}else{return 2;}}\n");
  CHECK(r.ok);
  CHECK(r.text == "struct P {\n"
                  "    name: string;\n"
                  "    age: i64;\n"
                  "}\n"
                  "fn f(c: bool) -> i64 {\n"
                  "    if c {\n"
                  "        return 1;\n"
                  "    } else {\n"
                  "        return 2;\n"
                  "    }\n"
                  "}\n");
  std::cout << "testGoldenControlStruct OK\n";
  return 0;
}

static int testCommentsKept() {
  FormatResult r = formatSource("t.ns", "// top\nfn main() {\n    print(1); // one\n}\n");
  CHECK(r.ok);
  CHECK(r.text == "// top\nfn main() {\n    print(1); // one\n}\n");
  std::cout << "testCommentsKept OK\n";
  return 0;
}

// Phase 57 (2.8.0): komentar dokumentasi `///` dipertahankan
// (regresi: formatter mendegradasi menjadi `//`, merusak `nusa doc`).
static int testDocCommentsKept() {
  FormatResult r = formatSource(
      "t.ns", "/// Add one.\nfn inc(x: i64) -> i64 {\n    return x + 1;\n}\nfn main() {\n    print(inc(1)); // trailing\n}\n");
  CHECK(r.ok);
  CHECK(r.text == "/// Add one.\nfn inc(x: i64) -> i64 {\n    return x + 1;\n}\nfn main() {\n    print(inc(1)); // trailing\n}\n");
  std::cout << "testDocCommentsKept OK\n";
  return 0;
}

static int testUnaryMinus() {
  FormatResult r =
      formatSource("t.ns", "fn main() {\n    let n = -5;\n    print(0 - -n);\n}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n"
                  "    let n = -5;\n"
                  "    print(0 - -n);\n"
                  "}\n");
  std::cout << "testUnaryMinus OK\n";
  return 0;
}

static int testStringEscape() {
  FormatResult r = formatSource(
      "t.ns", "fn main() {\n    print(\"a\\\"b\\\\c\");\n}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n    print(\"a\\\"b\\\\c\");\n}\n");
  std::cout << "testStringEscape OK\n";
  return 0;
}

static int testIdempotent() {  // Sampel acak dari tiap era sintaks: format 2x harus stabil.
  const char *samples[] = {
      "fn main() {\n    let numbers: [i64] = [1, 2, 3, 4];\n    print(numbers[0]);\n}\n",
      "import math;\nstruct P { x: i64; }\nfn f(a: [i64]) -> i64 {\n    let i = 0;\n    while i < len(a) {\n        print(a[i]);\n        i = i + 1;\n    }\n    return i;\n}\n",
      "fn main() { let p = Person { name: \"Nusa\", age: 1 }; print(p.age); }\n",
  };
  for (const char *s : samples) {
    FormatResult once = formatSource("t.ns", s);
    CHECK(once.ok);
    FormatResult twice = formatSource("t.ns", once.text);
    CHECK(twice.ok);
    if (once.text != twice.text) {
      std::cerr << "NOT IDEMPOTENT:\n--- once ---\n"
                << once.text << "--- twice ---\n"
                << twice.text;
      return 1;
    }
  }
  std::cout << "testIdempotent OK\n";
  return 0;
}

static int testLexError() {
  FormatResult r = formatSource("t.ns", "fn main() { @ }\n");
  CHECK(!r.ok);
  CHECK(!r.error.empty());
  std::cout << "testLexError OK\n";
  return 0;
}

// v1.1.0: spasi sebelum `[` literal array (`= [1]`, `: [i64]`),
// sementara `[` indexing tetap menempel (`a[0]`).
static int testArrayBracketSpacing() {
  FormatResult r = formatSource(
      "t.ns", "fn main(){let numbers:[i64]=[1,2];print(numbers[0]);}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n"
                  "    let numbers: [i64] = [1, 2];\n"
                  "    print(numbers[0]);\n"
                  "}\n");
  std::cout << "testArrayBracketSpacing OK\n";
  return 0;
}

// v1.2.0 (Phase 21a): deklarasi alias terformat wajar.
static int testTypeAliasFormatting() {
  FormatResult r = formatSource(
      "t.ns", "type Nums=[i64];\nfn main(){let a:Nums=[3,1];print(a[0]);}\n");
  CHECK(r.ok);
  CHECK(r.text == "type Nums = [i64];\n"
                  "fn main() {\n"
                  "    let a: Nums = [3, 1];\n"
                  "    print(a[0]);\n"
                  "}\n");
  std::cout << "testTypeAliasFormatting OK\n";
  return 0;
}

// v1.2.0 (Phase 21b): anotasi tipe fungsi terformat wajar.
static int testFnTypeFormatting() {
  FormatResult r = formatSource(
      "t.ns",
      "fn apply(f:fn(i64,i64)->i64,x:i64)->i64{return f(x,1);}\n"
      "fn main(){}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn apply(f: fn(i64, i64) -> i64, x: i64) -> i64 {\n"
                  "    return f(x, 1);\n"
                  "}\n"
                  "fn main() {\n}\n");
  std::cout << "testFnTypeFormatting OK\n";
  return 0;
}

// v1.2.0 (Phase 21c): bracket generik direkatkan.
static int testGenericFnFormatting() {
  FormatResult r = formatSource(
      "t.ns",
      "fn identity<T>(value:T)->T{return value;}\n"
      "fn first<T,U>(a:T,b:U)->T{return a;}\n"
      "fn main(){}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn identity<T>(value: T) -> T {\n"
                  "    return value;\n"
                  "}\n"
                  "fn first<T, U>(a: T, b: U) -> T {\n"
                  "    return a;\n"
                  "}\n"
                  "fn main() {\n}\n");
  std::cout << "testGenericFnFormatting OK\n";
  return 0;
}

// v1.2.0 (Phase 21d): aplikasi tipe direkatkan.
static int testGenericStructFormatting() {
  FormatResult r = formatSource(
      "t.ns",
      "struct Point<T>{x:T;y:T;}\n"
      "fn main(){let p:Point<i64> = Point{x:1,y:2};print(p.x);}\n");
  CHECK(r.ok);
  CHECK(r.text == "struct Point<T> {\n"
                  "    x: T;\n"
                  "    y: T;\n"
                  "}\n"
                  "fn main() {\n"
                  "    let p: Point<i64> = Point {\n"
                  "        x: 1, y: 2\n"
                  "    };\n"
                  "    print(p.x);\n"
                  "}\n");
  std::cout << "testGenericStructFormatting OK\n";
  return 0;
}

// v1.3.0 (Phase 22): trait/impl/method call terformat wajar.
static int testTraitFormatting() {
  FormatResult r = formatSource(
      "t.ns",
      "trait Printable{fn print(self);}\n"
      "struct Person{name:string;}\n"
      "impl Printable for Person{fn print(self){print(self.name);}}\n"
      "fn main(){let p=Person{name:\"N\"};p.print();}\n");
  CHECK(r.ok);
  CHECK(r.text == "trait Printable {\n"
                  "    fn print(self);\n"
                  "}\n"
                  "struct Person {\n"
                  "    name: string;\n"
                  "}\n"
                  "impl Printable for Person {\n"
                  "    fn print(self) {\n"
                  "        print(self.name);\n"
                  "    }\n"
                  "}\n"
                  "fn main() {\n"
                  "    let p = Person {\n"
                  "        name: \"N\"\n"
                  "    };\n"
                  "    p.print();\n"
                  "}\n");
  std::cout << "testTraitFormatting OK\n";
  return 0;
}

// v1.5.0 (Phase 24): `?` menempel.
static int testPropagateFormatting() {
  FormatResult r = formatSource(
      "t.ns", "fn main(){let x = f () ?;print(x ?);}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n"
                  "    let x = f()?;\n"
                  "    print(x?);\n"
                  "}\n");
  std::cout << "testPropagateFormatting OK\n";
  return 0;
}

// v1.8.0 (Phase 27): bar lambda menempel.
static int testLambdaFormatting() {
  FormatResult r = formatSource(
      "t.ns", "fn main() {\n    let add = |a: i64, b: i64| a + b;\n    let id = || 42;\n}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n"
                  "    let add = |a: i64, b: i64| a + b;\n"
                  "    let id = || 42;\n"
                  "}\n");
  std::cout << "testLambdaFormatting OK\n";
  return 0;
}

// Phase 50 (2.1.0): lambda sesudah `(` menempel tanpa spasi.
// Regresi: formatter lama mencetak `apply( |x| ...)` (spasi bogus).
static int testLambdaAfterParen() {
  FormatResult r = formatSource(
      "t.ns", "fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
              "    return f(x);\n"
              "}\n"
              "fn main() {\n"
              "    print(apply(|x| x + 1, 1));\n"
              "    print(apply(|x: i64| x * 2, 21));\n"
              "}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
                  "    return f(x);\n"
                  "}\n"
                  "fn main() {\n"
                  "    print(apply(|x| x + 1, 1));\n"
                  "    print(apply(|x: i64| x * 2, 21));\n"
                  "}\n");
  // `||` sesudah `(` juga menempel: `thread_spawn(|| ...)`.
  FormatResult r2 = formatSource(
      "t.ns", "fn main() {\n    let h = thread_spawn(|| 1);\n}\n");
  CHECK(r2.ok);
  CHECK(r2.text == "fn main() {\n    let h = thread_spawn(|| 1);\n}\n");
  std::cout << "testLambdaAfterParen OK\n";
  return 0;
}

// Phase 100 (3.1-foundation): `(` grouping sesudah operator/`=`/`return`
// diberi spasi (`a - (b)`, `= (a)`), cermin `[` literal (v1.1.0).
// Regresi: formatter lama mencetak `a -(b)` (spasi bogus hilang).
// Call (`f(`), call generik (`f<T>(`), dan unary (`-(`) tetap menempel.
static int testParenGroupingSpacing() {
  FormatResult r = formatSource(
      "t.ns",
      "fn id<T>(x:T)->T{return x;}\n"
      "fn main(){let a=10;let b=3;let r1=a-(a/b)*b;let r2=b*(a+1);let r3=a+(b*2);let x=(a+2)*3;let y=-(a+2);let z=id(a);print(r1+r2+r3+x+y+z);}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn id<T>(x: T) -> T {\n"
                  "    return x;\n"
                  "}\n"
                  "fn main() {\n"
                  "    let a = 10;\n"
                  "    let b = 3;\n"
                  "    let r1 = a - (a / b) * b;\n"
                  "    let r2 = b * (a + 1);\n"
                  "    let r3 = a + (b * 2);\n"
                  "    let x = (a + 2) * 3;\n"
                  "    let y = -(a + 2);\n"
                  "    let z = id(a);\n"
                  "    print(r1 + r2 + r3 + x + y + z);\n"
                  "}\n");
  std::cout << "testParenGroupingSpacing OK\n";
  return 0;
}

// Phase 125 (3.26.0): cleanup tak boleh menyentuh ISI string/comment.
// Regresi: `"  hi"` menjadi `" hi"` (spasi ganda di-collapse global).
static int testStringCommentKept() {
  FormatResult r = formatSource(
      "t.ns", "fn main() {\n    print(\"  hi  \"); //  jaga   spasi\n}\n");
  CHECK(r.ok);
  CHECK(r.text == "fn main() {\n"
                  "    print(\"  hi  \"); //  jaga   spasi\n"
                  "}\n");
  std::cout << "testStringCommentKept OK\n";
  return 0;
}

int main() {
  if (testGoldenBasic()) return 1;
  if (testGoldenControlStruct()) return 1;
  if (testCommentsKept()) return 1;
  if (testDocCommentsKept()) return 1;
  if (testUnaryMinus()) return 1;
  if (testStringEscape()) return 1;
  if (testIdempotent()) return 1;
  if (testLexError()) return 1;
  if (testArrayBracketSpacing()) return 1;
  if (testTypeAliasFormatting()) return 1;
  if (testFnTypeFormatting()) return 1;
  if (testGenericFnFormatting()) return 1;
  if (testGenericStructFormatting()) return 1;
  if (testTraitFormatting()) return 1;
  if (testPropagateFormatting()) return 1;
  if (testLambdaFormatting()) return 1;
  if (testLambdaAfterParen()) return 1;
  if (testParenGroupingSpacing()) return 1;
  if (testStringCommentKept()) return 1;
  std::cout << "ALL FMT TESTS PASSED\n";
  return 0;
}
