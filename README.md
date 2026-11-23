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
- [ ] Config repo, so ESPHome can fetch directly this component without manual upload to `config` dir

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
1. Download/clone this repo
2. In HomeAssistant Config directory, go to `esphome` and create there directory `custom_components` in which paste downloaded repo, rename folder to `rotenso`. See screen below

![image](https://github.com/user-attachments/assets/f6d4a096-1f95-4391-a281-8e65f34b43cf)

3. In ESPHome add following to yml:


```yml

external_components:
  - source: 
      type: local
      path: "./custom_components"
    components: [rotenso]

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

4. Flash firmware. Done

If you already have a `logger:` section, add `baud_rate: 0` to that section instead of adding a second one. Serial logging must be disabled whenever it shares the UART connected to the AC; otherwise log messages can interfere with the commands. Logs are still available over the ESPHome API.

The component waits for a valid AC state before sending a change, and waits for the control response before sending the next change. If no control response arrives it retries once and then requests a fresh state. Look for `No control response` or `AC control response differs from the requested state` in the ESPHome logs when diagnosing a change that the AC does not apply.

### Vane controls

Each climate entity automatically creates two Home Assistant select entities, named `<climate name> Vertical vane` and `<climate name> Horizontal vane`. They expose these choices:

| Vane | Fixed positions | Swing ranges |
| --- | --- | --- |
| Vertical | 1–5, where 1 is down and 5 is up | 1–3, 3–5, 1–5 |
| Horizontal | 1–5, where 1 is left and 5 is right | 1–5, 1–3, 2–4, 3–5 |

`Stopped` turns off movement without selecting a fixed position. `Unknown` is a status indicator used when the AC reports an unrecognized position or swing range; it is not a command. The selectors update from the AC's status, including changes made using the remote.

The climate entity also offers the standard Off, Vertical, Horizontal, and Both swing modes. Enabling an axis that was stopped uses its full 1–5 range; an axis that was already swinging keeps its selected range. Changes to temperature, mode, or fan speed preserve the reported vane settings.

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
