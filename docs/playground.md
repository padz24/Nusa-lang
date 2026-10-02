# Nusa Playground (Phase 42)

```
Browser
   ↓  (HTTP loopback 127.0.0.1)
Nusa Playground (`nusa playground`)
   ↓  (sumber → file sementara)
Compiler Service (compiler ini sendiri)
   ↓  (`Sandbox::runSandboxed`)
Sandbox (fork + batas + isolasi)
   ↓
Nusa Program
```

Contoh yang didukung master berjalan (`print("Hello, Nusa!")` →
`Hello, Nusa!` di browser maupun `curl`).

## Perintah

```
nusa sandbox <file.ns> [--timeout S] [--memory-mb M] [--output-kb K]
nusa playground [--port P] [--timeout S] [--memory-mb M]   # default 8080/5/256
```

- `sandbox`: verdict `ok | timeout | mem-limit | output-limit |
  signal | build-fail` (+ `code`) dicetak setelah output program.
- `playground`: `GET /` (halaman), `POST /api/run`
  (`{"source":"…"}` → `{"ok","verdict","output"}`). `--port 0` =
  ephemeral (port aktual dicetak — untuk test/paralel).

## Model keamanan (dan batas yang jujur)

Isolasi per run: proses terpisah (grup sendiri, `killpg` saat
kadaluarsa), cwd = tmpdir segar, stdin `/dev/null`, environment
dikosongkan total, stdout+stderr digabung + dibatasi, tanpa core
dump, CPU dibatasi (`RLIMIT_CPU`), supervisor membatasi TOTAL waktu
(run + budget build 120 dtk) sehingga tak ada fase yang gantung.

- **Memori via polling VmRSS (50ms) → `mem-limit`, BUKAN
  `RLIMIT_AS`**: diverifikasi empiris bahwa di Bionic, `RLIMIT_AS`
  < ~1GB membuat linker gagal memetakan CFI shadow saat startup
  (bahkan hello-world) — lalu prosesnya *hang* (bukan mati).
  OOM-killer kernel = backstop untuk spike antar-poll.
- **Tanpa namespace/jail**: `RLIMIT_NPROC` SENGAJA tak dipakai —
  di Android ia menghitung semua thread UID (satu sesi Termux)
  sehingga bisa membunuh runner yang sah. Fork-bomb hanya dibatasi
  wall-timeout (terbukti `verdict=timeout`, bukan isolasi penuh).
- **Jaringan tak diblokir** (butuh net-ns yang tak ada di Termux);
  server HANYA listen 127.0.0.1, tanpa auth (deployment publik =
  reverse-proxy + auth + TLS di depan, satu instans per pengguna).
- Request dibatasi (header 16KB, body/sumber 64KB); server
  sekuensial (satu run pada satu waktu — DoS bersamaan
  ditangani antrean TCP + timeout, bukan paralelisme).
- `import` lokal tak tersedia di playground (sumber = satu file);
  `import std.*` bisa (resolve via path exe, independen cwd).
  File sementara `/tmp/nusa-play-*.ns` dihapus usai run; workdir
  sandbox (`/tmp/nusa-sandbox-*`) dibersihkan best-effort.
