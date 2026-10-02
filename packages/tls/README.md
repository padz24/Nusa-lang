# tls — paket referensi ekosistem Nusa (v2.29.0)

Klien TLS atas OpenSSL sistem via shim C (pola `packages/sqlite`).
Verifikasi peer + hostname **selalu aktif** — tidak ada mode
insecure; CA via file eksplisit atau path default sistem.

## Bangun

```sh
cc -c shim.c                  # butuh header+lib ssl
nusa build app.ns -o app --link ./shim.o --link ssl --link crypto
# atau via paket: salin shim.o ke direktori paket, lalu nusa pkg build
```

## Batas referensi (bukan produksi)

- Klien saja (serve-TLS butuh terminasi sertifikat — follow-up).
- `recv`/`request` pola request/response (EOF = akhir body).
- 16 koneksi bersamaan; tutup segera (`tls_close`) — dial
  menganggur menahan server sekuensial (deadlock wajar,
  bukan bug shim).
