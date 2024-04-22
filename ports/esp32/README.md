# Bluebomb Micro ESP32
This is a port of BlueBomb Micro for the ESP32.

## Requirements
- An ESP32 with BR/EDR (Classic) support. A BLE only controller will not work.
- The latest [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

## Building from source
1. Navigate to this directory from the bluebomb_micro root (e.g. `cd ports/esp32`).
1. Integrate the custom btstack by running `python ../../external/btstack/port/esp32/integrate_btstack.py`.
1. Build the project by running `idf.py build -DBLUEBOMB_TARGET="WII_SM4_3J"`.  
    Replace `WII_SM4_3J` with one of the supported target systems matching your console (e.g. `MINI_SM_PAL`, `MINI_SM_NTSC`, `WII_SM4_3E`, ...).

## Flashing
To flash the binary built in the previous step, run the following command:
```bash
idf.py -p PORT flash
```
Replace `PORT` with your ESP32 board's USB port name. See the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#build-your-first-project) for more information.
