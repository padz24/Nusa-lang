# GPU / Accelerator Abstraction — Milestone 1: Eksplorasi + Seam (Phase 120)

Status: slice eksplorasi + paket referensi (target 3.21.0). Mandat
master: mulai eksplorasi; target awal device/query/transfer/kernel;
JANGAN ikat satu vendor; arsitektur muat CUDA/Vulkan/OpenCL/WebGPU
tanpa semuanya di bahasa inti.

## 1. Fakta perangkat (CI ini, terverifikasi via FFI Nusa)

- `/dev/kgsl*` TAK ADA; header OpenCL/Vulkan TAK ADA;
  `clinfo`/`vulkaninfo` TAK ADA; `/dev/dri/card0` ada (tak terpakai).
- `/system/vendor/lib64/libOpenCL.so` ADA — diakses via `dlopen`
  dari shim C (tanpa link-time dep): **1 platform, 1 device,
  kelas GPU** dari program Nusa (`ocl_platforms/devices/class`
  → `1/1/4`, rc 0). Eksperimen di `/tmp` sebelum komitmen.
- Batas FFI yang menentukan desain: TANPA pointer umum (sejak
  v1.13) → shim pola sqlite (handle/tabel slot + angka), NAMA
  device (string) dan buffer/context/queue = follow-up.

## 2. Seam vendor-netral (keputusan arsitektur)

- Lapisan: program Nusa → wrapper `.ns` → shim C per-backend →
  `dlopen` driver. Backend dipilih PRESENSI saat jalan, bukan
  saat kompilasi. Bahasa inti: NOL sintaks/IR/ABI baru (tetap).
- Paket referensi: `packages/opencl/` (query angka: platform,
  device, bitmask `CL_DEVICE_TYPE` untuk device selection) +
  manifest `[link]` + `native = true` + README. CUDA/Vulkan/
  WebGPU = paket SEAM-SAMA bila SDK-nya ada (pola didokumentasi,
  bukan janji).
- Transfer memori / kernel paralel: BUTUH buffer lintas batas
  (pointer) + antre perintah — prasyarat FFI, BUKAN slice ini
  (kriteria terima di §4).

## 3. Diverifikasi

- `e2e_gpu` 2/2: shim build + pkg jalur + run (kontrak struktural
  i64/bool + konsistensi device⇒platform⇒kelas; SKIP-device bila
  driver absen — portabel). `e2e_ffi` + `e2e_release` hijau.

## 4. Ditunda jujur + kriteria terima

- Nama device, context/queue/buffer/kernel, parallel-loop/SIMD
  tingkat-bahasa (Phase 121/122), backend kedua (bukti seam),
  WASM-GPU/WebGPU. Terima transfer-terbukti = program Nusa
  menjumlah vektor di device dan hasilnya benar (bukan klaim).
