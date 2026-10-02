# Security-First Package Ecosystem — Milestone 1: SBOM (Phase 113)

Status: slice implementasi (target 3.14.0). Mandat master: signatures,
checksums, trusted publishers, audit, SBOM, metadata kerentanan;
CLI audit/verify/sbom; build scripts berbatas izin.

## 1. Cakupan (disengaja: tanpa kriptografi baru)

- Dibangun: `nusa sbom` (+ `nusa pkg sbom`) — inventaris JSON
  `nusa-sbom/1` dari manifest + lock: root, components (path +
  registry), checksum, capabilities eksplisit, cached, +
  penanda `"signatures":false`. Deterministik (byte-identik).
- Sudah ada ( dipakai ulang, tak diduplikasi): checksum sha256
  lock (`verify`), capability manifest + enforcement
  `native=true` (`audit`, Phase 59/60), lock hermetik (Phase 80).

## 2. Ditunda beralasan (bukan ditolak)

- Signatures: BUTUH primitif asimetris teraudit. Nusa hanya punya
  sha256 + urandom (keputusan sadar Phase 76: tanpa algoritma
  baru). Opsi: (a) minisign/ed25519 eksternal terverifikasi,
  (b) Sigstore-style. Keduanya butuh infra kunci + audit —
  di luar milestone ini. SBOM menandai `"signatures":false`
  agar KETIADAANNYA eksplisit (bukan klaim diam).
- Trusted publishers / vulnerability metadata: BUTUH sumber data
  (registry advisory). Format advisory + discovery = milestone
  berikut (Phase 114 supply-chain); tanpa feed, skema = dekorasi.
- Permission boundaries build scripts: tak ada build script/hook
  di model paket (hanya tar+clang sejak Phase 59) — batas ini
  dipertahankan; bila hook diperkenalkan, wajib capability-gate
  ala plugin (Phase 112).

## 3. Diverifikasi

- `e2e_sbom` 3/3: fixture path+registry, JSON valid, capability,
  checksum pin, determinisme byte, `pkg sbom` identik, tanpa
  manifest ditolak. `e2e_eco2` + `e2e_release` hijau (tak tersentuh).
