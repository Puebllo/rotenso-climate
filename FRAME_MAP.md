# Rotenso UART frame map

This document describes the frames **currently sent or decoded by this component**. It is a reverse-engineered map for the captured Rotenso/TCL-family unit, not an official protocol specification. Other models and firmware may use different lengths or field meanings.

## Evidence and notation

- The primary evidence is [Rotenso.xlsx](Rotenso.xlsx): the `SetCommand` sheet contains commands, and the `Heartbeat` sheet contains AC responses. The sheet also contains the author's own byte annotations; those notes were part of the starting evidence.
- The worksheet's original vertical fixed-position numbering was later reversed using hardware feedback. The mappings below reflect the **current component**, where vertical position 1 is up and position 5 is down. The vertical swing-range labels were subsequently swapped to match that numbering.
- The [swing implementation](components/rotenso/rotenso_swing.cpp), [frame builder](components/rotenso/rotenso_frame_builder.cpp), and [response parser](components/rotenso/frame_parser.cpp) define the current behavior. The [UART tests](tests/uart_flow_test.cpp) include captured examples and check the mappings.

All byte indices below are **zero-based from the beginning of the complete frame**, not from the payload. Values are hexadecimal unless otherwise stated. Bits are numbered from bit 0, the least significant bit. “Unknown” means the component does not interpret that byte; a constant value in these captures is not proof that the byte is reserved or always constant on every model.

The analysis below covers 39 complete SET frames and 74 complete 61-byte status frames in the spreadsheet. All 113 have a valid XOR checksum. Some represent earlier experimental ESPHome frames; a valid checksum alone does not prove that a command caused the intended physical behavior.

## Transport and frame structure

The example ESPHome configuration uses **9600 baud, 8 data bits, even parity, and 1 stop bit** (`9600 8E1`).

```text
Offset       0         1         2         3          4        5 ... 4+N      5+N
Field      start     sender    receiver   command   length       payload       XOR
Example     BB         00         01         03       21       33 bytes       ...
```

- `BB 00 01` is sent by the ESPHome module to the AC. `BB 01 00` comes from the AC.
- Byte 4 is the payload length `N`, so total frame length is `N + 6` bytes.
- The last byte is the XOR of all **preceding** bytes. XORing the entire valid frame, including its checksum, gives `0x00`.
- The component sends an 8-byte GET/heartbeat (`0x04`) and a 39-byte SET (`0x03`). It processes 61-byte state responses with command `0x03` or `0x04`. Other response commands are framed and checked but not decoded as climate state.

## GET / heartbeat sent to the AC: 8 bytes

The component sends this request at startup and then approximately every 3 seconds, subject to the command queue:

```text
BB 00 01 04 02 01 00 BD
```

| Byte | Value | Meaning in this component |
| ---: | --- | --- |
| 0 | `BB` | Frame start. |
| 1 | `00` | Module-to-AC direction, part 1. |
| 2 | `01` | Module-to-AC direction, part 2. |
| 3 | `04` | GET/status request. |
| 4 | `02` | Two payload bytes follow. |
| 5 | `01` | Fixed request payload. |
| 6 | `00` | Fixed request payload. |
| 7 | `BD` | XOR of bytes 0–6. |

## SET command sent to the AC: 39 bytes

Each SET is a snapshot containing the current power, mode, target temperature, fan and vane settings. The component starts from its latest accepted state, applies queued changes, then builds the frame. Fields not implemented by this component are currently sent as fixed values; this does **not** mean their possible AC-side settings are preserved.

| Byte | Sent value | Meaning / current behavior |
| ---: | --- | --- |
| 0 | `BB` | Frame start. |
| 1 | `00` | Module-to-AC direction, part 1. |
| 2 | `01` | Module-to-AC direction, part 2. |
| 3 | `03` | SET command. |
| 4 | `21` | Payload length: `0x21` = 33 bytes, at offsets 5–37. |
| 5 | `00` | Fixed by builder; meaning unknown. |
| 6 | `00` | Fixed by builder; meaning unknown. |
| 7 | `60` / `64` | Power: bit 2 (`04`) off/on. Other bits are held in the `60` base value; their individual functions are not controlled here. |
| 8 | Varies | Preset/flag bits in the high nibble and operating mode in the low nibble; see below. |
| 9 | `50`–`5F` | Encoded whole target temperature; see below. |
| 10 | Varies | Fan speed in bits 0–2; vertical swing enabled with mask `38` in bits 3–5. |
| 11 | `00`, `02`, `08`, `0A` | Bit 1 (`02`) = half degree; bit 3 (`08`) = horizontal swing enabled. The flags combine with OR. |
| 12 | `00` | Fixed by builder; meaning unknown. |
| 13 | `00` | Fixed by builder; meaning unknown. |
| 14 | `00` | Fixed by builder; meaning unknown. |
| 15 | `00` | Fixed by builder; meaning unknown. |
| 16 | `00` | Fixed by builder; meaning unknown. |
| 17 | `00` | Fixed by builder; meaning unknown. |
| 18 | `00` | Fixed by builder; meaning unknown. |
| 19 | `00` | Fixed by builder; meaning unknown here. Sleep is not implemented. |
| 20 | `00` | Fixed by builder; meaning unknown. |
| 21 | `00` | Fixed by builder; meaning unknown. |
| 22 | `00` | Fixed by builder; meaning unknown. |
| 23 | `00` | Fixed by builder; meaning unknown. |
| 24 | `00` | Fixed by builder; meaning unknown. |
| 25 | `00` | Fixed by builder; meaning unknown. |
| 26 | `00` | Fixed by builder; meaning unknown. |
| 27 | `00` | Fixed by builder; meaning unknown. |
| 28 | `00` | Fixed by builder; meaning unknown. |
| 29 | `00` | Fixed by builder; meaning unknown. |
| 30 | `00` | Fixed by builder; meaning unknown. |
| 31 | `00` | Fixed by builder; meaning unknown. |
| 32 | Varies | Vertical vane position / range: lower five bits of its state. |
| 33 | `80` plus value | Horizontal vane position / range: `80 OR (state AND 3F)`. |
| 34 | `00` | Fixed by builder; meaning unknown. |
| 35 | `00` | Fixed by builder; meaning unknown. |
| 36 | `00` | Fixed by builder; meaning unknown. |
| 37 | `00` | Fixed by builder; meaning unknown. |
| 38 | Varies | XOR of bytes 0–37. |

The spreadsheet's SET annotation describes the checksum as XOR of bytes 0–38. Byte 38 is the checksum itself: XOR bytes 0–37 to **calculate** it, or XOR bytes 0–38 to **verify** that the result is zero.

### SET byte 8: mode and preset

| Field | Values currently encoded |
| --- | --- |
| Low nibble: mode | `01` heat, `02` dry, `03` cool, `07` fan only, `08` auto/default. Off is selected with byte 7; the low nibble defaults to `08`. |
| High nibble: preset | `00` none, `40` boost/turbo, `80` ESPHome ECO (called **low noise** in the builder's comment), `10` comfort/health in the encoder. |

Only Off, Heat, Cool, and Fan Only modes and None, ECO, and Boost presets are currently exposed in the component traits. The encoder contains additional cases, but that does not make them selectable in the default Home Assistant interface. In particular, the component's `ECO` label maps to the `80` low-noise flag in SET byte 8; do not assume it is the AC's separate energy-saving ECO function without further verification.

### SET temperature and fan

For a target in the supported 16–31 °C range, let `T` be the whole-degree part:

```text
byte[9] = 0x50 | ((31 - T) & 0x0F)
byte[11] bit 1 = 1 when the target has a .5 degree fraction
```

For example, 20 °C uses byte 9 = `5B`; 20.5 °C also uses `5B` and adds `02` to byte 11. Horizontal swing adds `08` to byte 11 independently, so 20.5 °C with horizontal swing uses `0A` there. The captures include `08` and `0A` while horizontal swing is active; they are not inherently the whole-degree and half-degree values when horizontal swing is off.

| ESPHome fan setting | SET byte 10, low 3 bits | Decoded status byte 8, high nibble |
| --- | --- | --- |
| Auto | `00` | `8` |
| Low | `02` | `9` or `C` |
| Medium | `03` | `A` |
| High | `05` | `B` or `D` |

The captures also contain fan level 2 = `06` and fan level 4 = `07` in the SET field, but the component currently offers only its four ESPHome fan settings. Status decoding collapses levels 1/2 to Low and levels 4/5 to High.

## Vane mechanics

The captures indicate a **movement flag plus a position/range value** for each axis. The component can enable vertical and horizontal movement independently, so both flags may be present in the same command. It does not send the motor's angle in degrees or control individual motor steps.

### Vertical vane

| Desired setting | SET byte 10 bits `38` | SET byte 32 | Status byte 10 bit `40` | Status byte 51, relevant bits |
| --- | --- | --- | --- | --- |
| Stopped / no explicit position | `00` | `00` | `00` | `00` |
| Fixed position 1, up | `00` | `01` | `00` | `01` |
| Fixed position 2 | `00` | `02` | `00` | `02` |
| Fixed position 3 | `00` | `03` | `00` | `03` |
| Fixed position 4 | `00` | `04` | `00` | `04` |
| Fixed position 5, down | `00` | `05` | `00` | `05` |
| Swing 1–5 | `38` | `08` | `40` | `08` |
| Swing 1–3, upper half | `38` | `10` | `40` | `10` |
| Swing 3–5, lower half | `38` | `18` | `40` | `18` |

The fixed-position rows use the current UI numbering, after the user's correction. The spreadsheet originally named `05` “fix 1 / down” and `01` “fix 5 / up”; those are now position 5 and position 1 in the component. While moving, the parser identifies the vertical range with `byte[51] & 18`; while stopped, it identifies the fixed position with `byte[51] & 07`.

### Horizontal vane

| Desired setting | SET byte 11 bit `08` | SET byte 33 | Status byte 10 bit `20` | Status byte 52, relevant bits |
| --- | --- | --- | --- | --- |
| Stopped / no explicit position | `00` | `80` | `00` | `00` |
| Fixed position 1, left | `00` | `81` | `00` | `01` |
| Fixed position 2 | `00` | `82` | `00` | `02` |
| Fixed position 3 | `00` | `83` | `00` | `03` |
| Fixed position 4 | `00` | `84` | `00` | `04` |
| Fixed position 5, right | `00` | `85` | `00` | `05` |
| Swing 1–5 | `08` | `88` | `20` | `08` |
| Swing 1–3 | `08` | `90` | `20` | `10` |
| Swing 2–4 | `08` | `98` | `20` | `18` |
| Swing 3–5 | `08` | `A0` | `20` | `20` |

`byte[11]` can also contain `02` for the half-degree flag, so its complete value during horizontal swing can be `08` or `0A`. While moving, the parser identifies the horizontal range with `byte[52] & 38`; while stopped, it uses `byte[52] & 07` for the fixed position.

For horizontal movement, the offset is taken from **your captures**: `SetCommand` rows 28–31 have bit `08` in complete-frame byte 11, while byte 12 remains `00`. The [AC-hack SET map](https://github.com/adaasch/AC-hack#0x03-set-values) describes bit `08` at payload byte 7, which would be complete-frame byte 12. This is a difference between the documented observations; the implementation uses the offset seen in your unit's frames.

### What changed in the captures

All rows below refer to the workbook and use the current vertical labels:

| Sheet and row | Labeled setting | SET byte 10 | SET byte 11 | SET byte 32 | SET byte 33 | SET checksum |
| --- | --- | --- | --- | --- | --- | --- |
| `SetCommand` 19 | Fan 3, no moving vanes | `03` | `00` | `00` | `80` | `27` |
| `SetCommand` 20 | Fan 3, vertical full swing | `3B` | `00` | `08` | `80` | `17` |
| `SetCommand` 21 | Fan 3, vertical upper swing / current 1–3 | `3B` | `00` | `10` | `80` | `0F` |
| `SetCommand` 22 | Fan 3, vertical lower swing / current 3–5 | `3B` | `00` | `18` | `80` | `07` |
| `SetCommand` 27 | Fan 3, vertical fixed up / current position 1 | `03` | `00` | `01` | `80` | `26` |
| `SetCommand` 28 | Fan 3, horizontal full swing; vertical position byte also differs | `03` | `08` | `01` | `88` | `26` |
| `SetCommand` 30 | Fan 3, horizontal middle swing | `03` | `08` | `01` | `98` | `36` |

For example, rows 19 and 20 show fan 3 as `03`, then fan 3 plus vertical movement as `3B` (`03 | 38`). Rows 20–22 keep that movement flag while byte 32 selects full, upper, or lower swing. In the response sheet, rows 58–60 show the matching vertical movement flag `40` and range values at byte 51; rows 66–69 show horizontal movement flag `20` and values at byte 52. Rows 52–54 include both flags (`60`) with both range bytes set to `08`.

The captures do not give the physical angles, speed, stepper motor timings, or the internals of the AC's vane controller. Also, some experimental response rows contain movement flags with a zero range byte. The component preserves the raw flags in that situation and reports the corresponding selector as `Unknown` rather than inventing a range.

### Complete SET example

`SetCommand` row 21 is Cool, 20 °C, fan 3, vertical upper swing, and no horizontal swing:

```text
BB 00 01 03 21 00 00 64 03 5B 3B 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
10 80 00 00 00 00 0F
```

Reading the important offsets: `[7]=64` is on, `[8]=03` is Cool, `[9]=5B` is 20 °C, `[10]=3B` is fan 3 plus vertical movement, `[11]=00` means no half-degree or horizontal movement, `[32]=10` selects the upper vertical range, and `[33]=80` has no explicit horizontal position or range. The final byte `[38]=0F` is the checksum.

## AC status response: 61 bytes

A SET response has command byte `03`; a GET/heartbeat response has command byte `04`. Both captured types use a 55-byte payload and the same state layout. The observed-value column summarizes the 74 complete status frames in the spreadsheet; `varies` means that more than one value was seen. Fields marked “not read” are not parsed by the current component.

| Byte | Observed values | Meaning / current behavior |
| ---: | --- | --- |
| 0 | `BB` | Frame start. |
| 1 | `01` | AC-to-module direction, part 1. |
| 2 | `00` | AC-to-module direction, part 2. |
| 3 | `03`, `04` | SET response or GET/heartbeat response. |
| 4 | `37` | Payload length: `0x37` = 55 bytes, at offsets 5–59. |
| 5 | `04` | Payload marker in these captures; not independently checked by the state parser. |
| 6 | `00` | Unknown; not read. |
| 7 | Varies | Power/preset high nibble and operating-mode low nibble; see below. |
| 8 | Varies | Fan status high nibble and target temperature low nibble; see below. |
| 9 | `00`, `02` | Half-degree indicator; current parser adds 0.5 °C when `(byte & 0F) == 02`. |
| 10 | `00`, `20`, `40`, `60` | Horizontal moving = bit 5 (`20`); vertical moving = bit 6 (`40`). |
| 11 | `00` | Unknown; not read. |
| 12 | `00` | Unknown; not read. |
| 13 | `00` | Unknown; not read. |
| 14 | `00` | Unknown; not read. |
| 15 | `00` | Unknown; not read. |
| 16 | `00` | Unknown; not read. |
| 17 | `66`, `69`, `6B`, `6E` | Unknown in this component; possible temperature-related field in other implementations, not verified here. |
| 18 | `03`, `0B`, `23`, `2B` | Unknown in this component; may relate to byte 17, not verified here. |
| 19 | `08`, `88` | Unknown; not read. |
| 20 | `00` | Unknown; not read. |
| 21 | `00` | Unknown; not read. |
| 22 | `00` | Unknown; not read. |
| 23 | `00` | Unknown; not read. |
| 24 | `00` | Unknown; not read. |
| 25 | `00` | Unknown; not read. |
| 26 | `00` | Unknown; not read. |
| 27 | `00` | Unknown; not read. |
| 28 | `00` | Unknown; not read. |
| 29 | `00` | Unknown; not read. |
| 30 | Varies | Unknown; not read. |
| 31 | `FF` | Unknown; not read. |
| 32 | `42` | Unknown; not read. This is **not** the SET frame's vertical byte. |
| 33 | `00`, `80` | Unknown; not read. This is **not** the SET frame's horizontal byte. |
| 34 | Varies | Unknown; the workbook considered fan-speed correlation uncertain. Not read. |
| 35 | Varies | Unknown; not read. |
| 36 | Varies | Unknown; not read. |
| 37 | Varies | Unknown; not read. |
| 38 | Varies | Unknown; not read. |
| 39 | Varies | Unknown; not read. |
| 40 | `80`, `8A`, `C0` | Unknown; not read. |
| 41 | `00` | Unknown; not read. |
| 42 | `00` | Unknown; not read. |
| 43 | `00` | Unknown; not read. |
| 44 | `00` | Unknown; not read. |
| 45 | Varies | Unknown; not read. |
| 46 | `00`, `01` | Unknown; not read. |
| 47 | `00` | Unknown; not read. |
| 48 | `00` | Unknown; not read. |
| 49 | `44` | Unknown; not read. |
| 50 | `40` | Unknown; not read. |
| 51 | `00`, `01`–`05`, `08`, `10`, `18` | Vertical position/range. The component stores `byte & 1F`. |
| 52 | `00`, `01`–`05`, `08`, `10`, `18`, `20` | Horizontal position/range. The component stores `byte & 3F`. |
| 53 | `00` | Unknown; not read. |
| 54 | `00` | Unknown; not read. |
| 55 | `7A` | Unknown; not read. |
| 56 | `00` | Unknown; not read. |
| 57 | `00` | Unknown; not read. |
| 58 | `00` | Unknown; not read. |
| 59 | `00` | Unknown; not read. |
| 60 | Varies | XOR of bytes 0–59. The UART frame handler checks the full frame XOR is zero before decoding. |

### Status byte 7: power, mode, preset

The current parser reads the high and low nibbles separately:

| Nibble | Values currently decoded |
| --- | --- |
| High: power / preset | `2` = Off / None; `3` = On / None; `7` = ECO; `B` = Boost; `F` = Boost (the capture notes suggest ECO + Boost). Other preset values fall back to None. |
| Low: mode, while not Off | `1` = Cool, `2` = Fan Only, `3` = Dry, `4` = Heat, `5` = Auto. Unrecognized values fall back to Off. |

This direction uses **different mode values** than the SET frame: for example, SET `03` means Cool, while status low nibble `1` means Cool.

### Status byte 8: fan and target temperature

```text
whole target temperature = 16 + (byte[8] & 0x0F)
fan code                 = (byte[8] >> 4) & 0x0F
```

For example, `A4` means fan code `A` (Medium) and 20 °C (`16 + 4`). A separate `02` in the low nibble of byte 9 makes the target 20.5 °C under the current parser's exact check.

## Communication and remaining questions

The component waits for a valid status before sending a SET, serializes SETs with heartbeats, and uses the returned status to update Home Assistant. If no SET response arrives within 1 second, it retries once when the bus is quiet and no partial frame is buffered; after another timeout it requests a fresh status. A 50 ms bus-quiet interval is enforced, and incomplete incoming frames are resynchronized after 500 ms of no new bytes.

### Leads from a related TCL implementation

The following fields are described by [AC-hack](https://github.com/adaasch/AC-hack) for its unit. They are **clues to investigate, not confirmed features of this Rotenso component or proof of the same behavior on your AC**. The source describes a different SET length and uses payload-relative offsets; the numbers below have been converted to the complete-frame offsets used in this document.

| Frame and byte | Reported meaning in the related implementation | Status here |
| --- | --- | --- |
| SET 7 | Bits `20` and `40` may control the buzzer and display. | The current builder always sets both bits; the individual effects have not been verified here. |
| SET 19 | Sleep-mode selection. | Always sent as `00`; not implemented here. |
| Status 9 | Bits `40` and `80` may indicate armed off/on timers. | Current parser only checks the low nibble for half degrees. |
| Status 11–12 | Possible off-timer hours and minutes. | Both were `00` in the analyzed captures; not parsed. |
| Status 13–14 | Possible on-timer hours and minutes. | Both were `00` in the analyzed captures; not parsed. |
| Status 15 | Bit `80` may request an acknowledgement. | Always `00` in the analyzed captures; this component does not send that acknowledgement. |
| Status 17–18 | Possible room-temperature-related values. | They vary in the captures but are not decoded. [Another TCL implementation](https://github.com/Kannix2005/esphome-tcl-ac/blob/main/PROTOCOL.md#room-temperature) describes a different conversion, so the sensor and formula need validation. |
| Status 30 | Possible outlet temperature according to AC-hack. | Varies in the captures; the other TCL implementation describes an internal coil/pipe sensor instead. Sensor identity and conversion are not confirmed here. |
| Status 34 | Possible indoor-unit activity indication. | Varies in the captures; the workbook also considered an uncertain fan-speed correlation. |

To investigate one of these, capture several SETs and responses while changing **only that setting**, and repeat with the AC in different modes. For a suspected temperature sensor, compare the raw bytes with an independent thermometer over a range of temperatures; correlation at one temperature is not enough to identify a sensor or formula.

Known limits of the map:

- Most payload bytes are still unknown. `00` or a constant in these samples does not prove “reserved”.
- The builder creates a full snapshot but only preserves fields represented in this component. Other AC settings may be overwritten by its fixed values.
- The current code does not decode the room-temperature fields or implement timers, sleep, display control, or beeper control.
- `Stopped` uses zero for the selected axis without specifying a known fixed position; the exact physical stopping behavior is determined by the AC.
- The `0x09` response and other message types are not decoded by this component.
- Fixed-position numbers are UI labels; actual angles and the validity of the mapping on other AC models remain unverified.
