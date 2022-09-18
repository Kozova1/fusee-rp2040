# fusee-rp2040
This is an implementation of the fusée gelée exploit that runs on the Adafruit Feather RP2040.
It should also work with other similar RP2040 based boards - make sure you change LED_PIN in [src/fusee.c](src/fusee.c).  
It is heavily based on the [excellent implementation of fusee-launcher by Qyriad](https://github.com/Qyriad/fusee-launcher),
as well as on the [implementation of fusee-launcher for samd21 by blockfeed](https://github.com/blockfeed/sam-fusee-launcher-internal/).

At the moment, it requires a patched TinyUSB implementation, because of hathach/tinyusb#1097.

## Build
To build fusee-rp2040, first install the [Pico SDK](https://github.com/raspberrypi/pico-sdk), as outlined in [this Getting Started document](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

After you are done, make sure you set the `PICO_SDK_PATH` environment variable to the correct value, and run these commands:
```sh
git clone https://github.com/Kozova1/fusee-rp2040
cd fusee-rp2040
mkdir build
cd build
cmake ..
make -j$(nproc)
```

After that, copy `build/fusee.uf2` to your board's internal memory, and it should hopefully work.