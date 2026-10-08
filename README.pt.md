# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · **Português** · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Site e instalação pelo navegador com um clique: https://ikrasnodymov.github.io/meshmesh/**

Firmware de mensagens fora da rede para aparelhos LoRa com ESP32 e nRF52. As mensagens saltam por
rádio de aparelho em aparelho, sem internet, sem rede celular e sem servidores. A camada de rádio é o
[MeshCore](https://github.com/meshcore-dev/MeshCore), então os nós MeshMesh conversam com nós
MeshCore padrão.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Tela inicial">
<img src="site/img/m9-chat-en.png" width="32%" alt="Chat direto">
<img src="site/img/m9-radar-en.png" width="32%" alt="Radar de sinais">
</p>

## Recursos

- **Chats** — mensagens diretas e públicas, confirmações de entrega ✓✓, reenvios, rota exibida em cada mensagem.
- **Canais** — até 8 canais MeshCore: entre por #hashtag, link `meshcore://` ou código QR, nome e chave;
  crie um privado, convide contatos por mensagem direta, encontre canais ouvidos no ar. [docs/en/channels.md](docs/en/channels.md)
- **Mapas offline** — blocos do OpenStreetMap no cartão SD, posição GPS e nós no mapa.
- **Nós por perto** — sinal, saltos, distância e direção; contatos, repetidores e salas.
- **Radar de sinais** — Wi-Fi, Bluetooth e LoRa ao seu redor, com rastreio “quente / frio”.
- **Sensor de movimento** — duas placas detectam uma pessoa andando entre elas (Wi-Fi CSI).
- **Xadrez** com seus contatos pela malha, e paciência Klondike no M9.
- **Bichinho da rede** — uma criatura de pixels no estilo Tamagotchi em todas as placas: se alimenta do tráfego de rádio, cresce e pode morrer de fome ou de solidão. [docs/en/pet.md](docs/en/pet.md)
- **Dados** — o rolador de dados DIC3R em todas as placas: paradas e fórmulas de RPG (2d6+1, d20, d66, d%), uma grade de Warhammer com limiar de sucesso, contadores e personagens, iguais na tela, na página web e no app. [docs/en/dice.md](docs/en/dice.md)
- **Xadrez sem MeshMesh no seu aparelho**: [a página de xadrez](https://ikrasnodymov.github.io/meshmesh/chess/) joga por meio de
  uma placa com o firmware oficial MeshCore Companion, via USB (Web Serial) ou Bluetooth (Web Bluetooth), no Chrome ou Edge.
- **Modos repetidor e servidor de sala** — escolhidos ao ligar (tela do M9, botão do Heltec) ou em Configurações,
  na página web ou via USB: o aparelho vira um repetidor ou servidor de sala MeshCore compatível com o padrão,
  com login de administrador e CLI remota pelo app MeshCore; chave, contatos e histórico são mantidos.
  [docs/en/repeater.md](docs/en/repeater.md)
- **Apps do MeshCore** — os apps oficiais do MeshCore se conectam por Bluetooth ou USB e usam os chats, contatos e canais da placa (o xadrez fica no app MeshMesh). [docs/en/companion.md](docs/en/companion.md)
- **GPS e bússola**, compartilhamento de posição, tela de bloqueio, digitação fonética em cirílico.
- **Interface web** pelo próprio ponto de acesso Wi-Fi do aparelho ou pela rede Wi-Fi de casa à qual ele se conecta (M9, T-Deck), e um **app para Android**
  (Wi-Fi, Bluetooth LE ou USB) com notificações de mensagens.
- **Interface do aparelho em 15 idiomas**: inglês, russo, ucraniano, espanhol, português, francês, alemão, italiano,
  polonês, turco, chinês, japonês, coreano, árabe e indonésio. O site instala o firmware com
  o idioma que você escolher; dá para trocá-lo depois em Configurações.

## Placas

| Placa | Situação |
|---|---|
| Elecrow ThinkNode M9 (teclado, tela 320×240) | testada em hardware |
| Heltec WiFi LoRa 32 V4 (OLED, um botão) | testada em hardware |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; sem Wi-Fi) | testada em hardware (nossa unidade não tem módulo GPS instalado) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, TFT a cores 240×135, um botão; sem Wi-Fi) | testada em hardware — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | apenas compilado, ainda não rodou em hardware — relatos são bem-vindos |

As placas ESP32 têm todos os recursos. A GAT562 (nRF52840) não tem Wi-Fi: sem ponto de acesso, radar Wi-Fi,
sensor de movimento nem cliente de internet; o celular se conecta pelo app para Android via Bluetooth ou USB.
Em compensação, ela ganha um teclado na tela e xadrez no OLED. Pinos e detalhes: [docs/en/boards.md](docs/en/boards.md).

## Instalação

**Navegador:** abra o [site](https://ikrasnodymov.github.io/meshmesh/#install) no Chrome ou
Edge em um computador, escolha sua placa e o idioma do aparelho e clique em **Instalar**. Atualizar o MeshMesh
mantém sua chave, contatos, configurações e histórico; marque “Erase device” só quando vier de outro firmware.

**esptool:** baixe o zip da placa no site e execute

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(Placas ESP32: `--chip esp32`, bootloader em `0x1000`, `--flash-freq 40m`; tamanho da flash conforme a placa.)

**GAT562 (nRF52840):** o mesmo botão **Instalar** no site (DFU serial via Web Serial); uma placa
com outro firmware precisa antes de RESET pressionado duas vezes. Ou pressione RESET duas vezes — aparece um disco
`GAT562-BOOT` — e copie para ele o `firmware.uf2` do site. Atualizações seguintes: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Rádio padrão: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — altere em Configurações → Rádio; todos os nós
de uma rede precisam coincidir. Respeite as normas de rádio do seu país.

## Compilação

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

App para Android: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
A interface da tela pode ser renderizada em um computador sem placa: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Documentação

Documentação detalhada em inglês: [canais](docs/en/channels.md), [compatibilidade com MeshCore](docs/en/meshcore-migration.md), [placas](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [modos repetidor e sala](docs/en/repeater.md),
[protocolo do xadrez](docs/en/chess.md), [formato dos mapas](docs/en/maps-format.md), [comparação com o MeshCore](docs/en/feature-parity.md),
[verificação em hardware](docs/en/verification.md). Os originais em russo estão em [docs/](docs/), e o guia completo
de controles e recursos está em [README.ru.md](README.ru.md).

Ainda não verificado: alcance LoRa, precisão do GPS a céu aberto, precisão da bússola, retransmissão por um
terceiro repetidor. Não implementado: voz, planejamento de rotas, atualizações OTA.

## Licença

[MIT](LICENSE). O MeshCore incluído (`lib/MeshCore`) mantém sua própria licença MIT. Dados de mapa ©
colaboradores do [OpenStreetMap](https://www.openstreetmap.org/copyright). A ideia do radar segue
o rastreador RSSI e os HUDs de radar de [Stevee87](https://github.com/Stevee87).
