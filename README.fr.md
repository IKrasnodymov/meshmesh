# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · **Français** · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Site web et installation en un clic depuis le navigateur : https://ikrasnodymov.github.io/meshmesh/**

Firmware de messagerie hors réseau pour appareils LoRa ESP32 et nRF52. Les messages passent par radio
d'un appareil à l'autre, sans Internet, sans réseau mobile et sans serveurs. La couche radio est
[MeshCore](https://github.com/meshcore-dev/MeshCore) : les nœuds MeshMesh communiquent donc avec les
nœuds MeshCore standard.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Écran d'accueil">
<img src="site/img/m9-chat-en.png" width="32%" alt="Chat privé">
<img src="site/img/m9-radar-en.png" width="32%" alt="Radar de signaux">
</p>

## Fonctions

- **Chats** — messages privés et publics, accusés de remise ✓✓, renvois, route affichée pour chaque message.
- **Canaux** — jusqu'à 8 canaux MeshCore : rejoindre par #hashtag, lien `meshcore://` ou QR code, nom et clé ;
  créer un canal privé, inviter des contacts par message privé, trouver les canaux captés sur les ondes. [docs/en/channels.md](docs/en/channels.md)
- **Régions** — portée des floods MeshCore : région par défaut et une par canal, recherche des régions des répéteurs proches ; répéteurs réglés par les commandes `region`. [docs/en/regions.md](docs/en/regions.md)
- **Cartes hors ligne** — tuiles OpenStreetMap sur la carte SD, position GPS et nœuds sur la carte.
- **Nœuds à proximité** — signal, sauts, distance et cap ; contacts, répéteurs et salons.
- **Radar de signaux** — Wi-Fi, Bluetooth et LoRa autour de vous, avec un pistage « chaud / froid ».
- **Détecteur de mouvement** — deux cartes détectent une personne qui passe entre elles (Wi-Fi CSI).
- **Échecs** avec vos contacts à travers le maillage, et solitaire Klondike sur le M9.
- **Animal du réseau** — une créature en pixels façon Tamagotchi sur toutes les cartes : elle se nourrit du trafic radio, grandit et peut mourir de faim ou de solitude. [docs/en/pet.md](docs/en/pet.md)
- **Dés** — le lanceur de dés DIC3R sur toutes les cartes : réserves et formules de JdR (2d6+1, d20, d66, d%), une grille Warhammer avec seuil de réussite, compteurs et personnages, identiques sur l’écran, la page web et l’appli. [docs/en/dice.md](docs/en/dice.md)
- **Échecs sans MeshMesh sur votre appareil** : [la page d'échecs](https://ikrasnodymov.github.io/meshmesh/chess/) joue via une
  carte équipée du firmware officiel MeshCore Companion, en USB (Web Serial) ou Bluetooth (Web Bluetooth), dans Chrome ou Edge.
- **Modes répéteur et serveur de salon** — choisis au démarrage (écran du M9, bouton du Heltec) ou dans les Réglages,
  sur la page web ou en USB : l'appareil devient un répéteur ou un serveur de salon MeshCore compatible avec le firmware d'origine,
  avec connexion admin et CLI à distance depuis l'appli MeshCore ; la clé, les contacts et l'historique sont conservés.
  [docs/en/repeater.md](docs/en/repeater.md)
- **Applis MeshCore** — les applis MeshCore officielles se connectent en Bluetooth ou USB et utilisent les discussions, contacts et canaux de la carte (les échecs restent dans l'appli MeshMesh). [docs/en/companion.md](docs/en/companion.md)
- **GPS et boussole**, partage de position, écran de verrouillage, saisie phonétique du cyrillique au clavier.
- **Interface web** via le point d'accès Wi-Fi de l'appareil lui-même ou le réseau Wi-Fi domestique qu'il rejoint (M9, T-Deck), et une **appli Android**
  (Wi-Fi, Bluetooth LE ou USB) avec notifications de messages.
- **Interface de l'appareil en 15 langues** : anglais, russe, ukrainien, espagnol, portugais, français, allemand, italien,
  polonais, turc, chinois, japonais, coréen, arabe et indonésien. Le site installe le firmware dans
  la langue choisie ; elle se change ensuite dans les Réglages.

## Cartes

| Carte | État |
|---|---|
| Elecrow ThinkNode M9 (clavier, écran 320×240) | testée sur le matériel |
| Heltec WiFi LoRa 32 V4 (OLED, un bouton) | testée sur le matériel |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick ; sans Wi-Fi) | testée sur le matériel (notre exemplaire n'a pas de module GPS) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, TFT couleur 240×135, un bouton ; sans Wi-Fi) | testée sur le matériel — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | compilation seulement, pas encore lancée sur le matériel — vos retours sont les bienvenus |

Les cartes ESP32 ont toutes les fonctions. La GAT562 (nRF52840) n'a pas de Wi-Fi : ni point d'accès, ni radar Wi-Fi,
ni détecteur de mouvement, ni client Internet ; le téléphone se connecte via l'appli Android en Bluetooth ou USB.
Elle ajoute un clavier virtuel et les échecs sur l'OLED. Broches et détails : [docs/en/boards.md](docs/en/boards.md).

## Installation

**Navigateur :** ouvrez le [site web](https://ikrasnodymov.github.io/meshmesh/#install) dans Chrome ou
Edge sur un ordinateur, choisissez votre carte et la langue de l'appareil, puis appuyez sur **Installer**. La mise à jour de MeshMesh
conserve votre clé, vos contacts, vos réglages et l'historique ; cochez « Erase device » uniquement si vous venez d'un autre firmware.

**esptool :** téléchargez le zip de la carte depuis le site web et lancez

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Cartes ESP32 : `--chip esp32`, bootloader à `0x1000`, `--flash-freq 40m` ; taille de flash selon la carte.)

**GAT562 (nRF52840) :** le même bouton **Installer** sur le site web (DFU série via Web Serial) ; une carte
dotée d'un autre firmware demande d'abord deux appuis sur RESET. Ou bien appuyez deux fois sur RESET — un lecteur `GAT562-BOOT`
apparaît — et copiez-y le fichier `firmware.uf2` du site web. Mises à jour suivantes : `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Radio par défaut : 868,731 MHz, BW 62,5 kHz, SF8, CR4/6 — modifiable dans Réglages → Radio ; tous les nœuds
d'un réseau doivent concorder. Respectez la réglementation radio de votre pays.

## Compilation

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Appli Android : `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
L'interface de l'écran peut être rendue sur un ordinateur sans carte : `tools/ui_preview/build.sh m9 OUTDIR en`.

## Documentation

Documentation détaillée en anglais : [canaux](docs/en/channels.md), [régions](docs/en/regions.md), [compatibilité MeshCore](docs/en/meshcore-migration.md), [cartes](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [modes répéteur et salon](docs/en/repeater.md),
[protocole d'échecs](docs/en/chess.md), [format des cartes](docs/en/maps-format.md), [comparaison avec MeshCore](docs/en/feature-parity.md),
[vérification matérielle](docs/en/verification.md). Les originaux russes se trouvent dans [docs/](docs/), avec le guide
complet des commandes et des fonctions dans [README.ru.md](README.ru.md).

Pas encore vérifié : portée LoRa, précision du GPS en plein ciel, précision de la boussole, relais via un
troisième répéteur. Non implémenté : voix, calcul d'itinéraire, mises à jour OTA.

## Licence

[MIT](LICENSE). MeshCore intégré (`lib/MeshCore`) conserve sa propre licence MIT. Données cartographiques ©
contributeurs [OpenStreetMap](https://www.openstreetmap.org/copyright). L'idée du radar s'inspire
du traceur RSSI et des HUD radar de [Stevee87](https://github.com/Stevee87).
