# Nusa Standard Library — v3.0.0

> Phase 40: dokumen ini = **Standard Library Specification**
> (kontrak API). Definisi bahasa normatif:
> `docs/language-spec-2.0.md` (§13 untuk daftar normatif;
> `docs/language-spec-1.0.md` diabadikan sebagai riwayat 1.20.0).

Standard library awal: kecil tetapi nyata dan teruji (stabil sejak v0.9.0). Terdiri dari
**builtin global** (diemisikan langsung ke libc/LLVM — tanpa `import`)
dan **modul `std/`** (di-`import`, di-resolve via direktori lokal →
`$NUSA_PATH` → `<exe>/../std`).

> Kebocoran memori: string hasil `input`/`str_concat`/`file_read`
> dialokasikan dan tak pernah dibebaskan. Ini disengaja
> (program-lifetime heap, lihat `docs/memory-model.md`) — garbage
> collector = RFC-0007 (Proposed, bukan janji).

## Builtin global (tanpa import)

### `len(a: [T]) -> i64`
Panjang array statis (konstanta kompilasi). Lihat v0.7.0.

### `input() -> string`
Baca satu baris dari stdin (newline akhir dibuang). EOF langsung
memberi `""`. Contoh:
```nusa
let name = input();
print(name);
```

### `assert(cond: bool)`
Lanjut bila `true`; bila `false`, cetak `assertion failed at F:L`
dan exit(1). Tulang punggung `nusa test`:
```nusa
assert(1 + 1 == 2);
```

### `str_len(s: string) -> i64`
Panjang string dalam byte (`strlen`).

### `str_concat(a: string, b: string) -> string`
Gabung dua string (hasil alokasi baru).

### `parse_int(s: string) -> i64`
Parse desimal (`strtoll`, basis 10). Berhenti di karakter non-digit;
non-numerik memberi 0. Contoh: `parse_int(input())`.

### `parse_int_valid(s: string) -> bool`
`true` bila seluruh input numerik penuh (konsisten `strtoll`:
spasi/sign OK, perlu ≥1 digit, sisa non-spasi ditolak). Fondasi
`parse_int_opt` (lihat `error-handling.md`).

### `file_read(path: string) -> string`
Seluruh isi file. File hilang/tak terbaca memberi `""` — cek
`file_exists` dulu bila penting.

### `file_write(path: string, content: string) -> bool`
Tulis (overwrite) file. `true` bila sukses.

### `file_exists(path: string) -> bool`
`true` bila file bisa dibuka untuk dibaca.

### `array_sum(a: [i64]) -> i64`, `array_min`, `array_max`
Agregasi inline atas array statis. Contoh:
```nusa
let total = array_sum(numbers);
```

### `sha256_hex(s: string) -> string` (v2.27.0)
Hex lowercase digest SHA-256 (FIPS 180-4; RFC-0011). Deterministik
total. Contoh: `sha256_hex("abc")` =
`ba7816bf...f20015ad` (64 char).

### `crypto_rand_hex(n: i64) -> string` (v2.27.0)
`2*n` hex dari `/dev/urandom`; `""` bila `n <= 0` atau sumber tak
terbaca (gagal eksplisit — SELALU cek panjang; lihat
`crypto_key_hex`). Tanpa fallback diam-diam.

### Kanonis `std.x` (v1.10.0)

`import std.time;` ≡ `import time;` (prefix `std.` dipetakan ke modul
datar; bentuk bertitik adalah kanonis, bentuk datar kompatibel).

Lapisan: **pure** (murni Nusa) vs **platform** (butuh builtin/runtime
C). Fungsionalitas Android-spesifik tidak ada — semua primitif
platform adalah libc/POSIX portabel (Bionic + glibc).

### `std/time.ns` — `import std.time;` (platform)

`now_ms() -> i64`, `now_s() -> i64`, `elapsed_ms(t0)`,
`elapsed_s(t0)` di atas `time_now_ms/s` (CLOCK_REALTIME).

### `std/random.ns` — `import std.random;` (platform + pure)

`rand_seed()` (entropi OS), `rand_next(state)`, `rand_abs`,
`rand_below(state, n)`, `rand_range(state, lo, hi)`, `rand_bool`
(LCG MMIX; bukan kriptografis; state eksplisit di-threading manual).

### `std/process.ns` — `import std.process;` (platform)

`prog_name()`, `args_count()`, `arg(i)`, `getenv_or(name, fallback)`,
`exit_with(code)` di atas `process_argv/argc/env/cwd/exit`.
Argumen CLI: `nusa run prog.ns a b` (diteruskan ke program).

### `std/path.ns` — `import std.path;` (pure)

`path_is_absolute/basename/dirname/ext/strip_ext/join/normalize`
(+ helper `path_step/push/pop`). Leksikal, separator `/`.

### `std/thread.ns` — `import std.thread;` (platform)

`spawn(f)`, `join(h)`, `par_map2(f, g, a, b)` (dua thread +
channel; deterministik), `run_send` (helper).

### `std/sync.ns` — `import std.sync;` (platform)

`lock_new/free`, `with_lock(m, f)`, `atomic_inc/dec` di atas
builtin mutex/atomic.

### `std/net.ns` — `import std.net;` (platform)

`dial(host, port) -> i64` (-1 = gagal), `send_str`, `recv_some`,
`recv_line` (murni Nusa), `close_conn`. TCP via getaddrinfo
(IPv4/IPv6/DNS). Server loopback 2.0: `listen_on(port)`,
`accept_one(srv)` (`tcp_listen`/`tcp_accept`; 127.0.0.1 by-design).

### `std/tasks.ns` — `import std.tasks;` (v2.8.0, murni Nusa)

`struct Future`, `task_spawn(f)`, `future_poll/await`,
`Cancel` + `cancel_new/cancel/is_cancelled/cancel_free`.
Lihat `docs/async-2.8.md`.

### `std/http.ns` — `import std.http;` (v2.9.0, murni Nusa)

`http_status/http_body` (parse), `http_get(host, port, path)`,
`http_serve_once(port, body)` (loopback). Butuh `import string;`
untuk helper string. Lihat `docs/net-2.9.md`.

### `std/actor.ns` — `import std.actor;` (v3.18.0, murni Nusa)

`struct ActorRef`, `actor_spawn(h, init)`, `actor_call(a, op, arg)`,
`actor_cast(a, op, arg)`. Handler `fn(op, arg, reply, state) ->
state` menjawab sendiri; triple-kirim atomik via mutex mailbox.
Aktor hidup selama proses; self-call sinkron = deadlock.
Lihat `docs/actors-3.18.md`.

### `std/trace.ns` — `import std.trace;` (v3.20.0, murni Nusa)

`struct Trace`, `new_request_id()`, `current_request_id()`
(env `TRACE_ID` atau bangkitan), `trace_begin(op)`,
`trace_end(t) -> ms`, `trace_event(op, status, note)` (satu baris
JSON per baris). Konsumen: `nusa trace run/check`.
Lihat `docs/dist-tooling-3.20.md`.

### `std/parallel.ns` — `import std.parallel;` (v3.22.0, murni Nusa)

`par_map_sum(n, w, f)` (jumlah `f(i)`, partisi block statik;
deterministik untuk `f` murni), `_par_block` (privat).
Tanpa work-stealing/SIMD (lihat `docs/parallel-3.22.md`).

### Concurrency (v1.9.0; bertag 2.0 — tanpa import)

`thread_spawn(f: fn() -> i64) -> i64`, `thread_join(h) -> i64`,
`mutex_new/lock/unlock/try_lock/free`, `chan_new/send/recv/free`
(`i64`), `chan_len(c) -> i64` (v2.7.0: panjang antre, non-blocking),
`chan_new_bounded(cap) -> i64` + `chan_try_send(c, v) -> bool`
(v3.19.0: backpressure),
`chan_new_bool/send_bool/recv_bool` (`bool`),
`chan_new_str/send_str/recv_str` (`string`; tag diverifikasi
runtime), `atomic_new/get/set/add/free` (`add` mengembalikan nilai
baru), `sleep_ms(ms)`. Lihat `docs/concurrency.md`.

## Modul `std/`

### `std/io.ns` — `import io;`
| Fungsi | Deskripsi |
|---|---|
| `read_int() -> i64` | `parse_int(input())`: baca angka dari stdin. |
| `read_line() -> string` | `input()`: baca satu baris (EOF → `""`). |

### `std/option.ns` — `import option;` (v2.0: enum natif)
| Fungsi | Deskripsi |
|---|---|
| `some(v)` | Konstruktor `Some`. |
| `is_some(o) / is_none(o)` | Predikat. |
| `opt_unwrap_or(o, d)` | Nilai atau fallback. |
| `opt_map(o, f)` | Petakan isi bila ada (tanpa dummy). |
| `parse_int_opt(s)` | `Option<i64>` penuh (`None` untuk invalid). |
| `file_read_opt(p)` | `Option<string>` (`None` bila hilang). |

### `std/error.ns` — `import error;` (v1.5.0)
| Item | Deskripsi |
|---|---|
| `struct Error` | `{ message: string }` — E standar. |
| `error(m)` | Konstruktor pesan. |

### `std/result.ns` — `import result;` (v2.0: enum natif)
| Fungsi | Deskripsi |
|---|---|
| `is_ok(r) / is_err(r)` | Predikat. |
| `res_unwrap_or(r, d)` | Nilai atau fallback. |
| `res_map(r, f)` | Petakan nilai bila Ok (tanpa dummy). |
| `res_map_err(r, f)` | Petakan error bila Err (tanpa dummy). |

Detail + operator `?`: `docs/error-handling.md`.

### `std/math.ns` — `import math;` (murni Nusa)
| Fungsi | Deskripsi |
|---|---|
| `abs(n)` | Nilai mutlak. |
| `min(a, b)` / `max(a, b)` | Minimum/maksimum. |
| `clamp(n, lo, hi)` | Jepit ke rentang inklusif. |
| `pow(base, exp)` | Pangkat (`exp >= 0`). |
| `fact(n)` | Faktorial (`fact(0) == 1`). |
| `gcd(a, b)` | FPB Euclid (input non-negatif). |
| `is_even(n)` | Ganjil/genap (negatif OK). |
| `isqrt(n)` | Akar kuadrat floor via Newton (`n <= 0` → 0). |

### `std/string.ns` — `import string;`
| Fungsi | Deskripsi |
|---|---|
| `str_repeat(s, n)` | Ulangi `s` sebanyak `n` kali. |
| `str_pad_left(s, width, pad)` | Pad kiri hingga panjang `width`. |
| `str_pad_right(s, width, pad)` | Pad kanan hingga panjang `width`. |
| `str_index_of(hay, needle, from)` | Indeks pertama `needle` dari `from` (-1 bila absen). |
| `str_contains(hay, sub)` | Predikat substring. |
| `str_starts_with(s, p)` / `str_ends_with(s, x)` | Predikat prefix/suffix. |
| `str_trim/trim_left/trim_right(s)` | Pangkas blank `" \t\r\n"`. |
| `str_is_blank(c)` | Predikat blank satu char. |
| `str_to_ascii_lower/upper(s)` | Peta kasus ASCII (byte lain lolos). |

Untuk lebar eksak, `pad` satu karakter (tanpa truncasi, pad
multi-karakter bisa overshoot).

### `std/collections.ns`
Panduan array: array `[T]` fixed-size tak bisa melewati batas fungsi
di v0.9.0 (tanpa parameter/return array), sehingga utilitas generik
tak bisa ditulis dalam Nusa sendiri — gunakan builtin `len`,
`array_sum`, `array_min`, `array_max` (contoh di badan modul).

### `std/fs.ns` — `import fs;`
| Fungsi | Deskripsi |
|---|---|
| `copy_file(src, dst) -> bool` | Salin file; `false` bila `src` hilang. |
| `file_size(path) -> i64` | Ukuran byte; hilang/tak terbaca → 0. |
| `write_file(path, content) -> bool` | Tulis (overwrite; lihat builtin). |
| `read_file_or(path, fb)` | Isi file atau fallback (cermin `getenv_or`). |

### `std/core.ns` — `import core;` (v2.26.0, murni Nusa)
| Fungsi | Deskripsi |
|---|---|
| `sign(n)` | -1, 0, atau 1. |
| `abs_diff(a, b)` | Selisih mutlak. |
| `ceil_div(a, b)` | Bagi bulat ke atas (`b > 0`; else 0). |
| `mod_euclid(a, b)` | Sisa Euclid `[0, b)` (`b > 0`; else 0). |
| `div_euclid(a, b)` | Hasil-bagi Euclid (pasangan sisa di atas). |
| `powmod(base, exp, m)` | Pangkat modular (`exp >= 0`, `m > 0`). |
| `ilog2(n)` | Floor log2 (`n <= 0` → 0). |
| `is_pow2(n)` | Predikat pangkat-dua. |
| `sum_to(n)` | 1+..+n (`n <= 0` → 0). |

### `std/testing.ns` — `import testing;` (v2.26.0, murni Nusa)
Asersi berlabel untuk file `nusa test`: sukses diam, gagal cetak
`FAIL <label>: ...` + abort exit(1).
| Fungsi | Deskripsi |
|---|---|
| `test_true/false(cond, label)` | Asersi boolean. |
| `test_eq_i64(exp, got, label)` | Samadengan + kedua nilai. |
| `test_eq_str(exp, got, label)` | Samadengan string + kedua nilai. |
| `test_eq_bool(exp, got, label)` | Samadengan boolean. |
| `test_fail(label)` | Gagal paksa berlabel. |

### `std/logging.ns` — `import logging;` (v2.26.0, murni Nusa)
Prefix level ke stdout; tanpa filter global (Nusa tanpa global
mutabel) — panggil dalam `if verbose` bila perlu.
| Fungsi | Deskripsi |
|---|---|
| `log_debug/info/warn/error(msg)` | Prefix `[DEBUG]`/`[INFO]`/`[WARN]`/`[ERROR]`. |

### `std/encoding.ns` — `import encoding;` (v2.26.0, murni Nusa)
Codec hex↔i64 (string arbitrer butuh builtin `ord` — di luar 3.0;
lihat `docs/stdlib-2.26.md`). Konvensi cermin
`parse_int`/`parse_int_valid`.
| Fungsi | Deskripsi |
|---|---|
| `hex_encode_i64(x)` | Hex lowercase tanpa `0x` (negatif = 16 digit two's complement). |
| `hex_decode_i64(s)` | Decode (invalid → 0). |
| `hex_valid(s)` | 1..16 digit hex. |
| `hex_valid_any(s)` | (v2.27.0) Charset hex, panjang bebas (untuk kunci). |
| `is_hex_digit(c)` / `hex_val(c)` | Predikat/nilai nibble (-1 bila invalid). |

### `std/crypto.ns` — `import crypto;` (v2.27.0, murni Nusa)
RFC-0011. Fail-closed di wrapper (primitif gagal-eksplisit).
| Fungsi | Deskripsi |
|---|---|
| `crypto_key_hex(n)` | Kunci hex `n` byte acak OS; panjang salah → die. |
| `crypto_compare(a, b)` | Tanpa early-exit (beda panjang → false). |
| `sha256_file(path)` | `sha256_hex(file_read(path))`. |

### `std/csv.ns` — `import csv;` (v2.28.0, murni Nusa)
CSV praktis gaya index/getter (OOB → `""`; newline akhir bukan
baris tambahan; `\r\n` ditoleransi).
| Fungsi | Deskripsi |
|---|---|
| `csv_num_rows(text)` / `csv_row(text, i)` | Baris logis (quote multi-baris = satu). |
| `csv_num_fields(row)` / `csv_field(row, i)` | Field ter-decode (`""` → `"`). |

### `std/json.ns` — `import json;` (v2.28.0, murni Nusa)
Objek datar top-level (nested/array diabaikan); hilang vs
tipe-salah tak dibedakan (lihat `json_has_key`).
| Fungsi | Deskripsi |
|---|---|
| `json_has_key(obj, key)` | Predikat keberadaan kunci. |
| `json_get_string(obj, key)` | Decode string atau `""`. |
| `json_get_int(obj, key)` | Prefix integer atau 0. |
| `json_get_bool(obj, key)` | `true` hanya untuk literal. |
| `json_valid(s)` | Kurung seimbang di luar string (cek dangkal). |

### `std/toml.ns` — `import toml;` (v2.28.0, murni Nusa)
Skalar baris `key = value` (`[[array]]` tak didukung).
| Fungsi | Deskripsi |
|---|---|
| `toml_get_string/int/bool(doc, section, key)` | Section `""` = top-level; komentar `#` diabaikan. |
| `toml_has_key(doc, section, key)` | (v2.29.0) Predikat keberadaan pasangan. |

### `std/config.ns` — `import config;` (v2.29.0, murni Nusa)
Env > TOML > fallback per kunci (`import toml` + `process`).
| Fungsi | Deskripsi |
|---|---|
| `config_str(doc, sec, key, env, fb)` | `$ENV` non-kosong, else TOML, else fallback. |
| `config_int(doc, sec, key, env, fb)` | `$ENV` valid, else TOML (0 eksplisit menang), else fallback. |
| `config_bool(doc, sec, key, env, fb)` | `$ENV` 1/true/0/false eksak, else TOML (false eksplisit menang), else fallback. |

### `std/metrics.ns` — `import metrics;` (v2.29.0, murni Nusa)
Counter eksplisit (tanpa global): tiap inc kembalikan struct baru.
| Item | Deskripsi |
|---|---|
| `struct Metrics` | `{ requests, errors, bytes, started_ms }`. |
| `metrics_new()` | Nol + cap waktu awal. |
| `m_req/m_err/m_bytes(m[, n])` | Counter bertambah (err bukan request). |
| `m_elapsed_ms(m)` | Milidetik sejak new. |
| `render_prometheus(m, prefix)` | Teks Prometheus (4 metrik + uptime). |

### `std/rpc.ns` — `import rpc;` (v2.30.0, murni Nusa)
JSON-RPC minimal atas TCP newline (string-only; blocking).
| Fungsi | Deskripsi |
|---|---|
| `rpc_request(id, method, params)` | Amplop request mentah. |
| `rpc_response(id, result)` / `rpc_error(id, msg)` | Amplop respons. |
| `rpc_method/params/id/result(x)` | Ekstrak amplop (params mentah utuh). |
| `rpc_call(host, port, method, params)` | Dial+kirim+baca+tutup (`""` = gagal). |
| `rpc_serve_once(port, handler)` | Accept+layan+tutup sekali (0 ok). |

### `std/discovery.ns` — `import discovery;` (v2.30.0, murni Nusa)
Daftar endpoint statis `"host:port"` (1/baris; IPv4/hostname).
| Fungsi | Deskripsi |
|---|---|
| `svc_count/svc_at(list[, i])` | Jumlah/pick modulo (`""` bila kosong). |
| `svc_host(ep)` / `svc_port(ep)` | Split endpoint (-1 bila port buruk). |
| `svc_round_robin(list, n)` | Pick ke-n (counter di pemanggil). |

### `std/timeout.ns` — `import timeout;` (v2.30.0, murni Nusa)
`with_timeout_ms(ms, op, fallback)`: task + poll 5ms; lewat =
fallback + worker ditinggalkan (tanpa preemptive kill).
i64-only (channel generik tak ada).

### `std/retry.ns` — `import retry;` (v2.30.0, murni Nusa)
| Item | Deskripsi |
|---|---|
| `struct RetryPolicy` | `{ attempts, base_ms, factor }`. |
| `retry_default()` | 3x, 50ms, x2. |
| `retry_with(p, op, is_err)` | Backoff hingga sukses; hasil terakhir. |

### `std/system.ns` — `import system;`
| Fungsi | Deskripsi |
|---|---|
| `die(msg: string)` | Cetak pesan lalu abort exit(1). |

## Resolusi modul

`import NAME;` dicari berurutan: direktori file pengimpor →
entri `$NUSA_PATH` (dipisah `:`) → `<direktori-exe>/../std`.
Lokal menang atas std (bisa override). `import string;` valid
(nama modul boleh sama dengan keyword tipe).
