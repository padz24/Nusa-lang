# Getting Started — Nusa v2.0.0

## 1. Verifikasi environment (Termux, AArch64)

```bash
uname -m
# aarch64
llvm-config --version
# 21.1.8
clang --version
# clang version 21.1.8 ... Target: aarch64-unknown-linux-android24
cmake --version
```

## 2. Build compiler

```bash
cd ~/nusa
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## 3. Smoke test

```bash
./build/nusa --version
# nusa version 2.0.0 (LLVM 21.1.8, AArch64 Android/Termux)
/build/nusa --help
ctest --test-dir build --output-on-failure
# 100% tests passed, 50 tests passed out of 50
```

## 4. Program pertama (tetap jalan)

`examples/hello.ns`:

```nusa
fn main() {
    print("Hello, Nusa!");
}
```

Build + jalankan:

```bash
./build/nusa build examples/hello.ns -o /tmp/hello
/tmp/hello
# Hello, Nusa!

# atau sekaligus:
./build/nusa run examples/hello.ns

# profil build (v2.13.0): dev (O0, default), debug (O0+g), release (O2)
./build/nusa build examples/hello.ns --profile release -o /tmp/hello
# flag eksplisit (-O* / -g) menang atas profil
./build/nusa clean   # hapus artefak (*.ns.ll, .nusa-cache/, target/, dist/)
```

## 5. Variables & expressions (v0.3.0)

`examples/arithmetic.ns`:

```nusa
fn main() {
    let a = 10;
    let b = 20;
    let result = a + b;
    print(result);
    ...
}
```

```bash
./build/nusa run examples/arithmetic.ns
# 30
# 70
# true
```

Variabel, aritmetika `+ - * /`, perbandingan, boolean, dan `print`
semua tipe kini dieksekusi. Lihat `docs/syntax.md` untuk tabel precedence.

## 6. Static types (v0.4.0)

`examples/types.ns`:

```nusa
fn main() {
    let age: i64 = 15;
    let active: bool = true;
    let name: string = "Nusa";
    print(age);
    ...
}
```

```bash
./build/nusa run examples/types.ns
# 15
# true
# Nusa
# 30
```

Tanpa anotasi, tipe disimpulkan (`let x = 10` → `i64`). Salah tipe ditolak:

```bash
./build/nusa build bad.ns   # berisi: let x: i64 = "hello";
# bad.ns:2:5: error: mismatched types in 'let': 'x' is declared as 'i64' but initializer has type 'string'
```

## 7. Functions (v0.5.0)

`examples/functions.ns`:

```nusa
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}

fn main() {
    let result = add(10, 20);
    print(result);
}
```

```bash
./build/nusa run examples/functions.ns
# 30
```

Fungsi berparameter, return type `-> T`, `return`, nested calls, dan
fungsi void (`greet("Nusa");` sebagai statement) semuanya didukung.
Argumen salah jumlah/tipe ditolak dengan pesan jelas.

## 8. Control flow (v0.6.0)

`examples/control.ns`:

```nusa
fn main() {
    let x: i64 = 10;

    if x > 5 {
        print(x);
    } else {
        print(0);
    }

    let n: i64 = 3;
    while n > 0 {
        print(n);
        n = n - 1;
    }
}
```

```bash
./build/nusa run examples/control.ns
# 10
# 3
# 2
# 1
```

Kondisi harus `bool`; tiap Block scope baru (shadowing OK);
`n = n - 1` memutasi variabel yang sudah ada (tipe harus sama).

## 9. String & array (v0.7.0)

`examples/arrays.ns`:

```nusa
fn main() {
    let message: string = "Hello";
    print(message);

    let numbers: [i64] = [1, 2, 3, 4];
    print(numbers[0]);
    print(len(numbers));
}
```

```bash
./build/nusa run examples/arrays.ns
# Hello
# 1
# 4
# 30
```

Index di luar batas → `runtime error: index out of bounds`, exit 1
(bounds check selalu aktif, tanpa flag).

## 10. Struct & module (v0.8.0)

`examples/person.ns`:

```nusa
struct Person {
    name: string;
    age: i64;
}

fn main() {
    let person = Person {
        name: "Nusa",
        age: 1
    };
    print(person.name);
    print(person.age);
}
```

```bash
./build/nusa run examples/person.ns
# Nusa
# 1
```

Modul (`examples/main.ns` + `examples/math.ns`):

```bash
cd examples
../build/nusa run main.ns
# 6
# Ada
```

`import math;` memuat `math.ns` tetangga; `_`-awalan module-private;
siklus ditolak dengan rantai yang jelas.

## 11. Standard library & tooling (v0.9.0)

```bash
# Validasi cepat tanpa codegen:
./build/nusa check examples/hello.ns
# Checked examples/hello.ns: no errors

# Test ala proyek (tests/*_test.ns via assert):
./build/nusa test tests/
# PASS tests/math_test.ns
# 3 passed, 0 failed

# Format (idempoten; --check untuk CI):
./build/nusa fmt --check examples/hello.ns
./build/nusa fmt examples/hello.ns

# Dokumentasi API:
./build/nusa doc std/math.ns | head -n 12
```

Stdlib (`docs/standard-library.md`): 55 builtin + 18 modul `std/`
(`io math string collections fs system error option result time
random process path thread sync net tasks http`) + `input`/`assert`/`str_*`/
`parse_int`/`file_*`/`array_*`. Contoh ujung-ke-ujung:

```nusa
import io;
import math;

fn main() {
    print(fact(read_int()));
}
```

```bash
echo 5 | ./build/nusa run fact.ns
# 120
```

## 12. Lihat LLVM IR

```bash
./build/nusa build examples/hello.ns -o /tmp/hello
cat examples/hello.ns.ll
# harus memuat:
#   declare i32 @puts(ptr)
#   define i32 @main() { ... ret i32 0 }

# Untuk fungsi:
./build/nusa build examples/functions.ns -o /tmp/fns
cat examples/functions.ns.ll
# harus memuat:
#   define i64 @add(i64 %a, i64 %b)
#   call i64 @add(i64 10, i64 20)

# Untuk array:
./build/nusa build examples/arrays.ns -o /tmp/arr
cat examples/arrays.ns.ll
# harus memuat:
#   alloca [4 x i64]
#   declare void @exit(i32)

# Untuk struct:
./build/nusa build examples/person.ns -o /tmp/person
cat examples/person.ns.ll
# harus memuat:
#   %Person = type { ptr, i64 }
```

## 13. Kalau error

Contoh salah:

```nusa
fn main() {
    let x = 10 + ;
}
```

```bash
./build/nusa build bad.ns
# bad.ns:2:18: error: expected expression
#         let x = 10 + ;
#                          ^
# help: expression: number, string, true/false, function call, [1, 2], variable name, or '(...)'
```

Compiler tidak crash — ia melaporkan `file:line:col` + snippet + saran.
