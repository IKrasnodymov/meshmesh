# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · **日本語** · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**ウェブサイトとブラウザからのワンクリックインストール: https://ikrasnodymov.github.io/meshmesh/**

ESP32 と nRF52 の LoRa デバイス向けの、オフグリッドで使えるメッセージングファームウェアです。
メッセージは無線でデバイスからデバイスへと中継され、インターネットも携帯電話網もサーバーも必要ありません。
無線レイヤーには [MeshCore](https://github.com/meshcore-dev/MeshCore) を採用しているため、MeshMesh のノードは
標準の MeshCore ノードと通信できます。

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="ホーム画面">
<img src="site/img/m9-chat-en.png" width="32%" alt="個人チャット">
<img src="site/img/m9-radar-en.png" width="32%" alt="信号レーダー">
</p>

## 機能

- **チャット** — 個人メッセージと公開メッセージ、✓✓ の配信確認、再送、メッセージごとの経路表示。
- **チャンネル** — MeshCore チャンネルを最大 8 個。#ハッシュタグ、`meshcore://` リンクや QR コード、名前と鍵で参加できます。
  非公開チャンネルを作成し、個人メッセージで連絡先を招待し、電波で受信したチャンネルを見つけられます。[docs/en/channels.md](docs/en/channels.md)
- **オフライン地図** — SD カード上の OpenStreetMap タイル、地図上の GPS 位置とノード。
- **周辺のノード** — 信号強度、ホップ数、距離と方位。連絡先、リピーター、ルーム。
- **信号レーダー** — 周囲の Wi-Fi、Bluetooth、LoRa を表示し、「近い / 遠い」で発信源を探せます。
- **モーションセンサー** — 2 台のボードの間を人が歩くと検知します (Wi-Fi CSI)。
- メッシュ経由で連絡先と遊べる**チェス**、M9 ではクロンダイク・ソリティアも。
- **メッシュペット**: すべてのボードで暮らすたまごっち風のピクセル生物。無線のトラフィックを食べて成長し、空腹や孤独で死ぬこともあります。 [docs/en/pet.md](docs/en/pet.md)
- **デバイスに MeshMesh がなくてもチェス**: [チェスページ](https://ikrasnodymov.github.io/meshmesh/chess/) は、
  公式の MeshCore Companion ファームウェアを載せたボードを通じて、USB (Web Serial) または Bluetooth (Web Bluetooth) 経由で、Chrome または Edge で対局できます。
- **リピーターとルームサーバーのモード** — 起動時 (M9 の画面、Heltec のボタン) または設定、
  ウェブページ、USB から選択します。デバイスは標準互換の MeshCore リピーターまたはルームサーバーになり、
  MeshCore アプリから管理者ログインとリモート CLI が使えます。鍵、連絡先、履歴はそのまま保持されます。
  [docs/en/repeater.md](docs/en/repeater.md)
- **GPS とコンパス**、位置の共有、ロック画面、フォネティック配列によるキリル文字のキーボード入力。
- デバイス自身の Wi-Fi アクセスポイントまたは接続先の家庭用 Wi-Fi (M9、T-Deck) 経由の**ウェブインターフェース**と、メッセージ通知付きの
  **Android アプリ** (Wi-Fi、Bluetooth LE、USB)。
- **デバイス UI は 15 言語に対応**: 英語、ロシア語、ウクライナ語、スペイン語、ポルトガル語、フランス語、ドイツ語、イタリア語、
  ポーランド語、トルコ語、中国語、日本語、韓国語、アラビア語、インドネシア語。ウェブサイトから選んだ言語で
  ファームウェアをインストールでき、後から設定で変更できます。

## ボード

| ボード | 状態 |
|---|---|
| Elecrow ThinkNode M9 (キーボード、320×240 画面) | 実機で検証済み |
| Heltec WiFi LoRa 32 V4 (OLED、ボタン 1 つ) | 実機で検証済み |
| GAT562 30S Mesh Kit (nRF52840、OLED、ジョイスティック、Wi-Fi なし) | 実機で検証済み (手元の個体には GPS モジュールが未搭載) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840、240×135 カラー TFT、ボタン 1 つ、Wi-Fi なし) | 実機で検証済み — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | ビルドのみ確認、実機では未動作 — 報告を歓迎します |

ESP32 ボードではすべての機能が使えます。GAT562 (nRF52840) には Wi-Fi がないため、アクセスポイント、Wi-Fi レーダー、
モーションセンサー、インターネットクライアントは使えません。スマートフォンは Android アプリから Bluetooth または USB で接続します。
その代わり、画面上キーボードと OLED でのチェスが加わります。ピン配置と詳細: [docs/en/boards.md](docs/en/boards.md)。

## インストール

**ブラウザ:** パソコンの Chrome または Edge で[ウェブサイト](https://ikrasnodymov.github.io/meshmesh/#install)を開き、
ボードとデバイスの言語を選んで **インストール** を押します。MeshMesh の更新では鍵、連絡先、設定、履歴が保持されます。
「Erase device」にチェックを入れるのは、他のファームウェアから移行する場合だけにしてください。

**esptool:** ウェブサイトからボードの zip をダウンロードして、次を実行します

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(ESP32 ボード: `--chip esp32`、ブートローダーは `0x1000`、`--flash-freq 40m`。フラッシュサイズはボードごとに異なります。)

**GAT562 (nRF52840):** ウェブサイトの同じ **インストール** ボタンを使います (Web Serial によるシリアル DFU)。他のファームウェアが
入っているボードでは、先に RESET を 2 回押す必要があります。または RESET を 2 回押して `GAT562-BOOT` ドライブを
表示させ、ウェブサイトから入手した `firmware.uf2` をそこにコピーします。以降の更新: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md))。

既定の無線設定: 868.731 MHz、BW 62.5 kHz、SF8、CR4/6 — 設定 → 無線 で変更できます。ネットワーク内の
すべてのノードで一致させる必要があります。お住まいの国の電波法規に従ってください。

## ビルド

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Android アプリ: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md))。
画面 UI はボードなしでパソコン上に描画できます: `tools/ui_preview/build.sh m9 OUTDIR en`。

## ドキュメント

英語の詳細なドキュメント: [チャンネル](docs/en/channels.md)、[MeshCore との互換性](docs/en/meshcore-migration.md)、[ボード](docs/en/boards.md)、
[GAT562](docs/en/gat562.md)、[Android](docs/en/android.md)、[リピーターとルームのモード](docs/en/repeater.md)、
[チェスのプロトコル](docs/en/chess.md)、[地図フォーマット](docs/en/maps-format.md)、[MeshCore との比較](docs/en/feature-parity.md)、
[ハードウェア検証](docs/en/verification.md)。ロシア語の原文は [docs/](docs/) にあり、操作と機能の完全な
ガイドは [README.ru.md](README.ru.md) にあります。

未検証: LoRa の到達距離、屋外での GPS 精度、コンパスの精度、3 台目のリピーターを経由した中継。
未実装: 音声、経路計画、OTA 更新。

## ライセンス

[MIT](LICENSE)。同梱の MeshCore (`lib/MeshCore`) は独自の MIT ライセンスを維持しています。地図データ ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) contributors。レーダーのアイデアは
[Stevee87](https://github.com/Stevee87) による RSSI トラッカーとレーダー HUD を参考にしています。
