# sqlite — paket referensi ekosistem Nusa (v2.28.0)

Bukti pola Phase 77: database engine hidup di **ekosistem**,
bukan stdlib. Paket ini membungkus libsqlite3 sistem via shim C
handle-based (`extern fn` menolak return string — kepemilikan
memori — maka hasil query ditulis CSV ke file lalu dibaca via
file_read + csv.ns; pointer `sqlite3*` pun tak terekspresikan
langsung).

## Bangun

```sh
cc -c shim.c                 # butuh header+lib sqlite3
nusa build app.ns -o app --link ./shim.o --link sqlite3
# atau via paket: salin shim.o ke direktori paket, lalu nusa pkg build
```

## Batas referensi (bukan produksi)

- 16 koneksi bersamaan; `ndb_query_dump` untuk SELECT.
- NULL → field kosong; nilai BLOB dibaca sebagai teks.
