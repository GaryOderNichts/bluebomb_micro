# BlueBomb Micro (BlueBomb for embedded systems)
This is a port of [BlueBomb](https://github.com/Fullmetal5/bluebomb) for BlueKitchen's [btstack](https://github.com/bluekitchen/btstack), which runs on various embedded systems and microcontrollers.

## Get started
Follow the guide for one of the supported systems below to set up BlueBomb Micro for your microcontroller.

### Supported systems
- [Raspberry Pi Pico W](ports/pico_w/)
- [ESP32](ports/esp32/)

### Usage
1. Make sure your microcontroller is plugged in and BlueBomb Micro running.  
    If your microcontroller has an LED, it will be slowly flashing at this point.
1. Start your Wii and navigate to the app that you are exploiting, for the System Menu you only need to turn on the Wii, you can leave it sitting on the Health and Safety screen.
1. __Turn OFF your wiimote at this point. DO NOT let anything else connect to the console via bluetooth.__
1. Make sure you console is close to your microcontroller. You may have to move it closer to get it in range, this will depend on your microcontroller.
1. Click the SYNC button on your console. You may have to click it several times in a row before it sees the microcontroller.  
    If your microcontroller has an LED, you will know it is connected when the LED starts flashing quickly.
    Stop pushing the SYNC button and wait for bluebomb to run.
    BlueBomb Micro will load a `boot.elf` off the root of a FAT32 formatted USB drive and run it. You can use the HackMii Installer's boot.elf from [here](https://bootmii.org/download/) to get the Homebrew Channel.

## Credits
- [@Fullmetal5](https://github.com/Fullmetal5) for BlueBomb
- [BlueKitchen](https://bluekitchen-gmbh.com/) for their btstack
