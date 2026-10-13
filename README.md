# SilkWindUp ESP32: Toolchain

ESP32-WROOM-32 project in C++ with **ESP-IDF v5.4**, built in a VS Code devcontainer (Ubuntu 24.04) on Windows.

## Project files

| Path | Purpose |
|---|---|
| `.devcontainer/Dockerfile` | Installs ESP-IDF and the Xtensa toolchain |
| `.devcontainer/devcontainer.json` | Devcontainer settings (optional `runArgs` for the USB device) |
| `CMakeLists.txt` | Top-level IDF project file |
| `main/CMakeLists.txt` | Registers `main.cpp` as the main component |
| `main/main.cpp` | Application code, entry point `app_main()` |

## Build

Open the project in the devcontainer, then:

```bash
idf.py set-target esp32
idf.py build
```

If `set-target` refuses to clean: `rm -rf build`.

## Flash

**From Windows** (Docker cannot see COM ports):

```powershell
cd build
esptool --chip esp32 --port COM3 --baud 460800 write_flash "@flash_args"
```

**From the container** (board attached with usbipd, `runArgs` set in `devcontainer.json`):

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

usbipd, in an admin PowerShell: `usbipd bind --busid <BUSID>` (once), then `usbipd attach --wsl --busid <BUSID>` (after every replug or reboot, before starting the container).

## Requirements

- CP210x driver installed, data USB cable
- 15-20 GB free on C:
- `.wslconfig` with `memory=8GB` and `swap=8GB`

## Troubleshooting

| Problem | Fix |
|---|---|
| `idf.py: command not found` | `. /opt/esp-idf/export.sh` |
| Container won't start (`/dev/ttyUSB0`) | `usbipd attach` first, or remove `runArgs` |
| `Cannot find source file main/main.cpp` | Code must be in `main/main.cpp` |
| `doesn't seem to be a CMake build directory` | `rm -rf build` |
| Upload stuck on "Connecting..." | Hold **BOOT** while connecting |
| Docker engine 500 error | Restart Docker Desktop, `wsl --shutdown`, free disk space |

## AVR to ESP32 notes

- GPIO is **3.3 V**, not 5 V tolerant
- Avoid GPIO 6-11 (flash); GPIO 34-39 are input only
- Default I2C: SDA 21, SCL 22
- No AVR headers or registers: use `driver/gpio.h` and `vTaskDelay(pdMS_TO_TICKS(ms))`