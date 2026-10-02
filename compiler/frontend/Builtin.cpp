// Implementasi dipindah verbatim dari Semantic.cpp (Phase 51).
// Nol perubahan perilaku; hanya lokasi (Frontend layer).
#include "compiler/frontend/Builtin.h"

namespace nusa {

bool isBuiltin(const std::string &name) {
  return name == "len" ||        // v0.7.0: panjang array
         name == "input" ||      // v0.9.0: baca baris stdin
         name == "assert" ||     // v0.9.0: asersi runtime
         name == "str_len" ||    // v0.9.0: strlen
         name == "str_concat" || // v0.9.0: gabung string
         name == "str_eq" ||     // v1.6.0: banding string (match)
         name == "parse_int" ||  // v0.9.0: string -> i64
         name == "parse_int_valid" || // v1.5.0: string -> bool
         name == "file_read" ||  // v0.9.0: baca file -> string
         name == "file_write" || // v0.9.0: tulis file -> bool
         name == "file_exists" || // v0.9.0: cek file -> bool
         name == "array_sum" ||  // v0.9.0: jumlah elemen [i64]
         name == "array_min" ||  // v0.9.0: minimum [i64]
         name == "array_max" ||  // v0.9.0: maksimum [i64]
         name == "thread_spawn" || // v1.9.0: luncurkan thread
         name == "thread_join" || // v1.9.0: tunggu thread
         name == "mutex_new" || name == "mutex_lock" || // v1.9.0: mutex
         name == "mutex_unlock" || name == "mutex_free" ||
         name == "mutex_try_lock" ||
         name == "chan_new" || name == "chan_send" || // v1.9.0: channel
         name == "chan_recv" || name == "chan_free" ||
         name == "chan_new_bool" || name == "chan_send_bool" || // 2.0: tag
         name == "chan_recv_bool" || name == "chan_new_str" ||
         name == "chan_send_str" || name == "chan_recv_str" ||
         name == "atomic_new" || name == "atomic_get" || // v1.9.0: atomik
         name == "atomic_set" || name == "atomic_add" ||
         name == "atomic_free" || name == "sleep_ms" || // v1.9.0: tidur
         name == "str_char_at" || name == "str_slice" || // v1.10.0: string
         name == "int_to_str" || // v1.19.0: i64 -> string
         name == "time_now_ms" || name == "time_now_s" || // v1.10.0: waktu
         name == "random_u64" || // v1.10.0: entropi
         name == "sha256_hex" || // v2.27.0: SHA-256 hex (RFC-0011)
         name == "crypto_rand_hex" || // v2.27.0: hex acak OS (RFC-0011)
         name == "process_exit" || name == "process_env" || // v1.10.0: proses
         name == "process_cwd" || name == "process_argc" ||
         name == "process_argv" || name == "tcp_connect" || // v1.10.0: net
         name == "tcp_send" || name == "tcp_recv" || name == "tcp_close" ||
         name == "tcp_listen" || name == "tcp_accept" || // 2.0: server
         name == "chan_len" || // v2.7.0: panjang antre channel
         name == "chan_new_bounded" || // v3.19.0: channel bounded
         name == "chan_try_send"; // v3.19.0: kirim non-blocking
}

} // namespace nusa
