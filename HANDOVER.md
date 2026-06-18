# Handover

## Context

This project was created after diagnosing a FiveStar GF-8048MBW-FS inverter that would not show data in SmartESS/DessMonitor despite the WiFi Plug Pro 5 logger being online.

The inverter serial port was tested with USB RS232 adapters. A CP2102 TTL UART adapter produced invalid data and should not be used directly on the inverter RS232 port.

The EB-WF03-01 / WiFi Plug Pro 5 logger module is ESP8266-based. The original firmware was backed up before custom firmware work.

## Findings

The inverter responds over RS232 using a legacy Megatec/Q1-style ASCII protocol:

- `2400` baud
- `8N1`
- command terminator: `\r`
- `Q1\r` returns live readings
- `F\r` returns rated/default values
- `I\r` returns ID/firmware-like data
- `Q\r` returns `ACK`

The inverter rejects common Voltronic/Axpert PI30 commands like `QPIGS` with `NAK`, and Modbus RTU probes did not respond.

The full command scan is documented in [docs/COMMAND_SCAN.md](docs/COMMAND_SCAN.md).

## Known Working Samples

```text
Q   -> ACK
Q1  -> (222.3 000.0 222.3 021 50.0 2.15 52.0 00100000
F   -> #230.0 021 048.0 50.0
I   -> #222222222222222222222222222R1.4.016
```

## PV Data

No tested command currently exposes PV voltage, PV current, or PV watts.

The following were tested and rejected with `NAK` or timeout:

- Plain Voltronic/Axpert commands such as `QPIGS`, `QPIRI`, `QPIWS`, `QMOD`, `QID`, `QVFW`, `QFLAG`, `QPGS0`, `QOPPT`, `QCHPT`, `QET`, `QLT`
- CRC-framed Voltronic commands using CRC16/XMODEM
- V-Series/forum-inspired commands such as `CMSG.INV-HB`, `CMSG.PV-HB`, `CMSG.MPPT-HB`, `VPHBI`
- H-style commands such as `HBAT`, `HPV`, `HGRID`, `HOP`

The MyBroadband FiveStar/Sunmagic thread shows PV data in a `VPHBI...` response frame, but this inverter rejected plain `CMSG.INV-HB` and multiple framing variants. The remaining theory is that V-Series uses an undiscovered handshake/framing layer, or this inverter firmware exposes only the older short command set.

## ESP8266 Logger Firmware

Custom firmware lives under:

```text
firmware/EBWF03FiveStarLogger
```

Important PlatformIO environments:

```text
eb_wf03_01                         normal logger
eb_wf03_01_ota                     normal logger OTA
eb_wf03_01_recovery                WiFi + OTA recovery firmware
eb_wf03_01_connected_diag          disables inverter UART for connected diagnostics
eb_wf03_01_pin_diag                publishes GPIO pin state diagnostics
eb_wf03_01_pin_diag_low_power      pin diagnostics with reduced WiFi power
eb_wf03_01_pin_diag_ultra_low_power ultra-low-power pin diagnostics, OTA disabled
eb_wf03_01_fast_power_monitor      immediate polling for short uptime windows
```

The factory backup indicated DOUT flash mode, so `platformio.ini` now sets:

```text
board_build.flash_mode = dout
```

## Logger Reboot Findings

When installed in the inverter socket, the logger repeatedly comes online briefly and then resets. MQTT and ping show a rough cycle:

```text
up for about 8 seconds
down for about 16-18 seconds
repeat every ~24-27 seconds
```

Diagnostic firmware showed:

```text
reset_reason = Power On
GPIO0 = 1
GPIO2 = 1
GPIO15 = 0
inverter UART disabled
```

Low-power and ultra-low-power firmware did not materially change the reset cadence. This makes firmware CPU/WiFi load and inverter serial polling unlikely as the sole cause.

Most likely remaining causes:

- inverter socket is power-cycling the logger
- EN/RST or a board-level power-control line is being toggled
- factory firmware initializes a board/inverter control path not yet replicated
- the inverter expects an electrical or protocol handshake before keeping the logger powered

Future hardware checks should measure the logger 3.3V rail and EN/RST line during the cycle.

## Open Questions

- `Q1` does not include PV input data.
- `LoadOrPowerField` likely represents load percentage or active power, but it should be calibrated against a known load.
- The status bit field `00100000` is captured raw until the bit meanings are confirmed.
- The logger reboot root cause is unresolved and likely requires hardware-level measurement.

## SmartHome Integration

`SmartHomePublisher` sends a JSON `POST` to the configured endpoint. It is intentionally generic:

- endpoint configured by `SmartHome:EndpointUrl`
- optional `X-Api-Key` header from `SmartHome:ApiKey`
- publishing disabled by default

If the SmartHome project expects a different contract, adjust `Publishing/SmartHomePublisher.cs`, preferably only in `ToPayload`.

## Operational Notes

On Raspberry Pi/Linux:

- serial port is usually `/dev/ttyUSB0`
- service user must be in the `dialout` group
- run under systemd for restart-on-failure behavior

The serial client reopens the port after failures. This helps with USB adapter disconnect/reconnect events.

## Recommended Next Steps

1. Flash `eb_wf03_01_fast_power_monitor` when using the logger in the short uptime window.
2. Confirm `LoadOrPowerField` by applying a known load.
3. Decode status bits.
4. Measure logger 3.3V and EN/RST while installed in the inverter socket.
5. Sniff V-Series Runtime traffic if PV data remains required.
6. Add integration tests around captured raw serial responses.
