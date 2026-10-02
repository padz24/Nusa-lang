/* Nusa Runtime 2.0 — Platform layer (Phase 55).
 *
 * Kontrak: inilah SATU-SATUNYA tempat kode platform-spesifik boleh
 * hidup. Subsistem (rt_thread/sync/chan/handle/time/net) hanya boleh
 * memakai API POSIX portabel + makro di bawah; dilarang `#ifdef
 * __ANDROID__` (atau platform lain) di luar header ini (dikunci
 * e2e_runtime.sh via grep).
 *
 * Status audit 2.6.0: NOL kode Android-spesifik di runtime
 * (hanya pthread + libc POSIX + /dev/urandom + fallback rand()).
 * Header ini mengkodifikasi batas untuk target masa depan
 * (Linux x86_64, WebAssembly, embedded).
 */
#ifndef NUSA_RT_PLATFORM_H
#define NUSA_RT_PLATFORM_H

/* Deteksi platform. */
#if defined(__ANDROID__)
#define NUSA_PLATFORM_ANDROID 1
#define NUSA_OS_NAME "Android"
#elif defined(__linux__)
#define NUSA_PLATFORM_LINUX 1
#define NUSA_OS_NAME "Linux"
#else
#define NUSA_OS_NAME "Unknown"
#endif

/* Runtime Nusa mensyaratkan LP64 (pointer muat di i64 handle). */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ != 8
#error "Nusa runtime membutuhkan pointer 64-bit (LP64)"
#endif

#endif /* NUSA_RT_PLATFORM_H */
