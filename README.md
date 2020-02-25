# Pokemon Sword and Shield Auto-Breeder

A fork of a fork of a fork of a fork, modified to grind automate the breeding process in pokemon sword and shield. Breeding faster makes it easier to both find high IV pokemon and shiny pokemon with less effort.

## Setup Instructions for an Arduino R3:
1. Download the LUFA library here ([LUFA DOWNLOAD](http://www.fourwalledcubicle.com/LUFA.php)) and extract it's contents in the LUFA folder of the project

2. Install packages needed for flashing the Arduino
    - For Debian/Ubuntu:
      - GCC components specific to avr: `sudo apt-get install gcc-avr binutils-avr avr-libc gdb-avr`
      - Potentially not used, but haven't verified doing things without it yet: `sudo apt-get install avrdude avrdude-doc`
      - Gives us everything needed to use `make`: `sudo apt-get install build-essential`
      - Program that allows us to flash the DFU chip/USB controller on the Arduino: `sudo apt-get install dfu-programmer`

3. Set the Arduino into DFU mode:
    - https://www.arduino.cc/en/Hacking/DFUProgramming8U2

4. Select the mode that you would like the arduino to run in by changing the mode variable which can be found in [Joystick.c](https://github.com/shinyquagsire23/Switch-Fightstick/blob/master/Joystick.c).

    - COLLECTING: Repeatedly grabs eggs from the day care lady on route 5.
      - Configure the number of eggs to collect by adjusting the `eggsToCollect` variable in Joystick.c.
    - COLLECT_THEN_HATCH: Collect a number of eggs from the day care lady, then
      hatch them.
      - Configure the number of eggs to collect by adjusting the `eggsToCollect` variable in Joystick.c.
        The number of eggs to hatch will be determined from this, rounded down to the nearest box.
    - HATCHING: Repeatedly hatches boxes of pokemon.
      - The number of boxes to hatch can be configured by changing the `boxesToHatch` variable in Joystick.c.
    - (NOT READY) RELEASING: Releases boxes of pokemon. Set the variable numBoxes to the number of boxes you would like to relase. (NOTE: only works if releasing full boxes of pokemon)
4. In terminal navigate to the inside of the project directory

5. In the directory containing our makefile: `make`. This will create Joystick.hex in the working dir.

6. Flash the arduino with the code by entering the following commands in terminal.

```
sudo dfu-programmer atmega16u2 erase
sudo dfu-programmer atmega16u2 flash Joystick.hex
sudo dfu-programmer atmega16u2 reset
```
NOTE: To return your arduino to default condition, flash it with "Arduino-usbserial-uno.hex" from [this repo](https://github.com/arduino/ArduinoCore-avr/tree/master/firmwares/atmegaxxu2/arduino-usbserial).

7. Make sure that you are standing in the appropriate area depending on the mode that you are trying to run the arduino in.
    - Collecting:
        - Location: Stand slightly to the left of the daycare lady on route 5
        - Menu status: Make sure the menu cursor is over the "Pokemon" tab in the x menu.
        - Text speed: Fast
    - Hatching:
        - Location: Start facing the wall to the left of the daycare in the wild area.
        - Menu status: Also make sure that your menu cursor is hovering over pokemon, and then exit the menu.
        - Text speed: Fast
    - (NOT READY) Releasing:
        - Location: Make sure that your menu cursor is hovering over the pokemon option, and then exit the menu.
        - Menu Status: Then stand anywhere without the menus open.
        - Text speed: Fast

8. Make sure that no other controllers are connected to the switch besides the docked joycons.

9. Plug in the Arduino into the switch and let the hunt begin! This can be done through either a USB-C cable (needs testing) or by plugging the arduino directly into the dock.

#### Thanks

Thanks to https://github.com/bertrandom/snowball-thrower for the updated information which modifies the original script to throw snowballs in Zelda. This C Source is much easier to start from, and has a nice object interface for creating new command sequences.

Thanks to Shiny Quagsire for his [Splatoon post printer](https://github.com/shinyquagsire23/Switch-Fightstick) and progmem for his [original discovery](https://github.com/progmem/Switch-Fightstick).
