# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · **Polski** · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Strona i instalacja jednym kliknięciem z przeglądarki: https://ikrasnodymov.github.io/meshmesh/**

Oprogramowanie do łączności poza siecią dla urządzeń LoRa z ESP32 i nRF52. Wiadomości skaczą drogą
radiową od urządzenia do urządzenia, bez internetu, bez sieci komórkowej i bez serwerów. Warstwą radiową
jest [MeshCore](https://github.com/meshcore-dev/MeshCore), więc węzły MeshMesh rozmawiają ze standardowymi
węzłami MeshCore.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Ekran główny">
<img src="site/img/m9-chat-en.png" width="32%" alt="Czat prywatny">
<img src="site/img/m9-radar-en.png" width="32%" alt="Radar sygnałów">
</p>

## Funkcje

- **Czaty** — wiadomości prywatne i publiczne, potwierdzenia doręczenia ✓✓, ponawianie, trasa widoczna przy każdej wiadomości.
- **Kanały** — do 8 kanałów MeshCore: dołączanie przez #hashtag, link `meshcore://` lub kod QR, nazwę i klucz;
  tworzenie kanału prywatnego, zapraszanie kontaktów wiadomością prywatną, wyszukiwanie kanałów słyszanych w eterze. [docs/en/channels.md](docs/en/channels.md)
- **Mapy offline** — kafelki OpenStreetMap na karcie SD, pozycja GPS i węzły na mapie.
- **Węzły w pobliżu** — sygnał, skoki, odległość i kierunek; kontakty, repeatery i pokoje.
- **Radar sygnałów** — Wi-Fi, Bluetooth i LoRa wokół ciebie, z namierzaniem „ciepło / zimno”.
- **Czujnik ruchu** — dwie płytki wykrywają osobę przechodzącą między nimi (Wi-Fi CSI).
- **Szachy** z kontaktami przez sieć mesh oraz pasjans Klondike na M9.
- **Zwierzak sieci** — pikselowe stworzonko w stylu Tamagotchi na każdej płytce: żywi się ruchem radiowym, rośnie i może umrzeć z głodu lub samotności. [docs/en/pet.md](docs/en/pet.md)
- **Kości** — rzucacz kośćmi DIC3R na każdej płytce: pule i formuły RPG (2d6+1, d20, d66, d%), siatka Warhammer z progiem sukcesu, liczniki i postacie, te same na ekranie, stronie WWW i w aplikacji. [docs/en/dice.md](docs/en/dice.md)
- **Szachy bez MeshMesh na urządzeniu**: [strona szachów](https://ikrasnodymov.github.io/meshmesh/chess/) gra przez
  płytkę z oficjalnym oprogramowaniem MeshCore Companion, przez USB (Web Serial) lub Bluetooth (Web Bluetooth), w Chrome lub Edge.
- **Tryby repeatera i serwera pokoju** — wybierane przy starcie (ekran M9, przycisk Heltec) lub w Ustawieniach,
  na stronie www, przez USB: urządzenie staje się repeaterem lub serwerem pokoju MeshCore zgodnym ze standardowym
  oprogramowaniem, z logowaniem administratora i zdalnym CLI z aplikacji MeshCore; klucz, kontakty i historia zostają.
  [docs/en/repeater.md](docs/en/repeater.md)
- **Aplikacje MeshCore** — oryginalne aplikacje MeshCore łączą się przez Bluetooth lub USB i korzystają z czatów, kontaktów i kanałów płytki (szachy zostają w aplikacji MeshMesh). [docs/en/companion.md](docs/en/companion.md)
- **GPS i kompas**, udostępnianie pozycji, ekran blokady, fonetyczne pisanie cyrylicą z klawiatury.
- **Interfejs www** przez własny punkt dostępu Wi-Fi urządzenia lub domową sieć Wi-Fi, do której się łączy (M9, T-Deck), oraz **aplikacja na Androida**
  (Wi-Fi, Bluetooth LE lub USB) z powiadomieniami o wiadomościach.
- **Interfejs urządzenia w 15 językach**: angielskim, rosyjskim, ukraińskim, hiszpańskim, portugalskim, francuskim,
  niemieckim, włoskim, polskim, tureckim, chińskim, japońskim, koreańskim, arabskim i indonezyjskim. Strona instaluje
  oprogramowanie z wybranym językiem; później można go zmienić w Ustawieniach.

## Płytki

| Płytka | Stan |
|---|---|
| Elecrow ThinkNode M9 (klawiatura, ekran 320×240) | sprawdzona na sprzęcie |
| Heltec WiFi LoRa 32 V4 (OLED, jeden przycisk) | sprawdzona na sprzęcie |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; bez Wi-Fi) | sprawdzona na sprzęcie (nasz egzemplarz nie ma modułu GPS) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, kolorowy TFT 240×135, jeden przycisk; bez Wi-Fi) | sprawdzona na sprzęcie — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | tylko kompilacja, jeszcze nieuruchamiane na sprzęcie — zgłoszenia mile widziane |

Płytki ESP32 mają wszystkie funkcje. GAT562 (nRF52840) nie ma Wi-Fi: brak punktu dostępu, radaru Wi-Fi,
czujnika ruchu i klienta internetu; telefon łączy się przez aplikację na Androida po Bluetooth lub USB.
W zamian ma klawiaturę ekranową i szachy na OLED. Piny i szczegóły: [docs/en/boards.md](docs/en/boards.md).

## Instalacja

**Przeglądarka:** otwórz [stronę](https://ikrasnodymov.github.io/meshmesh/#install) w Chrome lub
Edge na komputerze, wybierz płytkę i język urządzenia, naciśnij **Zainstaluj**. Aktualizacja MeshMesh
zachowuje klucz, kontakty, ustawienia i historię; zaznacz „Erase device” tylko przy przejściu z innego oprogramowania.

**esptool:** pobierz archiwum zip płytki ze strony i uruchom

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Płytki ESP32: `--chip esp32`, bootloader pod `0x1000`, `--flash-freq 40m`; rozmiar flash zależy od płytki.)

**GAT562 (nRF52840):** ten sam przycisk **Zainstaluj** na stronie (szeregowe DFU przez Web Serial); płytka
z innym oprogramowaniem wymaga najpierw dwukrotnego naciśnięcia RESET. Można też nacisnąć dwa razy RESET — pojawi się
dysk `GAT562-BOOT` — i skopiować na niego `firmware.uf2` ze strony. Kolejne aktualizacje: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Domyślne radio: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — zmienisz je w Ustawienia → Radio; wszystkie węzły
sieci muszą mieć te same parametry. Przestrzegaj przepisów radiowych obowiązujących w twoim kraju.

## Kompilacja

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Aplikacja na Androida: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
Interfejs ekranu można wyrenderować na komputerze bez płytki: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Dokumentacja

Szczegółowa dokumentacja po angielsku: [kanały](docs/en/channels.md), [zgodność z MeshCore](docs/en/meshcore-migration.md), [płytki](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [tryby repeatera i pokoju](docs/en/repeater.md),
[protokół szachów](docs/en/chess.md), [format map](docs/en/maps-format.md), [porównanie z MeshCore](docs/en/feature-parity.md),
[weryfikacja na sprzęcie](docs/en/verification.md). Rosyjskie oryginały są w [docs/](docs/), a pełny
przewodnik po sterowaniu i funkcjach — w [README.ru.md](README.ru.md).

Jeszcze niesprawdzone: zasięg LoRa, dokładność GPS pod otwartym niebem, dokładność kompasu, przekazywanie przez
trzeci repeater. Niezaimplementowane: głos, planowanie tras, aktualizacje OTA.

## Licencja

[MIT](LICENSE). Dołączony MeshCore (`lib/MeshCore`) zachowuje własną licencję MIT. Dane map ©
współtwórcy [OpenStreetMap](https://www.openstreetmap.org/copyright). Pomysł radaru nawiązuje do
trackera RSSI i radarowych HUD-ów autorstwa [Stevee87](https://github.com/Stevee87).
