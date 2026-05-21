# ESP-SR Wake Word With INMP441 Microphone

Minimal ESP32-S3 / ESP-IDF project for testing Espressif ESP-SR wake-word and speech-command recognition with an INMP441 I2S microphone.

The firmware:

- Initializes an INMP441 microphone through the ESP32-S3 I2S peripheral.
- Captures 16 kHz mono audio in 32-bit I2S slots and converts it to 16-bit PCM.
- Loads ESP-SR models from the `model` partition.
- Uses WakeNet keyword `hiesp`, which listens for "Hi ESP".
- After wake-word detection, runs the English MultiNet command recognizer.
- Prints RMS level, wake-word state, and recognized command strings/IDs to the serial log.

This project does not connect to Wi-Fi, call cloud APIs, or store credentials.

## Hardware

Target: ESP32-S3 with enough flash/PSRAM for ESP-SR models. The default config assumes 16 MB flash and PSRAM.

INMP441 wiring used by `main/pins.h`:

| INMP441 pin | ESP32-S3 pin |
| --- | --- |
| VDD | 3V3 |
| GND | GND |
| SCK / BCLK | GPIO 14 |
| WS / LRCLK | GPIO 15 |
| SD / DOUT | GPIO 16 |
| L/R | GND for left channel |

`I2S_MCLK` is set to `-1` because the INMP441 does not need MCLK.

If your microphone L/R pin is tied high instead of GND, change the I2S slot mask in `main/main.c` from `I2S_STD_SLOT_LEFT` to `I2S_STD_SLOT_RIGHT`.

## Build And Flash

Install ESP-IDF, then from this repo:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

Replace `COM5` with the serial port for your board.

After flashing, open the serial monitor and say:

```text
Hi ESP
```

Expected log messages include:

- `I2S RMS ~= ...` while audio is being captured.
- `Wake word detected! Listening...` after the wake word.
- `Command: ...` if MultiNet recognizes a supported command.

## Project Layout

- `main/main.c` contains the I2S microphone setup, ESP-SR model loading, AFE feed loop, wake-word detection, and command recognition loop.
- `main/pins.h` contains the active INMP441 pin mapping and speech audio constants.
- `main/idf_component.yml` declares the ESP-SR dependency.
- `partitions.csv` creates a `model` SPIFFS partition for bundled speech-recognition models.
- `sdkconfig.defaults` enables the custom partition table, ESP32-S3 PSRAM settings, WakeNet `Hi ESP`, and English MultiNet.

Generated folders such as `build/` and `managed_components/` are intentionally ignored by git. ESP-IDF recreates them during build.

## Adding This To Another ESP-IDF Project

Copy or merge these pieces into the target project:

1. Add `espressif/esp-sr` to the component manifest, like `main/idf_component.yml`.
2. Include the custom model partition from `partitions.csv`, or merge the `model` partition into your existing partition table.
3. Copy the relevant settings from `sdkconfig.defaults`, especially the ESP-SR model selections and PSRAM settings.
4. Copy the I2S setup and ESP-SR feed/detect loop from `main/main.c`.
5. Update the GPIO defines in `main/pins.h` to match your board wiring.

## Notes

- The app only logs recognized commands. It does not trigger GPIOs, display UI, or control devices yet.
- The active wake-word model is selected by `MY_WAKENET_KEYWORD` in `main/main.c`.
- The active command model is selected by `MY_MULTINET_KEYWORD` in `main/main.c`.
- If RMS values stay near zero, check microphone power, ground, BCLK, WS, data wiring, and left/right channel selection.
