# Cloud / Server Foundation — Design (Phase 78, target 2.29.0)

Status: implementasi slice ini. Mandat master: PHASE 78
(async I/O, networking, HTTP, TLS, logging, configuration,
metrics; framework-agnostic). Spec bahasa tak berubah.

## 1. Peta mandat → aktual

| Mandat | Status 2.29.0 |
|---|---|
| async I/O | ADA (`tasks.ns` Future/await/cancel + thread/sync) |
| networking | ADA (tcp + net.ns + e2e localhost) |
| HTTP | ADA (`http.ns` client + serve-once + e2e) |
| TLS integration | **baru** `packages/tls/` (shim OpenSSL client, verifikasipenuh, guard e2e) |
| logging | ADA (`logging.ns` Phase 75) |
| configuration | **baru** `std/config.ns` (env > TOML > fallback) |
| metrics | **baru** `std/metrics.ns` (struct eksplisit + render Prometheus) |

Batas arsitektur (didokumentasikan, bukan dipecahkan): handler
per-koneksi ber-fd butuh closure-menangkap sebagai nilai thread
— serve loop = accept+handle sekuensial (pola `http_serve_once`)
atau worker-pool channel; server TLS = klien dulu (serve-TLS
butuh terminasi sertifikat — follow-up).

## 2. `packages/tls/` (referensi, pola sqlite Phase 77)

- `shim.c`: tabel 16 slot `SSL*` (socket+SSL dibungkus);
  `ntls_dial(host, port, cafile)` — TCP + `SSL_connect` +
  `SSL_VERIFY_PEER` + cek hostname (`X509_check_host`);
  cafile `""` = path default sistem. `ntls_send/recv/close`.
  `recv` mengembalikan byte via FILE dump (pola sqlite:
  tanpa buffer lintas batas — `extern` tolak return string).
  Hmm — recv@Request-response: `ntls_request(host, port, cafile,
  request, path)` satu-shot (dial+kirim+terima-sampai-EOF+tutup)
  untuk pola HTTPS-GET; primitif terpisah untuk sesi.
- `tls.ns`: `extern fn` + alias `tls_dial/send/recv/close` +
  `tls_request`.
- `nusa.toml` + README + guard e2e (openssl CLI + py-ssl +
  libssl; CA+server-cert lokal; verifikasi negatif: cert
  salah → gagal).

## 3. `std/config.ns` (murni; `import toml` + `import process`
— preseden `encoding.ns` mengimpor `string`).

- `config_str(doc, section, key, env, fallback)`:
  env non-kosong menang, lalu TOML non-kosong, lalu fallback.
- `config_int/doc` (parse + valid? `parse_int_valid` dari...
  builtin global ✓): env valid-menang else TOML int.
- `config_bool`: env `"1"/"true"` (lowercase? eksak + "1";
  didokumentasikan) else TOML.

## 4. `std/metrics.ns` (murni; import time)

- `struct Metrics { requests, errors, bytes, started_ms }`.
- `metrics_new()`, `m_req/m_err/m_bytes` (value-semantics:
  kembalikan struct baru), `m_elapsed_ms(m)`,
  `render_prometheus(m, prefix)` (4 baris `# TYPE`+nilai).

## 5. Pengujian

- `tests/config_test.ns`, `tests/metrics_test.ns` (`nusa test`).
- `e2e_server.sh`: config golden + metrics golden +
  serve_once smoke + tls-paket guarded (dial+request lokal +
  verify-negatif) + negatif API.

## 6. Acceptance criteria

- `nusa test` +2 hijau; `e2e_server` hijau (atau SKIP-tls
  eksplisit); suite lama hijau; bench identik; bersih.
