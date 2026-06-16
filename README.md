# FiveStarInverterLogger

.NET Worker Service for reading a FiveStar GF-8048MBW-FS / Sunmagic-style inverter over RS232 and publishing the parsed readings to a SmartHome endpoint.

The inverter tested here uses a legacy Megatec/Q1-style ASCII protocol, not Modbus and not the newer Voltronic `QPIGS`/PI30 command set.

## Confirmed Protocol

- Cable: USB-to-RS232 DB9 adapter
- Port on Raspberry Pi/Linux: usually `/dev/ttyUSB0`
- Serial settings: `2400 8N1`
- Line ending: carriage return (`\r`)
- Working commands:
  - `Q` -> `ACK`
  - `Q1` -> live status
  - `F` -> nominal/rated values
  - `I` -> device/firmware-like value

Example `Q1` response:

```text
(222.3 000.0 222.3 021 50.0 2.15 52.0 00100000
```

Current field mapping:

| Field | Meaning |
| --- | --- |
| `222.3` | Input/grid voltage |
| `000.0` | Input field |
| `222.3` | Output voltage |
| `021` | Load/power field |
| `50.0` | Output frequency |
| `2.15` | Output current |
| `52.0` | Battery voltage |
| `00100000` | Status bits |

PV data is not present in `Q1`. FiveStar/Sunmagic protocol discussions suggest `CMSG.INV-HB` may expose additional binary data on some models; this service captures that command raw in `raw.cmsgInvHb` when the inverter responds.

## Configuration

Edit `appsettings.json` or override with environment variables.

```json
{
  "Inverter": {
    "PortName": "/dev/ttyUSB0",
    "BaudRate": 2400,
    "ReadTimeoutMs": 1200,
    "PollIntervalSeconds": 10
  },
  "SmartHome": {
    "Enabled": true,
    "EndpointUrl": "https://your-smarthome.example/api/inverter-readings",
    "ApiKey": "optional-api-key",
    "DeviceId": "fivestar-gf-8048mbw-fs"
  }
}
```

Environment variable examples:

```bash
export Inverter__PortName=/dev/ttyUSB0
export SmartHome__Enabled=true
export SmartHome__EndpointUrl=http://localhost:5000/api/inverter-readings
export SmartHome__ApiKey=replace-me
```

## Run Locally

```bash
dotnet restore
dotnet run
```

On macOS, the Astrum/WCH cable may appear as:

```text
/dev/cu.wchusbserial310
```

Override it:

```bash
Inverter__PortName=/dev/cu.wchusbserial310 dotnet run
```

## Raspberry Pi Setup

Install .NET 8 on Raspberry Pi OS, then:

```bash
git clone https://github.com/darthza/FiveStarInverterLogger.git
cd FiveStarInverterLogger
dotnet publish -c Release -o ./publish
```

Check the serial device:

```bash
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

If the service user cannot open the serial port:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in after changing groups.

## systemd Service

Create `/etc/systemd/system/fivestar-inverter-logger.service`:

```ini
[Unit]
Description=FiveStar Inverter Logger
After=network-online.target
Wants=network-online.target

[Service]
WorkingDirectory=/opt/FiveStarInverterLogger
ExecStart=/usr/bin/dotnet /opt/FiveStarInverterLogger/FiveStarInverterLogger.dll
Restart=always
RestartSec=10
User=pi
Environment=DOTNET_ENVIRONMENT=Production
Environment=Inverter__PortName=/dev/ttyUSB0
Environment=SmartHome__Enabled=false

[Install]
WantedBy=multi-user.target
```

Install and start:

```bash
sudo mkdir -p /opt/FiveStarInverterLogger
sudo cp -r publish/* /opt/FiveStarInverterLogger/
sudo systemctl daemon-reload
sudo systemctl enable --now fivestar-inverter-logger
sudo journalctl -u fivestar-inverter-logger -f
```

## EB-WF03-01 Custom Firmware

This repo also includes experimental Arduino/PlatformIO firmware for replacing
the SmartESS firmware on the EB-WF03-01 WiFi module:

```text
firmware/EBWF03FiveStarLogger
```

The firmware currently provides:

- ESP8266 WiFi connection
- ArduinoOTA updates after the first serial flash
- `2400 8N1` inverter UART polling
- raw `Q`, `Q1`, `F`, and `I` command support
- parsed `Q1` fields
- optional MQTT publishing, disabled by default

Before flashing, keep the verified factory firmware backup safe. See
`firmware/EBWF03FiveStarLogger/README.md` and
`firmware/EBWF03FiveStarLogger/docs/bring-up.md`.

## SmartHome Payload

The worker `POST`s JSON to `SmartHome:EndpointUrl` when enabled. The request includes `X-Api-Key` if `SmartHome:ApiKey` is set.

Payload shape:

```json
{
  "deviceId": "fivestar-gf-8048mbw-fs",
  "timestamp": "2026-06-14T18:00:00Z",
  "protocol": "Megatec/Q1 legacy RS232 ASCII",
  "status": {
    "inputVoltage": 222.3,
    "inputField": 0.0,
    "outputVoltage": 222.3,
    "loadOrPowerField": 21,
    "outputFrequency": 50.0,
    "outputCurrent": 2.15,
    "batteryVoltage": 52.0,
    "statusBits": "00100000",
    "approxOutputApparentPowerVa": 477.945
  },
  "rating": {
    "nominalAcVoltage": 230.0,
    "ratingField": 21,
    "nominalBatteryVoltage": 48.0,
    "nominalFrequency": 50.0
  },
  "identity": {
    "deviceId": "222222222222222222222222222",
    "firmware": "R1.4.016"
  },
  "raw": {
    "q": "ACK",
    "q1": "(222.3 000.0 222.3 021 50.0 2.15 52.0 00100000",
    "f": "#230.0 021 048.0 50.0",
    "i": "#222222222222222222222222222R1.4.016",
    "md": "",
    "qmd": "",
    "cmsgInvHb": ""
  }
}
```

## License

MIT
