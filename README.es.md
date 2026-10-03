# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · **Español** · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Sitio web e instalación desde el navegador con un clic: https://ikrasnodymov.github.io/meshmesh/**

Firmware de mensajería sin red para dispositivos LoRa con ESP32 y nRF52. Los mensajes saltan por radio de un
dispositivo a otro, sin internet, sin red móvil y sin servidores. La capa de radio es
[MeshCore](https://github.com/meshcore-dev/MeshCore), así que los nodos MeshMesh se comunican con nodos
MeshCore estándar.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Pantalla de inicio">
<img src="site/img/m9-chat-en.png" width="32%" alt="Chat directo">
<img src="site/img/m9-radar-en.png" width="32%" alt="Radar de señales">
</p>

## Funciones

- **Chats**: mensajes directos y públicos, acuses de entrega ✓✓, reintentos y la ruta de cada mensaje.
- **Canales**: hasta 8 canales MeshCore. Únete por #hashtag, enlace `meshcore://` o código QR, nombre y clave;
  crea uno privado, invita a tus contactos por mensaje directo y encuentra canales oídos en el aire. [docs/en/channels.md](docs/en/channels.md)
- **Mapas sin conexión**: teselas de OpenStreetMap en la tarjeta SD, posición GPS y nodos en el mapa.
- **Nodos cercanos**: señal, saltos, distancia y rumbo; contactos, repetidores y salas.
- **Radar de señales**: Wi-Fi, Bluetooth y LoRa a tu alrededor, con rastreo tipo “frío / caliente”.
- **Sensor de movimiento**: dos placas detectan a una persona que camina entre ellas (Wi-Fi CSI).
- **Ajedrez** con tus contactos a través de la malla, y solitario Klondike en el M9.
- **Ajedrez sin MeshMesh en tu dispositivo**: [la página de ajedrez](https://ikrasnodymov.github.io/meshmesh/chess/) juega a través de
  una placa con el firmware oficial MeshCore Companion, por USB (Web Serial) o Bluetooth (Web Bluetooth), en Chrome o Edge.
- **Modos repetidor y servidor de sala**: se eligen al arrancar (pantalla del M9, botón del Heltec) o en Ajustes,
  en la página web o por USB. El dispositivo se convierte en un repetidor o servidor de sala MeshCore compatible con el original,
  con acceso de administrador y CLI remota desde la app MeshCore; la clave, los contactos y el historial se conservan.
  [docs/en/repeater.md](docs/en/repeater.md)
- **GPS y brújula**, posición compartida, pantalla de bloqueo, escritura fonética en cirílico con el teclado.
- **Interfaz web** a través del propio punto de acceso Wi-Fi del dispositivo o de la red Wi-Fi doméstica a la que se conecta (M9, T-Deck), y una **app para Android**
  (Wi-Fi, Bluetooth LE o USB) con notificaciones de mensajes.
- **Interfaz del dispositivo en 15 idiomas**: inglés, ruso, ucraniano, español, portugués, francés, alemán, italiano,
  polaco, turco, chino, japonés, coreano, árabe e indonesio. El sitio web instala el firmware con
  el idioma que elijas; luego se puede cambiar en Ajustes.

## Placas

| Placa | Estado |
|---|---|
| Elecrow ThinkNode M9 (teclado, pantalla 320×240) | probada en hardware |
| Heltec WiFi LoRa 32 V4 (OLED, un botón) | probada en hardware |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; sin Wi-Fi) | probada en hardware (nuestra unidad no tiene módulo GPS instalado) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | solo compilación, aún sin probar en hardware; los informes son bienvenidos |

Las placas ESP32 tienen todas las funciones. La GAT562 (nRF52840) no tiene Wi-Fi: ni punto de acceso, ni radar Wi-Fi,
ni sensor de movimiento, ni cliente de internet; el teléfono se conecta mediante la app para Android por Bluetooth o USB.
A cambio, añade un teclado en pantalla y ajedrez en el OLED. Pines y detalles: [docs/en/boards.md](docs/en/boards.md).

## Instalación

**Navegador:** abre el [sitio web](https://ikrasnodymov.github.io/meshmesh/#install) en Chrome o
Edge en un ordenador, elige tu placa y el idioma del dispositivo y pulsa **Instalar**. Al actualizar MeshMesh
se conservan la clave, los contactos, los ajustes y el historial; marca “Erase device” solo si vienes de otro firmware.

**esptool:** descarga el zip de tu placa desde el sitio web y ejecuta

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Placas ESP32: `--chip esp32`, bootloader en `0x1000`, `--flash-freq 40m`; el tamaño de flash depende de la placa.)

**GAT562 (nRF52840):** el mismo botón **Instalar** del sitio web (DFU serie por Web Serial); una placa
con otro firmware necesita antes pulsar RESET dos veces. O bien pulsa RESET dos veces —aparece la unidad `GAT562-BOOT`—
y copia en ella `firmware.uf2` desde el sitio web. Actualizaciones posteriores: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Radio por defecto: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6; se cambia en Ajustes → Radio. Todos los nodos
de una red deben coincidir. Respeta la normativa de radio de tu país.

## Compilación

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

App para Android: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
La interfaz de la pantalla se puede renderizar en un ordenador sin placa: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Documentación

Documentación detallada en inglés: [canales](docs/en/channels.md), [compatibilidad con MeshCore](docs/en/meshcore-migration.md), [placas](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [modos repetidor y sala](docs/en/repeater.md),
[protocolo de ajedrez](docs/en/chess.md), [formato de mapas](docs/en/maps-format.md), [comparación con MeshCore](docs/en/feature-parity.md),
[verificación en hardware](docs/en/verification.md). Los originales en ruso están en [docs/](docs/), y la guía
completa de controles y funciones, en [README.ru.md](README.ru.md).

Aún sin verificar: alcance LoRa, precisión del GPS a cielo abierto, precisión de la brújula, reenvío a través de
un tercer repetidor. No implementado: voz, planificación de rutas, actualizaciones OTA.

## Licencia

[MIT](LICENSE). El MeshCore incluido (`lib/MeshCore`) mantiene su propia licencia MIT. Datos de mapas ©
colaboradores de [OpenStreetMap](https://www.openstreetmap.org/copyright). La idea del radar se inspira en
el rastreador RSSI y los HUD de radar de [Stevee87](https://github.com/Stevee87).
