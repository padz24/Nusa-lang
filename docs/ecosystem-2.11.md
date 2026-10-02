# Package Ecosystem 2.0 — Design (Phase 60, target 2.11.0)

Status: audit → Implementation (milestone 1: 4 perintah read-only).
Spec bahasa tak berubah; tanpa RFC (tooling).

## 1. Status daftar mandat vs realita 2.10.0

| Mandat | Status |
|---|---|
| semantic versioning | ADA (parse/banding/caret, Phase 30/31) |
| lock files | ADA (`nusa.lock`, pin+checksum) |
| dependency graph | ADA (resolver; belum divisualkan → `tree`) |
| checksums | ADA (transport + konten, 2-lapis) |
| package signing | BELUM (butuh infra kunci; §3) |
| registry mirrors | ADA-via-env (`NUSA_REGISTRY` ':'-separated, multi-sumber + fallback berurutan) → didokumentasikan sebagai mirrors |
| offline cache | ADA-implisit (cache valid ⇒ tanpa registry) → dibuktikan `verify` |
| reproducible resolution | ADA (`lockFresh` + tulis terurut) → didokumentasikan |
| `audit/tree/outdated/verify` | BELUM → milestone ini |

## 2. Empat perintah (read-only; top-level + `pkg`, cermin
// search/install/publish; tampil di `--help`)

- `tree`: graf dependensi dari manifest+lock (rekursi path-dep +
  pin registry; aman-siklus via visited; format `name version
  (source)` indentasi). Tanpa lock → resolve dari manifest
  (peringatan bila lock absen/tak-segar? cukup: pakai lock bila
  ada, bila tidak dari manifest langsung).
- `verify`: verifikasi ulang — manifest parse; tiap pin lock:
  path-dep = `dirChecksum` vs pin, registry = tarball cache vs
  index (bila cache ada; bila tak ada → SKIP jujur, bukan gagal);
  `packageDirSafe` atas dir ter-unpack. Baris OK/FAIL/SKIP per
  paket; rc != 0 bila ada FAIL.
- `outdated`: tiap registry-dep: versi terkunci vs maksimum di
  registries (baca index; abaikan bila index hilang). Kolom:
  `name req locked -> latest`. Bila lock absen → resolve segar
  sebagai basis? Sederhana: butuh lock (error jelas bila nihil).
- `audit`: laporan advisory (rc selalu 0 bila terbaca):
  capability per paket closure (root + path-dep rekursif +
  manifest cache registry bila ada), checksum tercatat, catatan
  `signing: unavailable (checksums only)`. Temuan = info, bukan
  vonis (tanpa DB advisory).

## 3. DITUNDA beralasan

Package signing (ed25519/minisign butuh format kunci + `publish
--sign` + verifikasi rantai — milestone tersendiri); enforcement
izin non-native (butuh interposisi syscall); `update --dry-run`.

## 4. Acceptance criteria

- `e2e_eco2.sh`: tree bentuk, verify OK lalu FAIL pasca-tamper,
  outdated menemukan versi baru, audit memuat capability.
- Seluruh suite hijau; bench identik.
