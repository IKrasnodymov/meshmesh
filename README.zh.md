# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · **中文** · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**网站与浏览器一键安装：https://ikrasnodymov.github.io/meshmesh/**

面向 ESP32 和 nRF52 LoRa 设备的离网通信固件。消息通过无线电在设备之间逐跳传递，无需互联网、
手机网络或服务器。无线电层采用 [MeshCore](https://github.com/meshcore-dev/MeshCore)，因此 MeshMesh
节点可以与标准 MeshCore 节点互通。

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="主屏">
<img src="site/img/m9-chat-en.png" width="32%" alt="私聊">
<img src="site/img/m9-radar-en.png" width="32%" alt="信号雷达">
</p>

## 功能

- **聊天**——私聊和公开消息，✓✓ 送达回执，自动重发，每条消息显示路由。
- **离线地图**——SD 卡上的 OpenStreetMap 瓦片，在地图上显示 GPS 位置和节点。
- **附近节点**——信号、跳数、距离和方位；联系人、中继器和房间。
- **信号雷达**——扫描周围的 Wi-Fi、Bluetooth 和 LoRa，支持“变强 / 变弱”测向。
- **移动传感器**——两块开发板可检测到有人从它们之间走过（Wi-Fi CSI）。
- 通过 Mesh 网络与联系人下**国际象棋**，M9 上还有 Klondike 纸牌接龙。
- **设备上无需 MeshMesh 也能下国际象棋**：[国际象棋页面](https://ikrasnodymov.github.io/meshmesh/chess/)通过运行官方
  MeshCore Companion 固件的开发板对弈，经 USB（Web Serial）或 Bluetooth（Web Bluetooth）连接，使用 Chrome 或 Edge。
- **中继器和房间服务器模式**——在启动时（M9 屏幕、Heltec 按键）或在设置、网页、USB 中选择：
  设备会成为与原版兼容的 MeshCore 中继器或房间服务器，可通过 MeshCore 应用进行管理员登录和远程 CLI；
  密钥、联系人和历史记录都会保留。[docs/en/repeater.md](docs/en/repeater.md)
- **GPS 和指南针**、位置共享、锁屏、西里尔字母音译键盘输入。
- 通过设备自带的 Wi-Fi 热点或其接入的家庭 Wi-Fi（M9、T-Deck）访问的**网页界面**，以及带消息通知的 **Android 应用**
  （Wi-Fi、Bluetooth LE 或 USB）。
- **设备界面支持 15 种语言**：英语、俄语、乌克兰语、西班牙语、葡萄牙语、法语、德语、意大利语、
  波兰语、土耳其语、中文、日语、韩语、阿拉伯语和印尼语。网站会按你选择的语言安装固件，
  之后可在设置中更改。

## 开发板

| 开发板 | 状态 |
|---|---|
| Elecrow ThinkNode M9（键盘，320×240 屏幕） | 已在硬件上测试 |
| Heltec WiFi LoRa 32 V4（OLED，单按键） | 已在硬件上测试 |
| GAT562 30S Mesh Kit（nRF52840，OLED，摇杆；无 Wi-Fi） | 已在硬件上测试（我们的样机未安装 GPS 模块）—— [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | 仅完成构建，尚未在硬件上运行——欢迎反馈 |

ESP32 开发板具备全部功能。GAT562（nRF52840）没有 Wi-Fi：没有热点、Wi-Fi 雷达、移动传感器和
互联网客户端；手机通过 Android 应用经 Bluetooth 或 USB 连接。它另有屏幕键盘，并可在 OLED 上下国际象棋。
引脚和细节：[docs/en/boards.md](docs/en/boards.md)。

## 安装

**浏览器**：在电脑上用 Chrome 或 Edge 打开[网站](https://ikrasnodymov.github.io/meshmesh/#install)，
选择开发板和设备语言，点击**安装**。更新 MeshMesh 会保留密钥、联系人、设置和历史记录；
只有从其他固件改装时才勾选 “Erase device”。

**esptool**：从网站下载对应开发板的 zip 并运行

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

（ESP32 开发板：`--chip esp32`，bootloader 位于 `0x1000`，`--flash-freq 40m`；flash 大小视开发板而定。）

**GAT562（nRF52840）**：使用网站上同一个**安装**按钮（通过 Web Serial 进行串口 DFU）；
装有其他固件的开发板需要先按两次 RESET。也可以按两次 RESET——会出现 `GAT562-BOOT` 磁盘——
然后把网站上的 `firmware.uf2` 复制进去。之后的更新：`python tools/nrf52.py flash PACKAGE`（[docs/en/gat562.md](docs/en/gat562.md)）。

默认无线电参数：868.731 MHz，BW 62.5 kHz，SF8，CR4/6——可在“设置 → 无线电”中修改；同一网络中的
所有节点必须一致。请遵守所在国家的无线电管理规定。

## 构建

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Android 应用：`cd android && ./gradlew testDebugUnitTest assembleRelease`（[docs/en/android.md](docs/en/android.md)）。
屏幕界面可以在没有开发板的电脑上渲染：`tools/ui_preview/build.sh m9 OUTDIR en`。

## 文档

英文详细文档：[MeshCore 兼容性](docs/en/meshcore-migration.md)、[开发板](docs/en/boards.md)、
[GAT562](docs/en/gat562.md)、[Android](docs/en/android.md)、[中继器和房间模式](docs/en/repeater.md)、
[国际象棋协议](docs/en/chess.md)、[地图格式](docs/en/maps-format.md)、[与 MeshCore 的对比](docs/en/feature-parity.md)、
[硬件验证](docs/en/verification.md)。俄文原版位于 [docs/](docs/)，完整的操作与功能指南见
[README.ru.md](README.ru.md)。

尚未验证：LoRa 通信距离、开阔天空下的 GPS 精度、指南针精度、经第三个中继器的转发。
尚未实现：语音、路线规划、OTA 更新。

## 许可证

[MIT](LICENSE)。内置的 MeshCore（`lib/MeshCore`）保留其自身的 MIT 许可证。地图数据 ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) 贡献者。雷达的设计思路借鉴了
[Stevee87](https://github.com/Stevee87) 的 RSSI 追踪器和雷达 HUD。
