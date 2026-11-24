# Rotenso AC ESPHome

This project provides custom external component to control Rotenso AC and probably other daughter-brands from TCL ACs ([midea component](https://esphome.io/components/climate/midea.html) is not working for this AC brand, completely different data packets).

> [!WARNING]
> **It's still work in progress.**
> 
> _I don't guarantee that using this external component won't damage your AC (It shouldn't though, if you send wrong data packet then AC won't even budge, ask me how I know it)_

## [Support](https://buymeacoffee.com/pabllo)

> [!NOTE]
> This project will be open-source forever. I did this component for my personal purposes but I think many other may benefit from it.
> 
> If you like this project and want to support it, you can consider [buying me a coffee](https://buymeacoffee.com/pabllo)
> 
> You can also support this project by letting me know, for which AC brands/models it also works -> [See](#check-for-this-wifi-module-if-you-have-it-then-itll-problably-will-work-for-you-too)
> 
![image](https://github.com/user-attachments/assets/4af840c1-f809-4a2d-9668-7374ca7e2d52)
> 
## How I did this

By reverse engineering hundreds of data packets from WiFi module ;) Change one option in SmartLife app at a time, observe changes in frames, and deduct meaning of bytes ( so far it works :) )

For the current command and status byte maps, captured examples, and remaining unknowns, see [FRAME_MAP.md](FRAME_MAP.md). The original captured packets are in [Rotenso.xlsx](Rotenso.xlsx).

## Requirements
- `ESPHome 2025.9` or higher
  
## Tested with AC

- [x] Rotenso Elis Series 3,5kW (Module TCLWBR 1.0.0)
- [x] Rotenso Roni Series 3,5kW (Module TCLWBR 1.0.0)


## What's working

- [x] Receiving state updates
- [x] Mode selection (Off, Cool, Heat, Fan)
- [x] Preset selection (Eco, Turbo)
- [x] Set fan speed (Auto, Low, Mid, High)
- [x] Set temperature with decimal (0.5\*C changes)
- [x] Vertical and horizontal vane position and swing range

## ToDo:

### ESPHome

- [ ] Expose Dry Mode

### Common AC functionality:

- [ ] Custom Fan speed setting (Fan speed from 1-5, auto) instead of mapping to ESPHome default ones
- [ ] On/Off display on indoor unit
- [ ] On/Off Beeper
- [ ] Function Sleep

### For some AC Units:

- [ ] I Feel (reading temp from remote)
- [ ] Smart 8\*C
- [ ] Anti-Mildew
- [ ] Self Clean
- [ ] Gentle wind (vertical flaps close with extra steps)
- [ ] Health (Air purification)

## ESPHome setup
Add the component source, UART connection, and climate platform to your ESPHome device YAML. ESPHome downloads the component from GitHub when it validates or builds the configuration; you do not need to copy its files manually.

```yml
external_components:
  - source: github://Puebllo/rotenso-climate@new_swings

# GPIO1/GPIO3 are also the default serial logger pins on an ESP8266.
# Disable serial logging when the AC uses that UART (API logs still work).
logger:
  baud_rate: 0

uart:
  id: uart_bus
  tx_pin: 1 # default pin
  rx_pin: 3 # default pin
  baud_rate: 9600
  parity: EVEN

climate:
  - platform: rotenso
    uart_id: uart_bus
    id: rotenso_climate
    name: "Rotenso AC"
```

The example uses the `new_swings` branch. Once this component has been merged into the repository's default branch, you can omit `@new_swings` to use that branch instead:

```yaml
external_components:
  - source: github://Puebllo/rotenso-climate
```

Adjust the UART pins for your hardware, then build and flash the firmware. ESPHome checks the Git repository for updates once a day by default; a component update takes effect on the device only after you rebuild and flash it. See [ESPHome external components](https://esphome.io/components/external_components/) for branch, tag, and refresh options.

If you already have a `logger:` section, add `baud_rate: 0` to that section instead of adding a second one. Serial logging must be disabled whenever it shares the UART connected to the AC; otherwise log messages can interfere with the commands. Logs are still available over the ESPHome API.

The component waits for a valid AC state before sending a change, and waits for the control response before sending the next change. If no control response arrives it retries once and then requests a fresh state. Look for `No control response` or `AC control response differs from the requested state` in the ESPHome logs when diagnosing a change that the AC does not apply.

### Local installation

To keep using a local component, copy the repository's `components/rotenso` directory to `custom_components/rotenso` beside your ESPHome YAML file. The `custom_components/rotenso` directory must directly contain `__init__.py`, `climate.py`, and the C++ files. Use this source in place of the GitHub source above:

```yaml
external_components:
  - source:
      type: local
      path: ./custom_components
    components: [rotenso]
```

For an existing local installation, update the copied files from `components/rotenso` or switch to the GitHub source; copying the repository root into `custom_components/rotenso` will no longer work.

### Vane controls

Each climate entity automatically creates two Home Assistant select entities, named `<climate name> Vertical vane` and `<climate name> Horizontal vane`. They expose these choices:

| Vane | Fixed positions | Swing ranges |
| --- | --- | --- |
| Vertical | 1–5, where 1 is up and 5 is down | 1–3, 3–5, 1–5 |
| Horizontal | 1–5, where 1 is left and 5 is right | 1–5, 1–3, 2–4, 3–5 |

Vertical swing range 1–3 covers the upper half and 3–5 the lower half.

`Stopped` turns off movement without selecting a fixed position. `Unknown` is a status indicator used when the AC reports an unrecognized position or swing range; it is not a command. The selectors update from the AC's status, including changes made using the remote.

The climate entity also offers the standard Off, Vertical, Horizontal, and Both swing modes. Enabling an axis that was stopped uses its full 1–5 range; an axis that was already swinging keeps its selected range. Changes to temperature, mode, or fan speed preserve the reported vane settings.

The detailed positions and ranges appear in the two **separate select entities**, not in the climate card's swing menu. In Home Assistant, open **Settings → Devices & services → ESPHome → your device** and look for the Vertical vane and Horizontal vane controls. You can add them to an Entities card alongside your climate entity:

```yaml
type: entities
title: Rotenso AC
entities:
  - climate.rotenso_ac
  - select.rotenso_ac_vertical_vane
  - select.rotenso_ac_horizontal_vane
```

Replace these example entity IDs with the IDs shown for your device in Home Assistant. If the selects are missing, ensure the ESPHome configuration uses this version of the component, then rebuild and flash the firmware; updating the repository alone does not update an installed device.

To customize the select names or assign IDs for ESPHome automations, add these optional entries under the existing climate configuration:

```yaml
climate:
  - platform: rotenso
    uart_id: uart_bus
    name: "Rotenso AC"
    vertical_swing_select:
      name: "Vertical vane"
      id: vertical_vane
    horizontal_swing_select:
      name: "Horizontal vane"
      id: horizontal_vane
```


## Hardware

tbc.

## Check for this WiFi module, if you have it, then it'll problably will work for you too

My AC is using generic TCLWBR Tuya module so my guess is, that if your AC is using this module, then this component should work you too.

[TCLWBR](https://developer.tuya.com/en/docs/iot/tclwbr-datasheet?id=Kcqmpgs2yc5c6)

![image](https://github.com/user-attachments/assets/ab24b9a1-d243-4316-a81d-9bce5a8e177e)


![image](https://github.com/user-attachments/assets/a02fddab-9535-4807-8265-efb2782c52a5)
