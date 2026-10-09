# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · **Türkçe** · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Web sitesi ve tarayıcıdan tek tıkla yükleme: https://ikrasnodymov.github.io/meshmesh/**

ESP32 ve nRF52 tabanlı LoRa cihazları için şebekeden bağımsız mesajlaşma yazılımı. Mesajlar internet,
mobil şebeke ve sunucu olmadan, radyo üzerinden cihazdan cihaza aktarılır. Radyo katmanı
[MeshCore](https://github.com/meshcore-dev/MeshCore) olduğundan MeshMesh düğümleri standart
MeshCore düğümleriyle haberleşir.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Ana ekran">
<img src="site/img/m9-chat-en.png" width="32%" alt="Özel sohbet">
<img src="site/img/m9-radar-en.png" width="32%" alt="Sinyal radarı">
</p>

## Özellikler

- **Sohbetler** — özel ve genel mesajlar, ✓✓ iletim onayları, yeniden gönderim, her mesaj için gösterilen rota.
- **Kanallar** — en fazla 8 MeshCore kanalı: #hashtag, `meshcore://` bağlantısı veya QR kodu, ad ve anahtarla katılma;
  özel kanal oluşturma, kişileri doğrudan mesajla davet etme, havada duyulan kanalları bulma. [docs/en/channels.md](docs/en/channels.md)
- **Bölgeler** — MeshCore flood kapsamı: varsayılan bölge ve kanal başına bölge, yakındaki rölelerin bölgelerini arama; röleler `region` komutlarıyla. [docs/en/regions.md](docs/en/regions.md)
- **Çevrimdışı haritalar** — SD kartta OpenStreetMap karoları, haritada GPS konumu ve düğümler.
- **Yakındaki düğümler** — sinyal, atlama sayısı, mesafe ve yön; kişiler, tekrarlayıcılar ve odalar.
- **Sinyal radarı** — çevrenizdeki Wi-Fi, Bluetooth ve LoRa, “sıcak / soğuk” yön bulmayla.
- **Hareket sensörü** — iki kart, aralarından geçen bir insanı algılar (Wi-Fi CSI).
- Mesh üzerinden kişilerinizle **satranç** ve M9'da Klondike solitaire.
- **Ağ evcil hayvanı** — her kartta Tamagotchi tarzı piksel bir yaratık: radyo trafiğiyle beslenir, büyür ve açlıktan ya da yalnızlıktan ölebilir. [docs/en/pet.md](docs/en/pet.md)
- **Zarlar** — her kartta DIC3R zar atıcı: RPG zar havuzları ve formüller (2d6+1, d20, d66, d%), başarı eşiğiyle Warhammer ızgarası, sayaçlar ve karakterler; ekranda, web sayfasında ve uygulamada aynı. [docs/en/dice.md](docs/en/dice.md)
- **Cihazınızda MeshMesh olmadan satranç**: [satranç sayfası](https://ikrasnodymov.github.io/meshmesh/chess/), resmi MeshCore
  Companion yazılımını çalıştıran bir kart üzerinden, USB (Web Serial) veya Bluetooth (Web Bluetooth) ile Chrome ya da Edge'de oynar.
- **Tekrarlayıcı ve oda sunucusu modları** — açılışta (M9 ekranı, Heltec düğmesi) ya da Ayarlar'da,
  web sayfasında veya USB üzerinden seçilir: cihaz, standartla uyumlu bir MeshCore tekrarlayıcısı ya da oda sunucusu olur;
  MeshCore uygulamasından yönetici girişi ve uzak CLI sunar; anahtar, kişiler ve geçmiş korunur.
  [docs/en/repeater.md](docs/en/repeater.md)
- **MeshCore uygulamaları** — resmî MeshCore uygulamaları Bluetooth veya USB ile bağlanır ve kartın sohbetlerini, kişilerini ve kanallarını kullanır (satranç MeshMesh uygulamasında kalır). [docs/en/companion.md](docs/en/companion.md)
- **GPS ve pusula**, konum paylaşımı, kilit ekranı, fonetik Kiril klavye girişi.
- Cihazın kendi Wi-Fi erişim noktası veya bağlandığı ev Wi-Fi ağı (M9, T-Deck) üzerinden **web arayüzü** ve mesaj bildirimli bir **Android uygulaması**
  (Wi-Fi, Bluetooth LE veya USB).
- **15 dilde cihaz arayüzü**: İngilizce, Rusça, Ukraynaca, İspanyolca, Portekizce, Fransızca, Almanca, İtalyanca,
  Lehçe, Türkçe, Çince, Japonca, Korece, Arapça ve Endonezce. Web sitesi yazılımı seçtiğiniz dille yükler;
  dili daha sonra Ayarlar'dan değiştirebilirsiniz.

## Kartlar

| Kart | Durum |
|---|---|
| Elecrow ThinkNode M9 (klavye, 320×240 ekran) | donanımda test edildi |
| Heltec WiFi LoRa 32 V4 (OLED, tek düğme) | donanımda test edildi |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; Wi-Fi yok) | donanımda test edildi (bizim örneğimizde GPS modülü takılı değil) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, 240×135 renkli TFT, tek düğme; Wi-Fi yok) | donanımda test edildi — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | yalnızca derleniyor, henüz donanımda çalıştırılmadı — geri bildirimlerinizi bekliyoruz |

ESP32 kartlarında tüm özellikler vardır. GAT562'de (nRF52840) Wi-Fi yoktur: erişim noktası, Wi-Fi radarı,
hareket sensörü ve internet istemcisi bulunmaz; telefon Android uygulaması üzerinden Bluetooth veya USB ile bağlanır.
Buna karşılık ekran klavyesi ve OLED'de satranç sunar. Pinler ve ayrıntılar: [docs/en/boards.md](docs/en/boards.md).

## Yükleme

**Tarayıcı:** bilgisayarda Chrome veya Edge ile [web sitesini](https://ikrasnodymov.github.io/meshmesh/#install)
açın, kartınızı ve cihaz dilini seçin, **Yükle** düğmesine basın. MeshMesh güncellemesi anahtarınızı,
kişilerinizi, ayarlarınızı ve geçmişinizi korur; “Erase device” kutusunu yalnızca başka bir yazılımdan geçerken işaretleyin.

**esptool:** kartın zip dosyasını web sitesinden indirip şunu çalıştırın:

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(ESP32 kartları: `--chip esp32`, önyükleyici `0x1000` adresinde, `--flash-freq 40m`; flash boyutu karta göre.)

**GAT562 (nRF52840):** web sitesindeki aynı **Yükle** düğmesi (Web Serial üzerinden seri DFU); başka bir
yazılım çalıştıran kartta önce RESET'e iki kez basılmalıdır. Ya da RESET'e iki kez basın — `GAT562-BOOT`
sürücüsü belirir — ve web sitesinden indirdiğiniz `firmware.uf2` dosyasını oraya kopyalayın. Sonraki güncellemeler: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Varsayılan radyo: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — Ayarlar → Radyo bölümünden değiştirin; bir ağdaki
tüm düğümlerde aynı olmalıdır. Ülkenizin radyo mevzuatına uyun.

## Derleme

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Android uygulaması: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
Ekran arayüzü kart olmadan bilgisayarda çizdirilebilir: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Belgeler

İngilizce ayrıntılı belgeler: [kanallar](docs/en/channels.md), [bölgeler](docs/en/regions.md), [MeshCore uyumluluğu](docs/en/meshcore-migration.md), [kartlar](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [tekrarlayıcı ve oda modları](docs/en/repeater.md),
[satranç protokolü](docs/en/chess.md), [harita biçimi](docs/en/maps-format.md), [MeshCore ile karşılaştırma](docs/en/feature-parity.md),
[donanım doğrulaması](docs/en/verification.md). Rusça asılları [docs/](docs/) klasöründedir; kontrollere ve
özelliklere dair tam kılavuz [README.ru.md](README.ru.md) dosyasındadır.

Henüz doğrulanmadı: LoRa menzili, açık havada GPS doğruluğu, pusula doğruluğu, üçüncü bir tekrarlayıcı
üzerinden aktarma. Uygulanmadı: ses, rota planlama, OTA güncellemeleri.

## Lisans

[MIT](LICENSE). Pakete dahil MeshCore (`lib/MeshCore`) kendi MIT lisansını korur. Harita verileri ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) katkıcıları. Radar fikri,
[Stevee87](https://github.com/Stevee87) tarafından geliştirilen RSSI izleyici ve radar HUD'larından esinlenmiştir.
