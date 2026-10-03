# MeshMesh

[English](README.md) · [Русский](README.ru.md) · **Українська** · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Сайт і встановлення з браузера в один клік: https://ikrasnodymov.github.io/meshmesh/**

Прошивка для автономного обміну повідомленнями на LoRa-пристроях з ESP32 і nRF52. Повідомлення
передаються радіо від пристрою до пристрою — без інтернету, мобільної мережі й серверів. Радіошар —
[MeshCore](https://github.com/meshcore-dev/MeshCore), тож вузли MeshMesh спілкуються зі звичайними
вузлами MeshCore.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Головний екран">
<img src="site/img/m9-chat-en.png" width="32%" alt="Особистий чат">
<img src="site/img/m9-radar-en.png" width="32%" alt="Радар сигналів">
</p>

## Можливості

- **Чати** — особисті й загальні повідомлення, позначки доставки ✓✓, повтори, маршрут для кожного повідомлення.
- **Канали** — до 8 каналів MeshCore: вступ за #хештегом, посиланням `meshcore://` або QR-кодом, назвою й ключем;
  створення закритого каналу, запрошення контактів особистим повідомленням, пошук каналів, почутих в ефірі. [docs/en/channels.md](docs/en/channels.md)
- **Офлайн-карти** — тайли OpenStreetMap на SD-карті, GPS-позиція та вузли на карті.
- **Вузли поруч** — сигнал, кількість пересилань, відстань і напрямок; контакти, репітери й кімнати.
- **Радар сигналів** — Wi-Fi, Bluetooth і LoRa навколо вас, пошук за принципом «тепліше / холодніше».
- **Датчик руху** — дві плати помічають людину, що проходить між ними (Wi-Fi CSI).
- **Шахи** з вашими контактами через mesh-мережу та пасьянс «Клондайк» на M9.
- **Шахи без MeshMesh на пристрої**: [сторінка шахів](https://ikrasnodymov.github.io/meshmesh/chess/) грає через
  плату з офіційною прошивкою MeshCore Companion — через USB (Web Serial) або Bluetooth (Web Bluetooth), у Chrome чи Edge.
- **Режими репітера й сервера кімнати** — вибираються під час запуску (екран M9, кнопка Heltec) або в налаштуваннях,
  на вебсторінці, через USB: пристрій стає сумісним зі штатною прошивкою репітером або сервером кімнати MeshCore
  із входом адміністратора й віддаленим CLI із застосунку MeshCore; ключ, контакти й історія зберігаються.
  [docs/en/repeater.md](docs/en/repeater.md)
- **GPS і компас**, передавання позиції, екран блокування, фонетичне введення кирилицею з клавіатури.
- **Веб-інтерфейс** через власну точку доступу Wi-Fi пристрою або домашню мережу Wi-Fi, до якої він підключений (M9, T-Deck), та **застосунок для Android**
  (Wi-Fi, Bluetooth LE або USB) зі сповіщеннями про повідомлення.
- **Інтерфейс пристрою 15 мовами**: англійською, російською, українською, іспанською, португальською, французькою,
  німецькою, італійською, польською, турецькою, китайською, японською, корейською, арабською та індонезійською.
  Сайт встановлює прошивку з вибраною мовою; пізніше її можна змінити в налаштуваннях.

## Плати

| Плата | Стан |
|---|---|
| Elecrow ThinkNode M9 (клавіатура, екран 320×240) | перевірено на залізі |
| Heltec WiFi LoRa 32 V4 (OLED, одна кнопка) | перевірено на залізі |
| GAT562 30S Mesh Kit (nRF52840, OLED, джойстик; без Wi-Fi) | перевірено на залізі (на нашому екземплярі модуль GPS не встановлено) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | лише збірки, на залізі ще не запускалися — будемо раді звітам |

Плати на ESP32 мають усі функції. GAT562 (nRF52840) не має Wi-Fi: немає точки доступу, Wi-Fi-радара,
датчика руху та інтернет-клієнта; телефон підключається через застосунок для Android по Bluetooth або USB.
Натомість є екранна клавіатура й шахи на OLED. Виводи й подробиці: [docs/en/boards.md](docs/en/boards.md).

## Встановлення

**Браузер:** відкрийте [сайт](https://ikrasnodymov.github.io/meshmesh/#install) у Chrome або
Edge на комп'ютері, виберіть плату й мову пристрою, натисніть **«Встановити»**. Оновлення MeshMesh
зберігає ключ, контакти, налаштування й історію; позначайте «Erase device» лише під час переходу з іншої прошивки.

**esptool:** завантажте zip-архів плати із сайту та виконайте

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Плати ESP32: `--chip esp32`, завантажувач за адресою `0x1000`, `--flash-freq 40m`; розмір flash — залежно від плати.)

**GAT562 (nRF52840):** та сама кнопка **«Встановити»** на сайті (послідовний DFU через Web Serial); на платі
з іншою прошивкою спершу двічі натисніть RESET. Або двічі натисніть RESET — з'явиться диск `GAT562-BOOT` —
і скопіюйте на нього `firmware.uf2` із сайту. Подальші оновлення: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Радіо за замовчуванням: 868.731 МГц, BW 62.5 кГц, SF8, CR4/6 — змінюється в «Налаштування → Радіо»; параметри
всіх вузлів мережі мають збігатися. Дотримуйтеся правил використання радіочастот у своїй країні.

## Збірка

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Застосунок для Android: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
Інтерфейс екрана можна відрендерити на комп'ютері без плати: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Документація

Докладна документація англійською: [канали](docs/en/channels.md), [сумісність із MeshCore](docs/en/meshcore-migration.md), [плати](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [режими репітера й кімнати](docs/en/repeater.md),
[протокол шахів](docs/en/chess.md), [формат карт](docs/en/maps-format.md), [порівняння з MeshCore](docs/en/feature-parity.md),
[перевірка на залізі](docs/en/verification.md). Російські оригінали — у [docs/](docs/), а повний
посібник з керування й можливостей — у [README.ru.md](README.ru.md).

Ще не перевірено: дальність LoRa, точність GPS під відкритим небом, точність компаса, пересилання через
третій репітер. Не реалізовано: голос, прокладання маршрутів, оновлення OTA.

## Ліцензія

[MIT](LICENSE). Вбудована MeshCore (`lib/MeshCore`) зберігає власну ліцензію MIT. Картографічні дані ©
учасники [OpenStreetMap](https://www.openstreetmap.org/copyright). Ідея радара спирається на
RSSI-трекер і радарні HUD від [Stevee87](https://github.com/Stevee87).
