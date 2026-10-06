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

[EasySkyMesh](https://github.com/IoTThinks/EasySkyMesh/wiki/PowerSaving#3-test-results) informed this
work. RXPS (duty-cycled LoRa reception) and scheduled GPS power cycling are not implemented.
LoRa reception remains continuous. MeshMesh current and battery-life gains are unmeasured;
other firmware's measurements do not establish our results. Community boards need owner testing.
