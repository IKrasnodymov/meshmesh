# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · **Italiano** · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Sito web e installazione dal browser con un clic: https://ikrasnodymov.github.io/meshmesh/**

Firmware di messaggistica off-grid per dispositivi LoRa ESP32 e nRF52. I messaggi saltano via radio da un
dispositivo all'altro, senza internet, senza rete mobile e senza server. Lo strato radio è
[MeshCore](https://github.com/meshcore-dev/MeshCore), quindi i nodi MeshMesh comunicano con i normali
nodi MeshCore.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Schermata principale">
<img src="site/img/m9-chat-en.png" width="32%" alt="Chat diretta">
<img src="site/img/m9-radar-en.png" width="32%" alt="Radar dei segnali">
</p>

## Funzioni

- **Chat** — messaggi diretti e pubblici, conferme di consegna ✓✓, reinvii, percorso mostrato per ogni messaggio.
- **Canali** — fino a 8 canali MeshCore: entra con #hashtag, link `meshcore://` o codice QR, nome e chiave;
  creane uno privato, invita i contatti con un messaggio diretto, trova i canali sentiti in onda. [docs/en/channels.md](docs/en/channels.md)
- **Mappe offline** — tile OpenStreetMap sulla scheda SD, posizione GPS e nodi sulla mappa.
- **Nodi vicini** — segnale, hop, distanza e direzione; contatti, ripetitori e stanze.
- **Radar dei segnali** — Wi-Fi, Bluetooth e LoRa intorno a te, con ricerca “acqua / fuoco”.
- **Sensore di movimento** — due schede rilevano una persona che cammina tra loro (Wi-Fi CSI).
- **Scacchi** con i tuoi contatti attraverso la mesh, e il solitario Klondike sull'M9.
- **Cucciolo della rete** — una creatura in pixel in stile Tamagotchi su ogni scheda: si nutre del traffico radio, cresce e può morire di fame o di solitudine. [docs/en/pet.md](docs/en/pet.md)
- **Dadi** — il lanciadadi DIC3R su ogni scheda: riserve e formule da GdR (2d6+1, d20, d66, d%), una griglia Warhammer con soglia di successo, contatori e personaggi, uguali sullo schermo, sulla pagina web e nell’app. [docs/en/dice.md](docs/en/dice.md)
- **Scacchi senza MeshMesh sul dispositivo**: [la pagina degli scacchi](https://ikrasnodymov.github.io/meshmesh/chess/) gioca tramite
  una scheda con il firmware ufficiale MeshCore Companion, via USB (Web Serial) o Bluetooth (Web Bluetooth), in Chrome o Edge.
- **Modalità ripetitore e server stanza** — scelte all'avvio (schermo dell'M9, pulsante dell'Heltec) o in Impostazioni,
  sulla pagina web, via USB: il dispositivo diventa un ripetitore o un server stanza MeshCore compatibile con quelli originali,
  con accesso admin e CLI remota dall'app MeshCore; chiave, contatti e cronologia restano.
  [docs/en/repeater.md](docs/en/repeater.md)
- **App MeshCore** — le app MeshCore ufficiali si collegano via Bluetooth o USB e usano chat, contatti e canali della scheda (gli scacchi restano nell'app MeshMesh). [docs/en/companion.md](docs/en/companion.md)
- **GPS e bussola**, condivisione della posizione, schermata di blocco, input cirillico fonetico da tastiera.
- **Interfaccia web** tramite il punto di accesso Wi-Fi del dispositivo stesso o la rete Wi-Fi di casa a cui si collega (M9, T-Deck), e un'**app Android**
  (Wi-Fi, Bluetooth LE o USB) con notifiche dei messaggi.
- **Interfaccia del dispositivo in 15 lingue**: inglese, russo, ucraino, spagnolo, portoghese, francese, tedesco, italiano,
  polacco, turco, cinese, giapponese, coreano, arabo e indonesiano. Il sito installa il firmware nella
  lingua che scegli; puoi cambiarla dopo in Impostazioni.

## Schede

| Scheda | Stato |
|---|---|
| Elecrow ThinkNode M9 (tastiera, schermo 320×240) | provata sull'hardware |
| Heltec WiFi LoRa 32 V4 (OLED, un pulsante) | provata sull'hardware |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; niente Wi-Fi) | provata sull'hardware (sul nostro esemplare il modulo GPS non è montato) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, TFT a colori 240×135, un pulsante; niente Wi-Fi) | provata sull'hardware — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | solo build, non ancora provate sull'hardware — segnalazioni benvenute |

Le schede ESP32 hanno tutte le funzioni. Il GAT562 (nRF52840) non ha Wi-Fi: niente punto di accesso, radar Wi-Fi,
sensore di movimento né client internet; il telefono si collega tramite l'app Android via Bluetooth o USB.
In compenso ha una tastiera a schermo e gli scacchi sull'OLED. Pin e dettagli: [docs/en/boards.md](docs/en/boards.md).

## Installazione

**Browser:** apri il [sito](https://ikrasnodymov.github.io/meshmesh/#install) in Chrome o
Edge su un computer, scegli la scheda e la lingua del dispositivo, premi **Installa**. L'aggiornamento di MeshMesh
conserva chiave, contatti, impostazioni e cronologia; spunta “Erase device” solo se arrivi da un altro firmware.

**esptool:** scarica lo zip della scheda dal sito ed esegui

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Schede ESP32: `--chip esp32`, bootloader a `0x1000`, `--flash-freq 40m`; dimensione della flash secondo la scheda.)

**GAT562 (nRF52840):** lo stesso pulsante **Installa** sul sito (DFU seriale via Web Serial); una scheda
con un altro firmware richiede prima di premere RESET due volte. Oppure premi RESET due volte — compare un'unità
`GAT562-BOOT` — e copiaci `firmware.uf2` preso dal sito. Aggiornamenti successivi: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Radio predefinita: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — si cambia in Impostazioni → Radio; tutti i nodi
di una rete devono coincidere. Rispetta le norme radio del tuo paese.

## Compilazione

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

App Android: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
L'interfaccia dello schermo si può renderizzare su un computer senza scheda: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Documentazione

Documentazione dettagliata in inglese: [canali](docs/en/channels.md), [compatibilità MeshCore](docs/en/meshcore-migration.md), [schede](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [modalità ripetitore e stanza](docs/en/repeater.md),
[protocollo degli scacchi](docs/en/chess.md), [formato delle mappe](docs/en/maps-format.md), [confronto con MeshCore](docs/en/feature-parity.md),
[verifica sull'hardware](docs/en/verification.md). Gli originali in russo sono in [docs/](docs/), con la guida
completa ai comandi e alle funzioni in [README.ru.md](README.ru.md).

Non ancora verificati: portata LoRa, precisione del GPS a cielo aperto, precisione della bussola, inoltro attraverso un
terzo ripetitore. Non implementati: voce, pianificazione dei percorsi, aggiornamenti OTA.

## Licenza

[MIT](LICENSE). MeshCore incluso (`lib/MeshCore`) mantiene la propria licenza MIT. Dati delle mappe ©
contributori di [OpenStreetMap](https://www.openstreetmap.org/copyright). L'idea del radar segue
il tracker RSSI e gli HUD radar di [Stevee87](https://github.com/Stevee87).
