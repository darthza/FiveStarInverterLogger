# Handover

## Context

This project was created after diagnosing a FiveStar GF-8048MBW-FS inverter that would not show data in SmartESS/DessMonitor despite the WiFi Plug Pro 5 logger being online.

The inverter serial port was tested with an Astrum USB-to-RS232 DB9 adapter. A CP2102 TTL UART adapter produced invalid data and should not be used directly on the inverter RS232 port.

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

## Known Working Samples

```text
Q   -> ACK
Q1  -> (222.3 000.0 222.3 021 50.0 2.15 52.0 00100000
F   -> #230.0 021 048.0 50.0
I   -> #222222222222222222222222222R1.4.016
```

## Open Questions

- `Q1` does not include PV input data.
- `CMSG.INV-HB` may expose PV data on related FiveStar/Sunmagic units, but this still needs to be tested on the target inverter while the RS232 cable is connected.
- `LoadOrPowerField` likely represents load percentage or active power, but it should be calibrated against a known load.
- The status bit field `00100000` is captured raw until the bit meanings are confirmed.

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

1. Test `CMSG.INV-HB` with the cable connected and inspect the raw hex in logs/payload.
2. Confirm `LoadOrPowerField` by applying a known load.
3. Decode status bits.
4. Add a repository CI workflow once the first GitHub commit is pushed.
5. Add integration tests around captured raw serial responses.
