# Supply-Chain Security — Milestone 1: Preseden + Beku (Phase 114)

Status: slice implementasi (target 3.15.0). Mandat master:
confusion/substitusi/update-jahat/kompromi/registry-attack;
pakai lock/signatures/checksums/registry tepercaya/provenance.
Tanpa kriptografi baru (keputusan Phase 113).

## 1. Temuan audit resolver (fakta, bukan asumsi)

- Preseden: registry PERTAMA yang cocok menang (`resolveInto`,
  `break` eksplisit). Multi-registry = kasus normal (mirror),
  sehingga shadowing NAMA adalah perilaku by-design — BAHAYAnya
  adalah ketidakterlihatan, bukan presedennya.
- Jaring yang sudah ada: pin lock mencatat `registry` + checksum;
  jalur pin TAK mencari-ulang (sumber terkunci pasca-`update`);
  `ensureCached` verifikasi ganda (transport + konten);
  traversal guard + name-mismatch (Phase 59/87).
- Jendela serangan tersisa: resolusi SEBAR (install/update) dari
  daftar registry beracun-pertama + build tanpa-lock yang
  fetch-diam-diam (`note: no nusa.lock; running update first`).

## 2. Yang dibangun (dua lapis, tanpa breaking)

- Lapis visibilitas: resolusi segar yang menemukan nama SAMA di
  registry lain → `warning: 'foo' also in registry 'B' (using
  first match from 'A')` (termasuk transitif; mirror tak rusak).
- Lapis beku: `pkg build --frozen` — gagal bila lock absen
  ("refusing to fetch…; run update explicitly") atau basi;
  TAK PERNAH fetch saat build. Untuk CI/rilis.
- `publish` dikecualikan sadar (op jaringan eksplisit).

## 3. Diverifikasi

- `e2e_supply` 4/4: frozen-segar, frozen-tanpa-lock (tak ada lock
  tercipta = tak ada fetch), frozen-basi, shadow-dua-registry
  (warning + checksum lock == index registry pertama).
- `e2e_pkg`/`e2e_registry`/`e2e_eco2` hijau (fixture satu-registry
  → tanpa warning baru); `e2e_release` hijau.

## 4. Ditunda jujur

- Signatures + publishers tepercaya + advisory DB (butuh primitif
  asimetris teraudit + feed — Phase 113); provenance di luar
  lock (registry-id + checksum) menunggu format advisory.
- Pin sumber per-dep (`[[package]] source-allowlist`) = kandidat
  milestone berikut (butuh desain manifest, tanpa RFC? format
  lock = RFC bila mengubah kontrak — dievaluasi saat itu).
