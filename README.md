# Rotenso AC ESPHome external component
This repository provides custom external component to control Rotenso AC and probably other daughter-brands from TLC ACs.

It could also allow to control other AC units brands if your AC has this WiFi Smart module

[TCLWBR](https://developer.tuya.com/en/docs/iot/tclwbr-datasheet?id=Kcqmpgs2yc5c6)

![image](https://github.com/user-attachments/assets/a02fddab-9535-4807-8265-efb2782c52a5)

## Support
This repository will be open-source for ever. I did this component for my personal purposes but I think many other may benefit from it.
If you like this project and even helped you, you can consider supporting it by buying me a coffe ;)



## How I did this
By reverse engineering hundreds of data packets from WiFi module ;) Change one option in SmartLife app at a time, observe changes in frames, and deduct meaning of bytes ( so far it works :) )

## Tested with AC:

- [x] Rotenso Elis Series 3,5kW


## Todo:
- [ ] Correctness check of preset and mode selection
- [ ] fix 0.5*C changes
- [ ] Horizontal Vane control
- [ ] Vertical Vane control
- [ ] 
