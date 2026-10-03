# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · **Bahasa Indonesia**

**Situs web dan instalasi sekali klik dari browser: https://ikrasnodymov.github.io/meshmesh/**

Firmware perpesanan off-grid untuk perangkat LoRa ESP32 dan nRF52. Pesan melompat lewat radio dari
perangkat ke perangkat, tanpa internet, tanpa jaringan seluler dan tanpa server. Lapisan radionya adalah
[MeshCore](https://github.com/meshcore-dev/MeshCore), sehingga node MeshMesh bisa berkomunikasi dengan node
MeshCore standar.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Layar beranda">
<img src="site/img/m9-chat-en.png" width="32%" alt="Chat langsung">
<img src="site/img/m9-radar-en.png" width="32%" alt="Radar sinyal">
</p>

## Fitur

- **Chat** — pesan langsung dan publik, tanda terima ✓✓, pengiriman ulang, rute ditampilkan per pesan.
- **Kanal** — hingga 8 kanal MeshCore: gabung lewat #hashtag, tautan `meshcore://` atau kode QR, nama dan kunci;
  buat kanal privat, undang kontak lewat pesan langsung, temukan kanal yang terdengar di udara. [docs/en/channels.md](docs/en/channels.md)
- **Peta offline** — tile OpenStreetMap di kartu SD, posisi GPS dan node di peta.
- **Node di sekitar** — sinyal, hop, jarak dan arah; kontak, repeater dan ruang.
- **Radar sinyal** — Wi-Fi, Bluetooth dan LoRa di sekitar Anda, dengan pelacakan “panas / dingin”.
- **Sensor gerak** — dua papan mendeteksi orang yang berjalan di antaranya (Wi-Fi CSI).
- **Catur** dengan kontak Anda lewat mesh, dan solitaire Klondike di M9.
- **Catur tanpa MeshMesh di perangkat Anda**: [halaman catur](https://ikrasnodymov.github.io/meshmesh/chess/) bermain melalui
  papan yang menjalankan firmware resmi MeshCore Companion, lewat USB (Web Serial) atau Bluetooth (Web Bluetooth), di Chrome atau Edge.
- **Mode repeater dan server ruang** — dipilih saat boot (layar M9, tombol Heltec) atau di Pengaturan,
  di halaman web, lewat USB: perangkat menjadi repeater atau server ruang MeshCore yang kompatibel dengan firmware standar,
  dengan login admin dan CLI jarak jauh dari aplikasi MeshCore; kunci, kontak dan riwayat tetap tersimpan.
  [docs/en/repeater.md](docs/en/repeater.md)
- **GPS dan kompas**, berbagi posisi, layar kunci, input keyboard Sirilik fonetik.
- **Antarmuka web** lewat titik akses Wi-Fi milik perangkat itu sendiri atau jaringan Wi-Fi rumah yang diikutinya (M9, T-Deck), dan **aplikasi Android**
  (Wi-Fi, Bluetooth LE atau USB) dengan notifikasi pesan.
- **Antarmuka perangkat dalam 15 bahasa**: Inggris, Rusia, Ukraina, Spanyol, Portugis, Prancis, Jerman, Italia,
  Polandia, Turki, Tionghoa, Jepang, Korea, Arab dan Indonesia. Situs web menginstal firmware dengan
  bahasa yang Anda pilih; bahasa bisa diubah nanti di Pengaturan.

## Papan

| Papan | Status |
|---|---|
| Elecrow ThinkNode M9 (keyboard, layar 320×240) | diuji di perangkat keras |
| Heltec WiFi LoRa 32 V4 (OLED, satu tombol) | diuji di perangkat keras |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; tanpa Wi-Fi) | diuji di perangkat keras (unit kami tidak memiliki modul GPS) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | hanya build, belum dijalankan di perangkat keras — laporan sangat diharapkan |

Papan ESP32 memiliki semua fitur. GAT562 (nRF52840) tidak punya Wi-Fi: tanpa titik akses, radar Wi-Fi,
sensor gerak maupun klien internet; ponsel terhubung lewat aplikasi Android via Bluetooth atau USB.
Sebagai gantinya ada keyboard di layar dan catur di OLED. Pin dan detail: [docs/en/boards.md](docs/en/boards.md).

## Instalasi

**Browser:** buka [situs web](https://ikrasnodymov.github.io/meshmesh/#install) di Chrome atau
Edge di komputer, pilih papan dan bahasa perangkat, lalu tekan **Instal**. Memperbarui MeshMesh
mempertahankan kunci, kontak, pengaturan dan riwayat Anda; centang “Erase device” hanya jika beralih dari firmware lain.

**esptool:** unduh zip papan dari situs web dan jalankan

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Papan ESP32: `--chip esp32`, bootloader di `0x1000`, `--flash-freq 40m`; ukuran flash sesuai papan.)

**GAT562 (nRF52840):** tombol **Instal** yang sama di situs web (serial DFU lewat Web Serial); papan
dengan firmware lain perlu ditekan RESET dua kali terlebih dahulu. Atau tekan RESET dua kali — drive `GAT562-BOOT`
akan muncul — lalu salin `firmware.uf2` dari situs web ke drive tersebut. Pembaruan berikutnya: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Radio bawaan: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — ubah di Pengaturan → Radio; semua node
dalam satu jaringan harus sama. Patuhi peraturan radio di negara Anda.

## Build

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Aplikasi Android: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
Antarmuka layar dapat dirender di komputer tanpa papan: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Dokumentasi

Dokumentasi terperinci dalam bahasa Inggris: [kanal](docs/en/channels.md), [kompatibilitas MeshCore](docs/en/meshcore-migration.md), [papan](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [mode repeater dan ruang](docs/en/repeater.md),
[protokol catur](docs/en/chess.md), [format peta](docs/en/maps-format.md), [perbandingan dengan MeshCore](docs/en/feature-parity.md),
[verifikasi perangkat keras](docs/en/verification.md). Versi asli berbahasa Rusia ada di [docs/](docs/), dengan
panduan lengkap kontrol dan fitur di [README.ru.md](README.ru.md).

Belum diverifikasi: jangkauan LoRa, akurasi GPS di ruang terbuka, akurasi kompas, relay melalui
repeater ketiga. Belum diimplementasikan: suara, perencanaan rute, pembaruan OTA.

## Lisensi

[MIT](LICENSE). MeshCore yang disertakan (`lib/MeshCore`) mempertahankan lisensi MIT-nya sendiri. Data peta ©
kontributor [OpenStreetMap](https://www.openstreetmap.org/copyright). Ide radar terinspirasi dari
pelacak RSSI dan HUD radar karya [Stevee87](https://github.com/Stevee87).
