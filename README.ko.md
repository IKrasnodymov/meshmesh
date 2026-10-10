# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · **한국어** · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**웹사이트와 브라우저에서 원클릭 설치: https://ikrasnodymov.github.io/meshmesh/**

ESP32·nRF52 LoRa 장치를 위한 오프그리드 메시징 펌웨어입니다. 메시지는 무선으로 장치에서 장치로
건너가며, 인터넷도 이동통신망도 서버도 필요 없습니다. 무선 계층은
[MeshCore](https://github.com/meshcore-dev/MeshCore)이므로 MeshMesh 노드는 일반
MeshCore 노드와 통신합니다.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="홈 화면">
<img src="site/img/m9-chat-en.png" width="32%" alt="개인 채팅">
<img src="site/img/m9-radar-en.png" width="32%" alt="신호 레이더">
</p>

## 기능

- **채팅** — 개인 메시지와 공개 메시지, ✓✓ 전달 확인, 재전송, 메시지별 경로 표시.
- **채널** — 최대 8개의 MeshCore 채널: #해시태그, `meshcore://` 링크나 QR 코드, 이름과 키로 참여합니다.
  비공개 채널을 만들고, 개인 메시지로 연락처를 초대하고, 전파에서 수신된 채널을 찾을 수 있습니다. [docs/en/channels.md](docs/en/channels.md)
- **지역** — MeshCore flood 범위: 기본 지역과 채널별 지역, 주변 리피터가 맡은 지역 검색; 리피터는 `region` 명령으로 설정. [docs/en/regions.md](docs/en/regions.md)
- **오프라인 지도** — SD 카드의 OpenStreetMap 타일, 지도 위의 GPS 위치와 노드.
- **워드라이빙** — 메시 커버리지 지도: 수신한 패킷과 채널 핑을 이를 전달한 리피터와 함께, 신뢰할 수 있는 GPS 또는 휴대폰 위치로 기록합니다. ESP32에서는 Wi-Fi와 Bluetooth도 기록하며 CSV, GeoJSON, KML, WiGLE로 내보냅니다. [docs/en/wardrive.md](docs/en/wardrive.md)
- **주변 노드** — 신호, 홉, 거리, 방향; 연락처, 리피터, 룸.
- **신호 레이더** — 주변의 Wi-Fi, Bluetooth, LoRa를 보여 주고 “가까워짐 / 멀어짐” 방식으로 추적합니다.
- **움직임 센서** — 두 보드가 그 사이를 지나가는 사람을 감지합니다(Wi-Fi CSI).
- 메시를 통해 연락처와 두는 **체스**, M9에서 즐기는 클론다이크 솔리테어.
- **메시 펫**: 모든 보드에 사는 다마고치 같은 픽셀 생물. 무선 트래픽을 먹고 자라며, 배고픔이나 외로움으로 죽을 수도 있습니다. [docs/en/pet.md](docs/en/pet.md)
- **주사위**: 모든 보드에서 쓰는 주사위 굴리기 DIC3R. RPG 주사위 풀과 수식(2d6+1, d20, d66, d%), 성공 기준이 있는 Warhammer 격자, 카운터와 캐릭터를 화면, 웹 페이지, 앱에서 똑같이 씁니다. [docs/en/dice.md](docs/en/dice.md)
- **장치에 MeshMesh가 없어도 체스**: [체스 페이지](https://ikrasnodymov.github.io/meshmesh/chess/)가
  공식 MeshCore Companion 펌웨어를 실행하는 보드를 통해 USB(Web Serial) 또는 Bluetooth(Web Bluetooth)로 대국합니다. Chrome 또는 Edge에서 작동합니다.
- **리피터 모드와 룸 서버 모드** — 부팅할 때(M9 화면, Heltec 버튼) 또는 설정, 웹 페이지, USB에서
  고릅니다. 장치는 기본 MeshCore와 호환되는 리피터나 룸 서버가 되며, MeshCore 앱에서 관리자 로그인과
  원격 CLI를 쓸 수 있습니다. 키, 연락처, 기록은 그대로 유지됩니다.
  [docs/en/repeater.md](docs/en/repeater.md)
- **MeshCore 앱** — 공식 MeshCore 앱이 Bluetooth 또는 USB로 연결되어 보드의 채팅, 연락처, 채널을 사용합니다(체스는 MeshMesh 앱에서). [docs/en/companion.md](docs/en/companion.md)
- **GPS와 나침반**, 위치 공유, 잠금 화면, 발음식 키릴 문자 키보드 입력.
- 장치 자체 Wi-Fi 액세스 포인트나 장치가 접속한 집 Wi-Fi(M9, T-Deck)로 여는 **웹 인터페이스**, 메시지 알림을 지원하는 **Android 앱**
  (Wi-Fi, Bluetooth LE 또는 USB).
- **15개 언어의 장치 UI**: 영어, 러시아어, 우크라이나어, 스페인어, 포르투갈어, 프랑스어, 독일어, 이탈리아어,
  폴란드어, 튀르키예어, 중국어, 일본어, 한국어, 아랍어, 인도네시아어. 웹사이트에서 고른 언어로 펌웨어가
  설치되며, 나중에 설정에서 바꿀 수 있습니다.

## 보드

| 보드 | 상태 |
|---|---|
| Elecrow ThinkNode M9 (키보드, 320×240 화면) | 하드웨어에서 검증됨 |
| Heltec WiFi LoRa 32 V4 (OLED, 버튼 하나) | 하드웨어에서 검증됨 |
| GAT562 30S Mesh Kit (nRF52840, OLED, 조이스틱; Wi-Fi 없음) | 하드웨어에서 검증됨(우리 기기에는 GPS 모듈이 장착되어 있지 않음) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, 240×135 컬러 TFT, 버튼 하나; Wi-Fi 없음) | 하드웨어에서 검증됨 — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | 빌드만 확인, 아직 하드웨어에서 실행해 보지 않음 — 사용 후기 환영 |

ESP32 보드에서는 모든 기능을 쓸 수 있습니다. GAT562(nRF52840)에는 Wi-Fi가 없어 액세스 포인트, Wi-Fi 레이더,
움직임 센서, 인터넷 클라이언트가 없으며, 휴대폰은 Bluetooth 또는 USB로 Android 앱을 통해 연결합니다.
대신 화면 키보드와 OLED 체스가 추가됩니다. 핀과 세부 사항: [docs/en/boards.md](docs/en/boards.md).

## 설치

**브라우저:** 컴퓨터의 Chrome 또는 Edge에서 [웹사이트](https://ikrasnodymov.github.io/meshmesh/#install)를 열고,
보드와 장치 언어를 고른 뒤 **설치**를 누르세요. MeshMesh를 업데이트해도 키, 연락처, 설정, 기록이
유지됩니다. 다른 펌웨어에서 옮겨 올 때만 “Erase device”를 선택하세요.

**esptool:** 웹사이트에서 보드의 zip을 내려받고 다음을 실행하세요.

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(ESP32 보드: `--chip esp32`, 부트로더는 `0x1000`, `--flash-freq 40m`; 플래시 크기는 보드마다 다릅니다.)

**GAT562 (nRF52840):** 웹사이트의 같은 **설치** 버튼을 쓰세요(Web Serial을 통한 시리얼 DFU). 다른
펌웨어가 설치된 보드는 먼저 RESET을 두 번 눌러야 합니다. 또는 RESET을 두 번 눌러 `GAT562-BOOT` 드라이브가
나타나면 웹사이트에서 받은 `firmware.uf2`를 복사하세요. 이후 업데이트: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

기본 무선 설정: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — 설정 → 무선에서 바꿀 수 있으며, 한 네트워크의
모든 노드는 설정이 같아야 합니다. 거주 국가의 전파 관련 규정을 지키세요.

## 빌드

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Android 앱: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
화면 UI는 보드 없이 컴퓨터에서 렌더링할 수 있습니다: `tools/ui_preview/build.sh m9 OUTDIR en`.

## 문서

영어로 된 자세한 문서: [채널](docs/en/channels.md), [지역](docs/en/regions.md), [MeshCore 호환성](docs/en/meshcore-migration.md), [보드](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [리피터와 룸 모드](docs/en/repeater.md),
[체스 프로토콜](docs/en/chess.md), [지도 형식](docs/en/maps-format.md), [MeshCore와 비교](docs/en/feature-parity.md),
[하드웨어 검증](docs/en/verification.md). 러시아어 원문은 [docs/](docs/)에 있으며, 조작법과 기능 전체 안내는
[README.ru.md](README.ru.md)에 있습니다.

아직 검증되지 않음: LoRa 통신 거리, 탁 트인 하늘 아래 GPS 정확도, 나침반 정확도, 세 번째 리피터를 거친
중계. 구현되지 않음: 음성, 경로 계획, OTA 업데이트.

## 라이선스

[MIT](LICENSE). 함께 포함된 MeshCore(`lib/MeshCore`)는 자체 MIT 라이선스를 따릅니다. 지도 데이터 ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) 기여자. 레이더 아이디어는
[Stevee87](https://github.com/Stevee87)의 RSSI 트래커와 레이더 HUD에서 왔습니다.

[알림, 빠른 전송, 채널 기록, 주기적 NTP와 사람 카운터](docs/en/notifications.md).
