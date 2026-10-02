# Runtime 2.0 — Design (Phase 55, target 2.6.0)

Status: audit selesai → Implementation (milestone 1: modularisasi
tanpa perubahan perilaku). Spec 2.0 tak berubah; tanpa RFC.

## 1. Audit batasan keras (mengapa bukan split biasa)

- Driver me-link **satu** sumber fallback `runtime/nusa_rt.c`
  langsung via clang (`findRuntimeFile`), dan .deb mengirim file
  itu (`packaging/deb.sh`). Menghapus/mengganti nama file =
  merusak instalasi + fallback. Solusi: `nusa_rt.c` tetap ada
  sebagai **agregator satu translation unit** (`#include`
  bagian-bagian, urutan dependensi) — objek hasil identik,
  fallback dan .deb tetap jalan (deb mengirim direktori).
- Objek precompiled `build/nusa_rt.o` tetap satu file (driver
  tak diubah sama sekali).
- Audit platform: NOL Android-spesifik di runtime hari ini
  (`__ANDROID__`/bionic/`linux/*` = nihil; hanya POSIX +
  `/dev/urandom` + `rand()` fallback). Platform layer milestone
  ini = kodifikasi batas (bukan evakuasi kode).

## 2. Target (mandat master)

```
Nusa Runtime
  ├── Memory .... (milik codegen: alloca/malloc+OOM-check; tetap)
  ├── Threads ... rt_thread.c (spawn/join/sleep)
  ├── Sync ...... rt_sync.c (mutex/atomic) + rt_chan.c (channel)
  ├── Time/Rand . rt_time.c (clock/urandom)
  ├── Net ....... rt_net.c (tcp client/server loopback)
  ├── Shared .... rt_handle.c (registry Phase 54)
  └── Platform .. rt_platform.h (deteksi + kontrak, §3)
```

I/O file & proses tetap emisi libc langsung di codegen (tidak
pindah — di luar mandat; dipetakan di sini agar eksplisit).

## 3. Platform layer (`runtime/rt_platform.h`)

- Makro `NUSA_PLATFORM_ANDROID` (`__ANDROID__`), `NUSA_PLATFORM_LINUX`,
  `NUSA_OS_NAME` string; static-assert LP64.
- Kontrak: kode platform-spesifik masa depan (WASM/embedded)
  HANYA boleh hidup di balik header ini; subsystem tidak boleh
  `#ifdef __ANDROID__` sendiri (dikunci `e2e_runtime.sh` via grep).

## 4. Milestone 1 (SATU-SATUNYA perubahan fase ini)

Pindah verbatim per subsistem (nol logika): thread, sync, chan,
handle, time, net + agregator + platform.h. CMake `DEPENDS`
diperluas; `deb.sh` kirim direktori `runtime/`.

DITUNDA: pemisahan I/O-Proses ke runtime (butuh desain emisi
codegen), platform kedua (Linux x86_64 build farm), refcount
handle (tercatat di mem-2.5.md).

## 5. Acceptance criteria

- `nm nusa_rt.o` memuat simbol yang sama persis sebelum/sesudah.
- Jalur fallback (tanpa `nusa_rt.o`) build+run OK.
- `e2e_runtime.sh` hijau + seluruh suite hijau + bench identik.
