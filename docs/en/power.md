# Power saving

Since 0.4.0 every board uses shared event waits (`src/Power.cpp`) in normal chat, repeater and
room roles. With the screen off, a ready idle radio and no active radar/CSI or diagnostic probes,
the application task waits up to 20 ms instead of polling every 2 ms. A LoRa interrupt notifies
it without waiting for the timeout. Buttons, GPS and USB are serviced between waits. USB/BLE
commands or buttons retain the fast loop for 1 s, allowing Android's 8 s background polling
interval to leave idle time. Transmission and queued sends retain the fast loop.

This blocks a FreeRTOS task, rather than forcibly powering down the CPU. Other tasks and BLE/Wi-Fi
continue running. Actual CPU sleep depends on the core, drivers, USB and peripherals. ESP32 keeps
its existing 80 MHz and repeater/room light sleep under their original conditions. Clock changes
remain forbidden with active Wi-Fi/Bluetooth. Native USB attached to a computer blocks ESP32
light sleep, but does not block task waits. Light-sleep IRQ handling is unchanged.

`status` fields `idle_waits`, `idle_wait_ms` and `idle_radio_events` count task waits, their elapsed
time and LoRa notifications received while waiting. They do not measure current or CPU sleep.
`sleeps`, `sleep_ms` and `slow_ms` remain separate ESP32 counters. A lit screen and active use
retain the fast loop.

Run `python tools/power_check.py --port PORT --peer PEER_PORT`. It temporarily changes only the
screen timeout, checks real advert reception during idle, USB and simulated UI wake-up, then
restores the setting. `nrf52_power_check.py` remains as a compatibility entry point.
`python tools/power_pair_check.py PORT1 PORT2` checks three direct messages and ACKs in each
direction without receiver USB polling until ACK, then restores the original roles. See `verification.md`
for results tied to particular images and remaining limitations.

## Power off

Since 0.8.0 the device can be switched off in software: "Turn off..." in the Settings menu of the
one-button boards, T114 and GAT562 (confirm by holding again or a second OK), the "Turn off" row in the
M9 and T-Deck settings (OK twice), the "Turn off the device" button in the Settings of the web page and the
app, and the USB command `poweroff`. After 1.5 s (the reply leaves; a packet on air gets up to 5 s more)
the firmware flushes delayed writes (contacts, chess, tournaments, the pet, server logins and posts), shows
"Device is off" and how to turn it on for 3 s, puts the transceiver to sleep, turns the screen, GPS and
peripheral supply off and switches off:

| Board | Off | On |
|---|---|---|
| Heltec V4, V3, Wireless Tracker, T3-S3, T-LoRa, XIAO S3, T-Deck | ESP32 deep sleep, pin levels held | hold PRG/BOOT (T-Deck: the trackball pressed) for about a second |
| Station G2, ThinkNode M2 | ESP32 deep sleep | RESET: the button is not on an RTC pin and cannot wake it |
| ThinkNode M9 | ESP32 deep sleep | RESET or the power slider: the M9 has no button on a GPIO |
| T-Beam, T-Beam Supreme | the AXP PMU cuts every rail (deep sleep without a PMU) | the board's PWR key |
| GAT562, T114 | soft off: the CPU sleeps in System ON waiting for the button, the rest is off | hold the joystick centre / USER until the LED lights (~1 s), then release |

On the ESP32 a short press wakes the board only for a moment: without a 0.7 s hold after the start it goes
back to sleep. RESET always turns it on. The nRF52 does not use System OFF: leaving it is a reset with the
button still held, and the T114 bootloader takes USER held at reset for its Bluetooth OTA update mode
(`HT-n5262-OTA`, no USB). The board stayed there, and a RESET at that moment ended in lost storage
(`verification.md`). So the nRF52 sleeps in System ON (FreeRTOS polls the button every 50 ms, Bluetooth is
silent, the BLE setting is kept) and restarts the ordinary way once the button is released. A switched-off
nRF52 keeps its USB port but does not answer commands. A USB update needs the board on; on the nRF52 the
bootloader is also reachable by a double RESET. On the classic ESP32 (T-Beam, T-LoRa) IRAM is
full and the deep sleep code did not fit: these builds stub out (`src/NoLedc.cpp`) the LEDC calls of
RadioLib's `tone()`, which only AFSK uses. The current while off has not been measured.

[EasySkyMesh](https://github.com/IoTThinks/EasySkyMesh/wiki/PowerSaving#3-test-results) informed this
work. RXPS (duty-cycled LoRa reception) and scheduled GPS power cycling are not implemented.
LoRa reception remains continuous. MeshMesh current and battery-life gains are unmeasured;
other firmware's measurements do not establish our results. Community boards need owner testing.
