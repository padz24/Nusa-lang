# 01 — Hello, Nusa

Prasyarat: compiler sudah di-build (`cmake --build build`).
Tujuan: program pertama + tiga perintah dasar.

```bash
./build/nusa run tutorials/01-hello/main.ns
# Hello, Nusa!

./build/nusa check tutorials/01-hello/main.ns
# Checked ...: no errors

./build/nusa build tutorials/01-hello/main.ns -o /tmp/hello01
/tmp/hello01
# Hello, Nusa!
```

Lihat: `docs/getting-started.md` (§4), `docs/syntax.md`.
Lanjut: `tutorials/02-variables/`.
