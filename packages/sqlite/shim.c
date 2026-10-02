/* Shim SQLite handle-based untuk FFI Nusa (paket referensi ekosistem).
 *
 * `extern fn` Nusa hanya mengenal string/i64/bool (return string
 * pun ditolak — kepemilikan memori tak terekspresikan). Shim ini
 * menyembunyikan pointer di tabel slot (handle = i64) dan menulis
 * hasil query sebagai CSV ke file (baris header = nama kolom);
 * Nusa membaca via file_read + csv.ns. Tak ada buffer lintas batas.
 *
 * Kontrak: query untuk SELECT; NULL → field kosong; -1 = error.
 */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NDB_MAX 16
static sqlite3 *ndb_slots[NDB_MAX] = {0};

static int ndb_ok(long long id) {
  return id >= 0 && id < NDB_MAX && ndb_slots[(int)id] != 0;
}

/* Buka DB (path atau ":memory:"); id slot atau -1. */
long long ndb_open(const char *path) {
  int i;
  if (!path)
    path = ":memory:";
  for (i = 0; i < NDB_MAX; i++) {
    if (!ndb_slots[i]) {
      if (sqlite3_open(path, &ndb_slots[i]) == SQLITE_OK)
        return (long long)i;
      ndb_slots[i] = 0;
      return -1;
    }
  }
  return -1;
}

/* Eksekusi non-query; jumlah changes atau -1. */
long long ndb_exec(long long id, const char *sql) {
  char *err = 0;
  int rc;
  if (!ndb_ok(id) || !sql)
    return -1;
  rc = sqlite3_exec(ndb_slots[(int)id], sql, 0, 0, &err);
  if (err)
    sqlite3_free(err);
  if (rc != SQLITE_OK)
    return -1;
  return (long long)sqlite3_changes(ndb_slots[(int)id]);
}

/* Satu sel CSV (NULL → kosong; quote bila perlu, `""` escape). */
static void ndb_csv_cell(FILE *f, const unsigned char *t) {
  const unsigned char *p;
  if (!t)
    return;
  for (p = t; *p; p++) {
    if (*p == ',' || *p == '"' || *p == '\r' || *p == '\n')
      break;
  }
  if (!*p) {
    fputs((const char *)t, f);
    return;
  }
  fputc('"', f);
  for (p = t; *p; p++) {
    if (*p == '"')
      fputc('"', f);
    fputc(*p, f);
  }
  fputc('"', f);
}

/* Query SELECT → CSV di `path` (baris pertama = nama kolom).
 * Kembalikan jumlah baris data (tanpa header) atau -1. */
long long ndb_query_dump(long long id, const char *sql, const char *path) {
  sqlite3_stmt *st = 0;
  FILE *f;
  long long n = 0;
  int cols, i, rc;
  if (!ndb_ok(id) || !sql || !path)
    return -1;
  if (sqlite3_prepare_v2(ndb_slots[(int)id], sql, -1, &st, 0) != SQLITE_OK)
    return -1;
  f = fopen(path, "w");
  if (!f) {
    sqlite3_finalize(st);
    return -1;
  }
  cols = sqlite3_column_count(st);
  for (i = 0; i < cols; i++) {
    if (i)
      fputc(',', f);
    ndb_csv_cell(f, (const unsigned char *)sqlite3_column_name(st, i));
  }
  fputc('\n', f);
  while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
    for (i = 0; i < cols; i++) {
      if (i)
        fputc(',', f);
      ndb_csv_cell(f, sqlite3_column_text(st, i));
    }
    fputc('\n', f);
    n++;
  }
  fclose(f);
  sqlite3_finalize(st);
  return rc == SQLITE_DONE ? n : -1;
}

/* Tutup koneksi; 0 ok, -1 id buruk. */
long long ndb_close(long long id) {
  if (!ndb_ok(id))
    return -1;
  sqlite3_close(ndb_slots[(int)id]);
  ndb_slots[(int)id] = 0;
  return 0;
}
