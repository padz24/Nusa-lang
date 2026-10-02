# Plugin System — Milestone 1: Kontrak v1 Tersandbox-Proses (Phase 112)

Status: slice implementasi (target 3.13.0). Mandat master: arsitektur
plugin (compiler/tooling/aplikasi), UTAMAKAN sandboxed WASM/WASI
daripada eksekusi natif arbitrer.

## 1. Keputusan jujur: sandbox-proses dulu, WASM kemudian

Backend WASM (engine + wasi-sysroot) BELUM ADA di CI (fakta Phase
111) → plugin WASM tak bisa diuji → DITUNDA dengan kriteria terima.
Sebagai gantinya milestone ini mengirim plugin yang BENAR-BENAR
tersandbox hari ini: proses terisolasi + batas timeout/mem/output
(menunggangi `sandbox::runSandboxedInput`, sekeluarga `nusa
sandbox`/`playground` yang diaudit Phase 87). WASM adalah jalur
migrasi (kontrak v1 dirancang portabel: request-response JSON
murni, tanpa syscall asumsi), BUKAN klaim hari ini.

## 2. Kontrak `nusa-plugin/1`

- Unit: program `.ns` + (opsional) manifest JSON
  `{"name","version","ops":[...]}`.
- CLI: `echo '{"op":"add","a":1}' | nusa plugin run p.ns
  [--manifest m.json] [--timeout S] [--memory-mb M] [--output-kb K]`
  (default = default sandbox; `<=0` ditolak cermin S1).
- Protokol: stdin = SATU objek JSON (`"op"` string wajib);
  stdout = SATU objek JSON (`"ok":bool, ...`). Contoh rujukan:
  plugin `calc` di `e2e_plugin.sh` (murni Nusa: `input` +
  `std/json` + `str_concat`).
- Gate: op ∉ manifest.ops → `verdict=refused` TANPA eksekusi
  (pra-eksekusi, tanpa proses anak). Tanpa manifest = gate
  nonaktif (terdokumentasi; discovery masa depan).
- Hasil: respons ke stdout; `[plugin: verdict=... code=...]` ke
  stderr; rc = exit program bila ok, 1 bila verdict lain/
  penolakan/manifest rusak/request kosong.

## 3. Model keamanan (eksplisit, cermin playground)

| Jaminan | Status |
|---|---|
| Timeout/mem/output-cap + kill | ADA (verdict teruji) |
| chdir terisolasi + env scrub + stdin terkendali | ADA |
| Tanpa namespace/user-isolation (batas Termux) | DIAKUI (sama dgn playground) |
| Blokir jaringan/filesystem | TIDAK (plugin bisa memakai net/fs host — CAPABILITY masa depan via manifest + WASI) |
| Eksekusi natif arbitrer di luar sandbox | DITOLAK (satu-satunya jalan = via batas ini) |

Plugin compiler/tooling (makro, linter) = follow-up di atas
kontrak yang sama; aplikasi plugin = kasus utama hari ini.

## 4. Diverifikasi

- Unit `test_plugin` 4/4 (manifest, penolakan, op, refusal
  pra-eksekusi); `e2e_plugin` 5/5 (roundtrip 42, refusal,
  tanpa-manifest, timeout, build-fail/kosong/manifest-hilang).
- `e2e_playground` + `e2e_sec` hijau (jalur sandbox lama utuh);
  `e2e_release` hijau; `.ns` + IR OK.

## 5. Ditunda jujur

- Backend WASM/WASI (terima = `e2e_plugin` varian engine hijau).
- Capability manifest enforceable (fs/net/process) + `inspect`/
  discovery + streaming multi-turn + signing plugin.
