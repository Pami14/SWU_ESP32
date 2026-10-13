# SilkWindUp ESP32: Toolchain

ESP32-WROOM-32 project built with **ESP-IDF v5.4** (C++) inside a VS Code devcontainer on Windows.

## Overview

| Part | Tool |
|---|---|
| Target | ESP32 (Xtensa), `esp32` |
| Framework | ESP-IDF v5.4 |
| Compiler | `xtensa-esp32-elf-gcc/g++` 14.2.0 (installed by `install.sh`) |
| Build system | CMake + Ninja via `idf.py` |
| Environment | Docker devcontainer (Ubuntu 24.04) |
| Flashing | From Windows (COM port) or from the container via usbipd |

## Project layout

```
SilkWindUp_ESP32/
├── .devcontainer/
│   ├── Dockerfile
│   └── devcontainer.json
├── CMakeLists.txt
└── main/
    ├── CMakeLists.txt
    └── main.cpp
```

**`CMakeLists.txt`**
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(silkwindup)
```

**`main/CMakeLists.txt`**
```cmake
idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")
```

**`main/main.cpp`** (entry point is `app_main`, not `main`)
```cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

extern "C" void app_main()
{
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    while (true) {
        gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(GPIO_NUM_2, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## Dockerfile

```dockerfile
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV IDF_PATH=/opt/esp-idf
ENV IDF_TOOLS_PATH=/opt/esp-tools

RUN apt-get update && apt-get install -y --no-install-recommends \
    git wget flex bison gperf python3 python3-venv python3-pip \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 \
    ca-certificates gdb \
    && rm -rf /var/lib/apt/lists/*

RUN git clone -b v5.4 --depth 1 --recursive --shallow-submodules \
      https://github.com/espressif/esp-idf.git $IDF_PATH \
    && $IDF_PATH/install.sh esp32 \
    && rm -rf $IDF_TOOLS_PATH/dist

RUN echo '. /opt/esp-idf/export.sh' >> /root/.bashrc

WORKDIR /workspace
```

`IDF_TOOLS_PATH` must be set **before** `install.sh` runs.

## devcontainer.json

Without a device (build only, flash from Windows):
```json
{
    "name": "ESP32",
    "build": { "dockerfile": "Dockerfile" }
}
```

With the board passed through (needs `usbipd attach` before the container starts):
```json
{
    "name": "ESP32",
    "build": { "dockerfile": "Dockerfile" },
    "runArgs": [
        "--device=/dev/ttyUSB0:/dev/ttyUSB0",
        "--group-add=dialout"
    ]
}
```

## Build

```bash
idf.py set-target esp32
idf.py build
```

If `set-target` refuses to clean, delete the old build folder first: `rm -rf build`.

## Flash

**Option A: from the container** (board attached via usbipd)
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```
Exit the monitor with `Ctrl+]`.

**Option B: from Windows** (Docker cannot see COM ports)
```powershell
cd c:\projects\SilkWindUp_ESP32\build
esptool --chip esp32 --port COM3 --baud 460800 write_flash "@flash_args"
```
Use `write-flash` if your esptool version asks for it.

### usbipd (Windows, admin PowerShell)
```powershell
usbipd list
usbipd bind --busid <BUSID>      # once per device
usbipd attach --wsl --busid <BUSID>   # after every replug or reboot
```

## Windows prerequisites

- **CP210x driver** installed (Device Manager, Ports (COM & LPT), no "!").
- Data USB cable.
- At least 15-20 GB free on C: for the Docker build.
- `C:\Users\<you>\.wslconfig`:
  ```ini
  [wsl2]
  memory=8GB
  swap=8GB
  ```

## Troubleshooting

| Problem | Fix |
|---|---|
| `idf.py: command not found` | `. /opt/esp-idf/export.sh` |
| Container won't start (`/dev/ttyUSB0` not found) | `usbipd attach` first, or remove `runArgs` |
| `Cannot find source file main/main.cpp` | Move code to `main/main.cpp` |
| `doesn't seem to be a CMake build directory` | `rm -rf build` |
| Upload hangs on "Connecting..." | Hold **BOOT** while connecting |
| Docker engine returns 500 | Restart Docker Desktop, `wsl --shutdown`, free disk space |

## Porting notes (AVR to ESP32)

- ESP32 GPIO is **3.3 V** and not 5 V tolerant.
- Replace `<avr/io.h>`, `DDRB`, `PORTB`, `_delay_ms()` with `gpio_set_direction()`, `gpio_set_level()`, `vTaskDelay(pdMS_TO_TICKS(ms))`.
- GPIO 6-11 are used by flash and must not be used. GPIO 34-39 are input only.
- Default I2C: SDA 21, SCL 22. Default SPI (VSPI): MOSI 23, MISO 19, SCK 18, SS 5.