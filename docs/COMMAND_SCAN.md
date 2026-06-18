# FiveStar GF-8048MBW-FS RS232 Command Scan

This file records the RS232 commands tried against the FiveStar GF-8048MBW-FS inverter during local testing.

## Serial Settings

Confirmed working settings:

```text
2400 baud
8 data bits
No parity
1 stop bit
CR terminated commands
```

Observed USB serial device names on macOS varied, for example:

```text
/dev/cu.wchusbserial110
/dev/cu.wchusbserial3130
```

## Confirmed Read Commands

```text
Q  -> ACK
Q1 -> live status frame
F  -> rating/config frame
I  -> identification / firmware frame
```

Example responses:

```text
Q  -> ACK
Q1 -> (227.0 000.0 229.9 027 50.1 2.00 49.0 00000000
F  -> #230.0 021 048.0 50.0
I  -> #222222222222222222222222222R1.4.016
```

Known `Q1` fields:

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

`Q1` does not expose PV input voltage/current/power.

## Commands That Changed Settings

The following returned `ACK` and should be treated as setting/control commands:

```text
F50 -> ACK
F60 -> ACK
```

`F50` was sent after discovering `F60` was accepted, and the inverter reported 50 Hz again:

```text
F  -> #230.0 021 048.0 50.0
Q1 -> ... 50.1 ...
```

Avoid brute-forcing setting-like command prefixes.

## Scanned Command Results

### Single Letters

```text
A -> NAK
B -> NAK
C -> NAK
D -> NAK
E -> NAK
F -> #230.0 021 048.0 50.0
G -> NAK
H -> NAK
I -> #222222222222222222222222222R1.4.016
J -> NAK
K -> NAK
L -> NAK
M -> NAK
N -> NAK
O -> NAK
P -> NAK
Q -> ACK
R -> NAK
S -> NAK
T -> NAK
U -> NAK
V -> NAK
W -> NAK
X -> NAK
Y -> NAK
Z -> NAK
```

### Single Digits

```text
0 -> NAK
1 -> NAK
2 -> NAK
3 -> NAK
4 -> NAK
5 -> NAK
6 -> NAK
7 -> NAK
8 -> NAK
9 -> NAK
```

### Q Commands

```text
Q1  -> live status frame
Q2  -> NAK
Q3  -> timeout
Q4  -> NAK
Q5  -> NAK
Q6  -> NAK
q6  -> NAK
Q1-Q99 scan -> Q1 was the only useful response; most returned NAK, some timed out
```

Additional Q-family commands tested:

```text
Q3PV  -> timeout or NAK depending run
q3pv  -> NAK
QPV   -> NAK
QPV1  -> NAK
QPV2  -> NAK
QPV3  -> NAK
QGS   -> NAK
QS    -> NAK
QBV   -> NAK
QLDL  -> NAK
QRI   -> NAK
QMF   -> NAK
QPAR  -> NAK
QWS   -> NAK
QFS   -> NAK
QSK1  -> NAK
QSK2  -> NAK
QSK3  -> NAK
QSK4  -> NAK
QSKT1 -> NAK
QSKT2 -> NAK
QSKT3 -> NAK
QSKT4 -> NAK
QET   -> NAK
QEY   -> NAK
QEM   -> NAK
QED   -> NAK
QLT   -> NAK
QLD   -> NAK
QT    -> NAK
QCHGS -> NAK
QMCHGCR  -> NAK
QMUCHGCR -> NAK
QBEQI    -> NAK
QMN      -> NAK
QGMN     -> NAK
```

### F Commands

```text
F   -> #230.0 021 048.0 50.0
F1-F99 scan:
F50 -> ACK
F60 -> ACK
all other F-number commands tested returned NAK
```

Treat `F50` and `F60` as setting commands.

### I Commands

```text
I    -> #222222222222222222222222222R1.4.016
I1-I99 -> all returned NAK
```

### Basic Short Candidates

```text
V  -> NAK
V1 -> NAK
B  -> NAK
B1 -> NAK
M  -> NAK
M1 -> NAK
P  -> NAK
P1 -> NAK
p1 -> NAK
p2 -> NAK
p3 -> NAK
p4 -> NAK
p5 -> NAK
p6 -> NAK
```

### Voltronic / Axpert Query Candidates

Plain CR-terminated commands returned NAK unless noted:

```text
QPIGS
QPIGS2
QPIRI
QPIWS
QMOD
QID
QFLAG
QPI
QVFW
QVFW2
QDI -> timeout in one scan
QMD
MD
MOD
ID
PI
VFW
VFW2
FLAG
PIGS
PIRI
PIWS
PGS0
QPGS0
PGS1
QPGS1
CHGS
MUCHGCR
QOPPT
QCHPT
QTPR
QBEQI
```

### CRC-Framed Voltronic Commands

The following commands were sent as:

```text
command bytes + CRC16/XMODEM + CR
```

All returned NAK:

```text
QPIGS
QPIWS
QPIRI
QMOD
QID
QVFW
QVFW2
QFLAG
QOPPT
QCHPT
QMN
QT
QET
QLT
QLD
DAT
RTEY
RTDL
QEY
```

Example:

```text
QPIGS send=51 50 49 47 53 b7 a9 0d -> NAK
```

### Protocol 18 / P-Style Candidates

```text
P003GS -> NAK
P003PI -> NAK
P003WS -> NAK
P005GS -> NAK
P006GS -> NAK
P007GS -> NAK
```

### V-Series / Forum-Inspired Candidates

The MyBroadband forum thread showed PV data in a response frame starting with:

```text
56 50 48 42 49
```

ASCII:

```text
VPHBI
```

Plain commands and variants tested:

```text
CMSG.INV-HB     -> NAK
CMSG.INV_HB     -> NAK
CMSG:INV-HB     -> NAK
CMSG INV-HB     -> NAK
CMSG.INV.ST     -> NAK
CMSG.INV-STAT   -> NAK
CMSG.INV-STATUS -> NAK
CMSG.INV-DATA   -> NAK
CMSG.INV-RT     -> NAK
CMSG.CHGR-HB    -> NAK
CMSG.CHG-HB     -> NAK
CMSG.PV-HB      -> NAK
CMSG.SOL-HB     -> NAK
CMSG.MPPT-HB    -> NAK
INV-HB          -> NAK
INV_HB          -> NAK
CHG-HB          -> NAK
PV-HB           -> NAK
SOL-HB          -> NAK
MPPT-HB         -> NAK
HB              -> NAK
INVHB           -> NAK
CMSG            -> NAK
VPHBI           -> NAK
VPHB            -> NAK
VPHB?           -> NAK
VPHBI?          -> NAK
VPHBI0          -> NAK
VPHBI1          -> NAK
```

Additional variants of `CMSG.INV-HB` were tried:

```text
no terminator
CR
LF
CRLF
NUL
ETX
EOT
CTRL-Z
STX + payload
STX + payload + ETX
payload + ETX
8-bit length prefix
16-bit little-endian length prefix
16-bit big-endian length prefix
```

No useful response or `VPHBI` frame was observed. Variants with CR/CRLF generally returned `NAK`; non-terminated/binary variants generally timed out or produced NAK.

### H-Style Candidates

```text
HBAT  -> NAK
hbat  -> NAK
HPV   -> NAK
hpv   -> NAK
HGRID -> NAK
hgrid -> NAK
HOP   -> NAK
hop   -> NAK
```

## PV Data Status

No command tested so far exposes PV voltage, PV current, or PV watts on this inverter.

The confirmed `Q1` response contains AC input/output, load percentage, output current, battery voltage, and status bits, but not PV data.

The likely possibilities are:

```text
1. PV data uses a vendor-specific protocol not yet discovered.
2. PV data is available only after a V-Series/runtime handshake.
3. PV data is not exposed on this RS232 port/firmware variant.
4. The factory dongle uses a private protocol adapter not represented by the plain commands tested here.
```

## Safety Notes

Avoid blind brute-force scans of setting/control command prefixes. The scan discovered that:

```text
F50 -> ACK
F60 -> ACK
```

and these appear to set output frequency.

Avoid testing shutdown, output-control, or battery-test commands unless intentionally prepared:

```text
S...
C...
T...
TL
CT
SON
SOFF
SKON...
SKOFF...
P...
POP...
PCP...
PBCV...
```
