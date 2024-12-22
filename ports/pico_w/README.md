# Bluebomb Micro Pico W
This is a port of BlueBomb Micro for the Raspberry Pico W / Pico 2 W.

## Requirements
- A Raspberry Pi Pico W / Pico 2 W (the non-W variant will not work).

## Using the pre-compiled binaries
1. Download the latest binaries from the [releases](https://github.com/GaryOderNichts/bluebomb_micro/releases) page.
1. Plug in your Pico W to your PC while holding down the `BOOTSEL` button.  
    Your Pico should now show up as a drive.
1. Unzip the downloaded file and navigate to the `ports/pico_w` directory.  
    For the Pico W open the `build_pico_w` folder.  
    For the Pico 2 W open the `build_pico2_w` folder.
1. Copy the file which matches your Wii to the drive.  
    If you have a European Wii running version 4.3, you would copy `bluebomb_WII_SM4_3E.uf2`,  
    if you have a European Wii Mini, you would copy `bluebomb_MINI_SM_PAL.uf2`, etc...
1. The drive should disconnect and your Pico is ready.

## Building from source
1. Set up the [pico-sdk](https://github.com/raspberrypi/pico-sdk).
1. Create a `build` directory and navigate into it.
1. Run `cmake .. -DBLUEBOMB_TARGET="WII_SM4_3J" -DPICO_BOARD="pico_w"`  
    Replace `WII_SM4_3J` with your target system.  
    Replace `pico_w` with your target board (`pico_w`/`pico_2w`).
1. Run `make`
