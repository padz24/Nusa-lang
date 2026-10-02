# Reproducible Builds — Design (Phase 61, target 2.12.0)

Status: audit → Implementation (milestone 1: tarball deterministik
+ pin compiler di lock). Jaminan terdokumentasi §4. Spec tak berubah.

## 1. Audit non-determinisme (2.11.0)

| Sumber | Status |
|---|---|
| `tar -czf` publish | NON-DETERMINISTIK (mtime/uid/gid/urutan + gzip timestamp; komentar kode mengakui "gzip tak deterministik") |
| Lock resolution | deterministik (sorted + first-match registry + tulis terurut) |
| Checksum konten | deterministik (`dirChecksum` terurut) |
| Compiler | minimum (`compiler =`), BUKAN pin → compiler beda = hasil beda |
| Biner `.ll`/exe | `source_filename` = path input (path-beda = byte-beda); urutan build = CLI; selebihnya deterministik LLVM-O0 |

## 2. Milestone 1

1. **Tarball deterministik**: `tar --sort=name --mtime='@0'
   --owner=0 --group=0 --numeric-owner -cf - … | gzip -n -c`
   (terverifikasi byte-identik di toolchain ini). Republish konten
   identik → byte identik (cabang "segarkan hash transport" menjadi
   no-op alami; logika dipertahankan).
2. **Pin compiler di lock**: `writeLock` emitsi header
   `[meta] compiler = "<exact>"`; `lockFresh` menolak bila absen
   atau beda (`run pkg update`). Lock lama (pra-2.12) menjadi
   basi SEKALI (self-heal via update; nol fixture lock di repo).
3. **Jaminan** (§4, jujur): path-sama + lock-sama + compiler-sama
   ⇒ tarball byte-identik & biner byte-identik (diuji `--fresh`
   ganda + `cmp`).

## 3. DITUNDA

Normalisasi path di `.ll` (source_filename absolut; butuh mode
`--remap-path` tersendiri), `.deb` reproducible (timestamp
dpkg-deb), bootstrap bit-identik lintas-mesin, signed artifacts.

## 4. Jaminan reproduksibilitas v2.12

Dijamin: (a) resolusi deps deterministik untuk registry-order
sama; (b) `nusa.lock` mencatat compiler eksak; (c) tarball
publish byte-deterministik; (d) build ulang `--fresh` di path
sama byte-identik. TIDAK dijamin: lintas path direktori (path
tertulis di IR), lintas versi compiler/LLVM, lintas platform.
