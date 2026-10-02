# 08 — Package project

Tujuan: dari satu file ke proyek paket (`nusa.toml`, `nusa pkg`,
`nusa test`, `nusa new`, registry lokal).
Prasyarat: `tutorials/01-hello/` – `07-closures-concurrency/`.

```bash
# A. Jadikan tutorial ini paket path-deps minimal:
./build/nusa run tutorials/08-package-project/main.ns
# Hello, package!

# B. Scaffold proyek baru dari template (di /tmp agar repo bersih):
./build/nusa new hello-world demo --dir /tmp/nusa-tut08
./build/nusa run /tmp/nusa-tut08/demo/main.ns
# Hello, demo!

# C. Alur paket di dalam scaffold:
cd /tmp/nusa-tut08/demo
/tmp/nusa-tut08/../../build/nusa test
/tmp/nusa-tut08/../../build/nusa pkg build
```

Aturan: manifest `nusa.toml` (nama/versi/deps/compiler/target);
lock + SHA-256 untuk reproducible build; `pkg build` menolak lock
basi; registry = file-backed (`nusa search/install/publish
--registry DIR`, `$NUSA_REGISTRY`).

Lihat: `docs/pkg.md`, `docs/ecosystem.md`, `website/ecosystem.html`.
Latihan: `nusa new library mylib --dir /tmp/...` lalu `nusa test`.
