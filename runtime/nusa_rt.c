/* Runtime Nusa — agregator Runtime 2.0 (Phase 55, v2.6.0).
 *
 * SATU translation unit (disengaja): driver me-link file ini langsung
 * (`clang prog.ll runtime/nusa_rt.c`, fallback bila nusa_rt.o nihil)
 * dan .deb mengirimnya — nama + sifat single-file DIPERTAHANKAN.
 * Isi per subsistem tinggal di rt_*.c (dipindah verbatim); urutan
 * include = urutan dependensi (handle dulu). JANGAN kompilasi rt_*.c
 * terpisah. Peta + kontrak platform: docs/runtime-2.6.md.
 */
#include "rt_platform.h"

#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "rt_handle.c" /* Shared: registry (dipakai semua handle) */
#include "rt_alloc.c"  /* Alokator seam (weak; timpa via --link) */
#include "rt_thread.c" /* Threads */
#include "rt_sync.c"   /* Synchronization */
#include "rt_chan.c"   /* Channels */
#include "rt_time.c"   /* Time & entropy */
#include "rt_crypto.c" /* SHA-256 + OS entropy (Phase 76) */
#include "rt_net.c"    /* Networking */
