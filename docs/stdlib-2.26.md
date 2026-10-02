# Standard Library 3.0 — Design (Phase 75, target 2.26.0)

Status: implementasi slice ini. Mandat master: PHASE 75
(matang; konsisten, terdokumentasi, teruji, portabel;
platform-specific jelas). Spec bahasa tak berubah
(murni-Nusa; tanpa builtin baru).

## 1. Peta master → aktual

| Master | Aktual 3.0 | Catatan |
|---|---|---|
| std.core | **baru** `std/core.ns` | util int (di bawah) |
| std.io | `std/io.ns` + `read_line` | simetri `read_int` |
| std.fs | `std/fs.ns` + `write_file`, `read_file_or` | konvensi `getenv_or` |
| std.path | tetap | sudah matang (197 baris) |
| std.string | + starts/ends/trim/ascii-case | tanpa `ord` (lookup alfabet) |
| std.collections | tetap doc-only | butuh param array (blokir bahasa) |
| std.math | tetap | sudah matang |
| std.time/random | tetap | — |
| std.net/http | tetap | tercakup e2e localhost |
| std.process/thread/sync | tetap | — |
| std.async | = `std/tasks.ns` | Future/await/cancel (tanpa sintaks) |
| std.crypto | **DITUNDA** | butuh builtin `ord`/bitwise (preseden: `random.ns` "NOT cryptographic") |
| std.encoding | **baru** `std/encoding.ns` | codec hex↔i64 (tanpa `ord`, via lookup) |
| std.testing | **baru** `std/testing.ns` | asersi berlabel untuk `nusa test` |
| std.logging | **baru** `std/logging.ns` | prefix level (tanpa filter global — tanpa global) |
| std.option/result/error | tetap | — |

## 2. Konvensi konsistensi (dikunci 3.0, tanpa rename breaking)

- Prefix per modul bila rawan tabrakan (`str_`, `path_`,
  `http_`, `file_`, `rand_`, `task_`/`future_`, `opt_`,
  `res_`, `log_`, `hex_`, `test_`); nama pendek hanya inti
  matematika (`abs`, `min`, `sign`, ...).
- Pasangan validasi cermin `parse_int`/`parse_int_valid`:
  `hex_decode_i64` (invalid → 0) + `hex_valid`.
- Fallback cermin `getenv_or`: `read_file_or`.
- Setiap fn: doc `//` di atasnya (dipakai `nusa doc`);
  setiap modul: header layer + failure policy.

## 3. API baru (normatif-informal)

- core: `sign`, `abs_diff`, `ceil_div` (b>0), `mod_euclid`
  (b>0), `div_euclid` (b>0), `powmod` (exp≥0, mod>0),
  `ilog2` (≤0→0), `is_pow2`, `sum_to` (n≤0→0).
- testing: `test_true/false(cond, label)`,
  `test_eq_i64/eq_str/eq_bool(exp, got, label)`,
  `test_fail(label)` — sukses diam, gagal `die`-berlabel.
- logging: `log_debug/info/warn/error(msg)` — prefix
  `[DEBUG]`/`[INFO]`/`[WARN]`/`[ERROR]` ke stdout.
- encoding: `hex_encode_i64(x)` (lowercase, tanpa `0x`;
  negatif = komplemen-2 16 digit), `hex_decode_i64(s)`,
  `hex_valid(s)` (1..16 digit, decode-exact), `hex_valid_any(s)`
  (charset saja, panjang bebas — untuk kunci; v2.27.0),
  `is_hex_digit(c)`.
- string: `str_starts_with/ends_with`, `str_trim/trim_left/
  trim_right` (set `" \t\r\n"`), `str_to_ascii_lower/upper`.
- io: `read_line()`; fs: `write_file`, `read_file_or`.

## 4. Pengujian

- `tests/core_test.ns`, `tests/encoding_test.ns`,
  `tests/string2_test.ns`, `tests/testing_test.ns`,
  `tests/logging_test.ns` (via `nusa test`).
- `compiler/tests/e2e_stdlib3.sh`: modul baru + golden
  logging + jalur-gagal testing (rc!=0) + io/fs baru.

## 5. Acceptance criteria

- `nusa test tests/` hijau; `e2e_stdlib3` hijau; suite lama
  hijau (tanpa rename); `nusa doc` atas modul baru OK;
  bench identik; `.ns.ll` bersih.
