# MeshMesh

[English](README.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · **العربية** · [Bahasa Indonesia](README.id.md)

<div dir="rtl">

**الموقع والتثبيت من المتصفح بنقرة واحدة: https://ikrasnodymov.github.io/meshmesh/**

برنامج ثابت للمراسلة دون شبكة لأجهزة LoRa المبنية على ESP32 وnRF52. تنتقل الرسائل لاسلكيا من جهاز إلى
آخر، دون إنترنت ودون شبكة هاتف محمول ودون خوادم. الطبقة اللاسلكية هي
[MeshCore](https://github.com/meshcore-dev/MeshCore)، لذا تتواصل عقد MeshMesh مع عقد
MeshCore القياسية.

</div>

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="الشاشة الرئيسية">
<img src="site/img/m9-chat-en.png" width="32%" alt="محادثة خاصة">
<img src="site/img/m9-radar-en.png" width="32%" alt="رادار الإشارات">
</p>

<div dir="rtl">

## الميزات

- **المحادثات**: رسائل خاصة وعامة، وإشعارات تسليم ✓✓، وإعادة المحاولة، وعرض المسار لكل رسالة.
- **خرائط دون اتصال**: مربعات OpenStreetMap على بطاقة SD، والموقع من GPS والعقد على الخريطة.
- **العقد القريبة**: قوة الإشارة وعدد القفزات والمسافة والاتجاه؛ جهات الاتصال والمرحلات والغرف.
- **رادار الإشارات**: Wi-Fi وBluetooth وLoRa من حولك، مع توجيه «أدفأ / أبرد» للوصول إلى المصدر.
- **مستشعر الحركة**: لوحتان تكشفان شخصا يمشي بينهما (Wi-Fi CSI).
- **الشطرنج** مع جهات اتصالك عبر الشبكة، ولعبة السوليتير Klondike على M9.
- **الشطرنج دون MeshMesh على جهازك**: [صفحة الشطرنج](https://ikrasnodymov.github.io/meshmesh/chess/) تلعب عبر
  لوحة تعمل ببرنامج MeshCore Companion الرسمي، عبر USB ‏(Web Serial) أو Bluetooth ‏(Web Bluetooth)، في Chrome أو Edge.
- **وضعا المرحل وخادم الغرفة**: يختاران عند الإقلاع (شاشة M9، زر Heltec) أو من الإعدادات،
  أو من صفحة الويب، أو عبر USB: يصبح الجهاز مرحل MeshCore أو خادم غرفة متوافقا مع البرنامج القياسي،
  مع تسجيل دخول المسؤول وواجهة أوامر عن بعد من تطبيق MeshCore؛ ويبقى المفتاح وجهات الاتصال والسجل.
  [docs/en/repeater.md](docs/en/repeater.md)
- **GPS والبوصلة**، ومشاركة الموقع، وشاشة القفل، وكتابة الحروف السيريلية بتخطيط لوحة مفاتيح يطابق لفظ الحروف اللاتينية.
- **واجهة ويب** عبر نقطة وصول Wi-Fi الخاصة بالجهاز، و**تطبيق Android**
  ‏(Wi-Fi أو Bluetooth LE أو USB) مع إشعارات الرسائل.
- **واجهة الجهاز بـ 15 لغة**: الإنجليزية والروسية والأوكرانية والإسبانية والبرتغالية والفرنسية والألمانية والإيطالية
  والبولندية والتركية والصينية واليابانية والكورية والعربية والإندونيسية. يثبت الموقع البرنامج الثابت باللغة
  التي تختارها، ويمكن تغييرها لاحقا من الإعدادات.

## اللوحات

| اللوحة | الحالة |
|---|---|
| Elecrow ThinkNode M9 (لوحة مفاتيح، شاشة 320×240) | مختبرة على الجهاز |
| Heltec WiFi LoRa 32 V4 ‏(OLED، زر واحد) | مختبرة على الجهاز |
| GAT562 30S Mesh Kit ‏(nRF52840، OLED، عصا تحكم؛ دون Wi-Fi) | مختبرة على الجهاز (لا توجد وحدة GPS في نسختنا) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | تبنى فقط ولم تشغل بعد على الجهاز — نرحب بالتقارير |

لوحات ESP32 تدعم كل الميزات. أما GAT562 ‏(nRF52840) فلا تحتوي على Wi-Fi: لا نقطة وصول ولا رادار Wi-Fi
ولا مستشعر حركة ولا عميل إنترنت؛ ويتصل الهاتف عبر تطبيق Android باستخدام Bluetooth أو USB.
وتضيف لوحة مفاتيح على الشاشة والشطرنج على شاشة OLED. المنافذ والتفاصيل: [docs/en/boards.md](docs/en/boards.md).

## التثبيت

**من المتصفح:** افتح [الموقع](https://ikrasnodymov.github.io/meshmesh/#install) في Chrome أو
Edge على حاسوب، واختر لوحتك ولغة الجهاز، ثم اضغط **تثبيت**. تحديث MeshMesh
يحافظ على مفتاحك وجهات اتصالك وإعداداتك وسجلك؛ لا تحدد “Erase device” إلا عند الانتقال من برنامج ثابت آخر.

**esptool:** قم بتنزيل ملف zip الخاص باللوحة من الموقع ونفذ

</div>

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

<div dir="rtl">

(لوحات ESP32: `--chip esp32`، ومحمل الإقلاع عند `0x1000`، و`--flash-freq 40m`؛ وحجم الذاكرة حسب اللوحة.)

**GAT562 ‏(nRF52840):** زر **تثبيت** نفسه على الموقع (DFU تسلسلي عبر Web Serial)؛ اللوحة
التي تعمل ببرنامج ثابت آخر تحتاج أولا إلى الضغط على RESET مرتين. أو اضغط RESET مرتين فيظهر قرص `GAT562-BOOT`،
ثم انسخ إليه `firmware.uf2` من الموقع. التحديثات اللاحقة: `python tools/nrf52.py flash PACKAGE` ‏([docs/en/gat562.md](docs/en/gat562.md)).

الإعدادات اللاسلكية الافتراضية: 868.731 MHz، BW 62.5 kHz، SF8، CR4/6. يمكن تغييرها من الإعدادات ← راديو؛ ويجب أن تتطابق
في كل عقد الشبكة. التزم بأنظمة الاتصالات اللاسلكية في بلدك.

## البناء

</div>

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

<div dir="rtl">

تطبيق Android: `cd android && ./gradlew testDebugUnitTest assembleRelease` ‏([docs/en/android.md](docs/en/android.md)).
يمكن عرض واجهة الشاشة على حاسوب دون لوحة: `tools/ui_preview/build.sh m9 OUTDIR en`.

## التوثيق

توثيق مفصل بالإنجليزية: [التوافق مع MeshCore](docs/en/meshcore-migration.md)، [اللوحات](docs/en/boards.md)،
[GAT562](docs/en/gat562.md)، [Android](docs/en/android.md)، [وضعا المرحل والغرفة](docs/en/repeater.md)،
[بروتوكول الشطرنج](docs/en/chess.md)، [صيغة الخرائط](docs/en/maps-format.md)، [مقارنة مع MeshCore](docs/en/feature-parity.md)،
[التحقق على الأجهزة](docs/en/verification.md). النصوص الروسية الأصلية في [docs/](docs/)، والدليل الكامل
للتحكم والميزات في [README.ru.md](README.ru.md).

لم يتحقق منه بعد: مدى LoRa، ودقة GPS تحت السماء المفتوحة، ودقة البوصلة، والترحيل عبر
مرحل ثالث. غير منفذ: الصوت، وتخطيط المسارات، وتحديثات OTA.

## الترخيص

[MIT](LICENSE). تحتفظ مكتبة MeshCore المضمنة (`lib/MeshCore`) بترخيص MIT الخاص بها. بيانات الخرائط ©
مساهمو [OpenStreetMap](https://www.openstreetmap.org/copyright). فكرة الرادار مستوحاة من
متتبع RSSI وواجهات الرادار التي صممها [Stevee87](https://github.com/Stevee87).

</div>
