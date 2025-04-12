# DcDc converter control software

This repository contains a dc-dc converter control software which is intended to run on a raspberry-pi pico,
and is buildable only the pico-sdk.

The basis for this project are the [wlan freertos examples from the pico-example repository](https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/freertos).

## Build instructions

This project does require to have the pico_sdk installed, as well as the [Free-RTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main) downloaded
in some folder on the machine.

To build then simply run the following commands:
```bash
mkdir build
cd build
cmake .. -DFREERTOS_KERNEL_PATH=<PATH_TO_YOUR_RTOS_KERNEL_FOLDER>
make -j12
```

