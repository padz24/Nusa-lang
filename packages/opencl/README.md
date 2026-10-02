# Paket `opencl` — query akselerator (referensi)

Paket ekosistem (bukan stdlib): membuktikan SEAM vendor-netral
untuk backend akselerator — paket + shim C + wrapper Nusa. Backend
dipilih PRESENSI via `dlopen` (bukan link-time), sehingga bahasa
inti tak terikat vendor apa pun (CUDA/Vulkan/OpenCL/WebGPU semua
muat di seam yang sama).

## API (query-only; angka saja)

- `gpu_platforms() -> i64` — jumlah platform (0 = tak ada).
- `gpu_devices() -> i64` — jumlah device platform pertama
  (-1 = gagal query).
- `gpu_class() -> i64` — bitmask `CL_DEVICE_TYPE` device pertama
  (`GPU=4`, `CPU=2`; 0 = tak ada) — dasar device selection.
- `gpu_present() -> bool` — ada yang bisa dipilih?

Nama device (string) + context/queue/buffer/kernel = follow-up
(butuh pointer lintas batas — di luar FFI hari ini).

## Bangun

```bash
cc -c packages/opencl/shim.c -o shim.o   # hanya -ldl
# lalu pkg build dengan [link] + native = true (lihat e2e_gpu.sh)
```

Lihat `docs/gpu-3.21.md` (desain abstraksi + hasil probe perangkat).
