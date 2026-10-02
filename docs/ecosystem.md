# Ekosistem Nusa (Phase 43)

Inventaris resmi komponen ekosistem (mandat master §28) — apa yang
ada di repo, di mana, dan statusnya (12/12 ✅ sejak Phase 43
lanjutan; riwayat penundaan website dicatat di CHANGELOG).

| Komponen master | Status | Lokasi |
|---|---|---|
| Compiler | ✅ 2.0.0 | `compiler/`, `build/nusa` |
| Standard Library | ✅ builtin + 16 modul | `std/`, `docs/standard-library.md` |
| Package Registry | ✅ file-backed | `docs/pkg.md`, `nusa search/install/publish` |
| Language Server | ✅ 7 fitur | `docs/lsp.md`, `nusa lsp` |
| Formatter | ✅ | `nusa fmt`, `docs/` |
| Documentation | ✅ 30+ dokumen + spec 2.0 | `docs/` |
| Examples | ✅ v1.0 + 7 contoh 2.0 | `examples/` (+ `website/examples.html`) |
| Tutorials | ✅ jalur 01–08 | `tutorials/` (+ `website/tutorials.html`) |
| RFC repository | ✅ proses + 8 RFC | `rfcs/` |
| Playground | ✅ sandbox + HTTP loopback | `docs/playground.md` |
| Project templates | ✅ 4 template | `nusa new`, §Template di bawah |
| Website | ✅ statis offline-first | `website/` (§Website di bawah) |

## Template (`nusa new`)

```
nusa new <template> [name] [--dir DIR]   # name default = template
nusa new --list
```

| Template | Isi | Verifikasi |
|---|---|---|
| hello-world | main.ns + nusa.toml + README | run → `Hello, <name>!` |
| cli-app | main.ns (argv/usage) + nusa.toml + README | run ± arg |
| library | calc.ns + calc_test.ns + nusa.toml + README | `nusa test` |
| server | handler.ns + main.ns + server_test.ns + nusa.toml + README | serve-once + `nusa test` |

Template server = serve-once loopback nyata (`tcp_listen`/
`tcp_accept` sejak 2.0): handler murni teruji + accept satu
koneksi lalu keluar (loop abadi menunggu executor RFC-0004).
Semua file scaffold lolos `nusa fmt --check` (nol baris kosong,
konvensi repo).
Direktori target tak-kosong DITOLAK (anti-menimpa); nama mengikuti
aturan paket (`[A-Za-z_][A-Za-z0-9_]*`); `compiler =` minimum diisi
versi compiler yang men-scaffold.

## Website (statis, offline-first)

`website/` = situs statis resmi (HTML + satu CSS, tanpa JavaScript
eksternal, tanpa CDN, tanpa tracking, tanpa build step): `index.html`
(beranda + quickstart), `ecosystem.html` (cermin tabel di atas),
`tutorials.html` (index jalur 01–08), `examples.html` (index contoh
runnable), `README.md` (cara preview + publish).

```bash
python3 -m http.server 8000   # dari root repo
# buka http://127.0.0.1:8000/website/
```

Tautan `../docs|tutorials|examples|rfcs/` relatif — bekerja saat
di-serve dari root maupun saat folder-folder itu ikut dipublish.
Konvensi penerbitan: `docs/foo.md` → `docs/foo.html` (render Markdown
→ HTML dengan nama yang sama; `.md` tetap sumber kebenaran).
Kontrak offline (tanpa referensi http/https/CDN) dikunci
`e2e_ecosystem.sh`.

## Tutorials (`tutorials/01–08`)

Jalur belajar berurutan, tiap modul `README.md` + `main.ns` runnable
(`nusa run` + `nusa fmt --check` dikunci `e2e_ecosystem.sh`):
01 hello, 02 variables, 03 functions/struct, 04 generics+traits,
05 match+enum, 06 error handling (Option/Result 2.0 + `?`),
07 closures+concurrency, 08 package project (`nusa pkg`/`new`/`test`).

## Examples 2.0 (`examples/`)

Basis v1.0 dipertahankan; tambahan runnable: `generics.ns`
(generic fn+struct), `traits.ns` (trait/impl), `match.ns` (match),
`enums.ns` (enum payload + generic), `closures.ns` (lambda),
`error_option_result.ns` (Option/Result enum + `?`),
`concurrency.ns` (thread + channel i64).
