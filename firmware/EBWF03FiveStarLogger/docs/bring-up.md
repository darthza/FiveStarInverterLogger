# Bring-Up Checklist

## Before Flashing

- Confirm factory backup exists and checksum matches.
- Confirm module is powered from stable 3.3V.
- Confirm CP2102 TX/RX are 3.3V logic.
- Confirm `DL` is only grounded during reset for flashing.

## First Firmware Goal

This first firmware is intentionally simple:

- Join WiFi.
- Enable OTA.
- Poll inverter using `Q1`.
- Print raw responses to debug serial.
- Publish to MQTT only if enabled in `include/config.h`.

## Expected Q1 Response

Example:

```text
(222.3 000.0 222.3 021 50.0 2.15 52.0 00100000
```

Mapped as:

```text
input voltage
bypass/input field
output voltage
load percent
output frequency
output current
battery voltage
status bits
```

## First Test

1. Flash firmware.
2. Remove `DL` from `GND`.
3. Power-cycle normally.
4. Confirm it joins WiFi.
5. Confirm OTA is visible.
6. Connect inverter UART.
7. Watch debug output for `Q1`.

## If It Does Not Boot

- Check WiFi credentials.
- Serial flash a fixed build.
- As a last resort, restore the factory backup.

