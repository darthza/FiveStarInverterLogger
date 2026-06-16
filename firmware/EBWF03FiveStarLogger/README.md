# EB-WF03-01 FiveStar Logger Firmware

Custom ESP8266 firmware for replacing the SmartESS/DessMonitor firmware on an EB-WF03-01 WiFi module and polling a FiveStar GF-8048MBW-FS inverter locally.

## Current Status

- Target module: EB-WF03-01 / ESP8266EX / 4MB flash
- Factory firmware backup: verified separately before flashing
- Inverter protocol: legacy ASCII serial, not Modbus
- UART settings: `2400 8N1`
- Known commands: `Q`, `Q1`, `F`, `I`
- OTA: enabled using ArduinoOTA
- MQTT: optional, enabled in `include/config.h`

## Safety

Do not flash this until you have a verified factory firmware backup.

Known good backup SHA-256 from this unit:

```text
00034df2fb95549b8e661d74ed564894f3e059cf458b07139cb7f3c4b80a73ae
```

## Configure

Copy the example config:

```bash
cp include/config.example.h include/config.h
```

Edit `include/config.h`:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `OTA_PASSWORD`
- optional MQTT settings

`include/config.h` is ignored by git.

## Build

Install PlatformIO, then:

```bash
pio run
```

## First Serial Flash

Put the module into bootloader mode:

1. Connect `DL` / `GPIO0` to `GND`.
2. Power-cycle or reset the module.
3. Release `DL` after a couple of seconds.

Then flash:

```bash
pio run -t upload --upload-port /dev/cu.usbserial-0001
```

## OTA Updates

After the first successful flash and WiFi join:

```bash
FIVESTAR_OTA_PASSWORD='your-ota-password' pio run -e eb_wf03_01_ota -t upload
```

If OTA fails because the firmware cannot boot or cannot join WiFi, use serial flashing again.

## MQTT Diagnostics

When MQTT is enabled, the firmware publishes retained inverter values and
non-retained status/diagnostic values under `MQTT_TOPIC_PREFIX`.

Useful topics:

```text
status/version
status/ip
status/rssi
status/uptime_ms
status/free_heap
status/inverter_timeouts
status/last_timeout_command
raw/q1_parse_error
```

If `status/uptime_ms` keeps increasing and `free_heap` is steady, the ESP is
not reboot-looping. If `status/inverter_timeouts` increases, the ESP is alive
but the inverter UART is not replying on the configured pins/baud/path.

## UART Pins

Expected EB-WF03-01 mapping:

```text
GPIO1 / TX0 -> inverter RX
GPIO3 / RX0 -> inverter TX
GPIO0 / DL  -> bootloader select, hold low only for flashing
GND         -> inverter/module ground
3.3V        -> regulated 3.3V only
```

Debug logging uses `Serial1` TX-only, typically GPIO2 on ESP8266. It is not required for operation.

## Restore Factory Firmware

If needed, restore the original SmartESS firmware with esptool:

```bash
python -m esptool --port /dev/cu.usbserial-0001 --baud 115200 write-flash 0x000000 work/firmware/eb-wf03-01-esp8266-backup.bin
```
