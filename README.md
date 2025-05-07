# Rotenso AC ESPHome
This project provides custom external component to control Rotenso AC and probably other daughter-brands from TLC ACs ([midea component](https://esphome.io/components/climate/midea.html)  is not working for this AC brand, completely different data packets). 

**It's still work in progress.**

*I don't guarantee that using this external component won't damage your AC (It shouldn't though, if you send wrong data packet then AC won't even budge, ask me how I know it)*

## [Support](https://buymeacoffee.com/pabllo)

![image](https://github.com/user-attachments/assets/4af840c1-f809-4a2d-9668-7374ca7e2d52)

This project will be open-source forever. I did this component for my personal purposes but I think many other may benefit from it.

If you like this project and want to support it, you can consider [buying me a coffee](https://buymeacoffee.com/pabllo)

You can also support this project by letting me know, for which AC brands/models it also works

## How I did this
By reverse engineering hundreds of data packets from WiFi module ;) Change one option in SmartLife app at a time, observe changes in frames, and deduct meaning of bytes ( so far it works :) )

## Tested with AC:
- [x] Rotenso Elis Series 3,5kW

## What's working
- [x] Preset and mode selection
- [x] Fan speed 
- [x] Receiving state updates

## ToDo:
- [ ] fix 0.5*C changes
- [ ] Horizontal Vane control
- [ ] Vertical Vane control
- [ ] Seperate Fan speed setting - (Fan speed from 1-5, auto)
- [ ] Better logging

## Hardware
tbc.


## Check for this WiFi module, if you have it, then it'll problably will work for you too

My AC is using generic TLCWBR Tuya module so my guess is, that if your AC is using this module too, it should work

[TCLWBR](https://developer.tuya.com/en/docs/iot/tclwbr-datasheet?id=Kcqmpgs2yc5c6)

![image](https://github.com/user-attachments/assets/a02fddab-9535-4807-8265-efb2782c52a5)


