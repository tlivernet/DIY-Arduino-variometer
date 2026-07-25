# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single-sketch Arduino project: a low-cost paragliding variometer (climb/sink rate instrument) built on an Arduino Nano. Sensor data (BMP085 barometric pressure), a Nokia 5110 LCD, a rotary encoder, an RTC, and a piezo buzzer are combined to display altitude, vertical speed, time, temperature, and battery level, and to log flight statistics to EEPROM.

The entire application lives in one file: `Variometer/Variometer.ino`. There is no build system, package manager, or test suite — this is compiled and flashed via the Arduino IDE.

Bundled third-party libraries live under `libraries/` and are vendored copies (not git submodules), meant to be copied into the user's Arduino `libraries` folder:
- `Adafruit_BMP085_U`, `Adafruit_Sensor` — pressure/temperature sensor
- `Adafruit_GFX`, `Adafruit_PCD8544` — Nokia 5110 LCD graphics driver
- `Encoder` — rotary encoder input
- `MenuBackend` — menu tree/event state machine driving the UI
- `RTClib` — DS1307 real-time clock
- `EEPROMAnything` — templated helper for reading/writing arbitrary structs to EEPROM
- `toneAC` — piezo buzzer tone generation without an external amp

## Build / compile

There is no CLI build. Compilation and flashing happen through the Arduino IDE:

1. Copy the contents of `libraries/` into the Arduino IDE's own `libraries/` folder (e.g. `~/Documents/Arduino/libraries` or `C:\Users\<user>\Documents\Arduino\libraries`).
2. Open `Variometer/Variometer.ino` in the Arduino IDE.
3. **This sketch only compiles cleanly on Arduino IDE >= 1.5 and <= 1.6.1** — newer IDE versions produce compilation errors. This constraint is documented in the README and matters when troubleshooting build issues.
4. Use "Vérifier"/Verify to compile, "Téléverser"/Upload to flash to the board.

If there is no Arduino IDE available in this environment, compilation cannot actually be verified — say so rather than assuming success from a code read-through.

### First-flash EEPROM initialization

`Variometer.ino` has a top-level flag:

```cpp
bool initialisation = false;  // If true, reset and update eeprom memory at arduino start
```

On the very first flash to a new board, this must be set `true` to initialize the EEPROM-backed config/stats layout, then re-flashed with it set back to `false`. Leaving it `true` wipes EEPROM (config + stats) on every boot. Never commit a change that leaves this `true`.

## Architecture

Everything is in `Variometer/Variometer.ino` (~1130 lines), organized as one flat sketch with clearly delimited sections (search for the `//////` banner comments): RTC, ENCODER, MENU, ECRAN (screen), VARIO.

### Persistent state (EEPROM)

Two structs are packed into EEPROM via `EEPROMAnything`'s templated read/write:

- `struct Conf` (`conf`, at EEPROM offset 0) — user-configurable settings: climb/sink beep thresholds, altitude offset, backlight level, contrast, volume, QNH (sea-level pressure), and `stat_index` (which flight-record slot is active).
- `struct Stat` (`stat`, at offset `sizeof(Conf) + sizeof(Stat) * index`) — one flight's statistics (start time, duration, min/max altitude, max climb/sink rate, cumulative gain). Up to `NB_STATS` (8) slots are stored round-robin; `incrementStatIndex()` advances to the next slot without overwriting non-empty ones.

When editing either struct's layout, remember EEPROM offsets are computed from `sizeof()` — changing field types/order shifts every subsequent stat slot and effectively invalidates existing on-device EEPROM data (this is exactly why the `initialisation` re-flash step exists).

### Menu system

Built with `MenuBackend`, which models the UI as a graph of `MenuItem` nodes linked by `addAfter`/`addRight`/`addBefore`/`addLeft`/`addAfter` relationships (see `menuSetup()`). Each item carries a numeric "shortkey" (the `MENU_*` `#define`s) used inside `renderMenu()`'s switch statement to decide what to draw and which config field to edit. Two callbacks drive all interaction:

- `menuChangeEvent()` — fired when the encoder moves the cursor to a different menu item; re-renders the screen for the new item.
- `menuUseEvent()` — fired when the encoder's push-button selects/confirms an item; either moves through the left/right menu graph or toggles `menuUsed` (edit mode for the currently displayed value).

`renderMenu()` is the single large dispatcher that both draws the current menu screen and applies encoder-driven increments/decrements to whichever `conf.*` field is active (via the `updateConfItem()` helper), then persists to EEPROM once editing is confirmed.

`readButtons()` polls the `Encoder` object and the enter pin each loop iteration and translates rotation/press into `menu.moveUp()/moveDown()/use()` calls or, when a `Vario`/`Stat` sub-view is active, view-specific behavior (adjust volume, page through stat slots, long-press to reset current stat).

### Main loop (`loop()`)

Runs continuously and, each iteration:
1. Polls encoder/button (`readButtons()`).
2. Reads pressure and computes smoothed `Altitude` (`updateAltitude()`), then derives `vario` (vertical speed) from the altitude delta over elapsed time, exponentially smoothed.
3. If a flight is in progress (`stat.chrono_start != 0`), accumulates min/max altitude, max climb/sink rate, and cumulative altitude gain into `stat`.
4. Drives the buzzer (`makeBeeps()`), whose beep frequency/latency are functions of climb/sink rate (`getBeepFrequency()`, `getBeepLatency()`), gated by the configurable climb/sink thresholds.
5. Once per second: re-renders the vario screen (`renderVario()`), writes `conf` to EEPROM (throttled — only 5s after a change, via `push_write_eeprom`), and runs flight-detection logic: a flight "starts" when altitude departs a ~4m band from a baseline for 15s, and "ends" when altitude has stayed within a ~1.5m band for 15s, at which point stats are written to EEPROM and the active stat slot advances.

### Hardware pin mapping

Pin assignments are `#define`d near the top of the file (`PIN_SCLK`, `PIN_LIGHT`, `PIN_SCE`, `PIN_RESET`, `PIN_DC`, `PIN_SDIN` for the LCD; encoder on pins 3/2; `Enter` on pin 12). These must match the physical wiring — the README documents a pin-mapping change history (older revisions used different pins) since this reflects the real, hand-wired hardware, not just software config. If a user reports the display or encoder behaving as if wired backwards, check whether their build matches the pin `#define`s or the older/newer wiring diagram in `Variometer.png`.

## Language note

The README, in-UI menu strings, and many code comments are in French (this is a project from the French paragliding community). Keep user-facing strings (menu labels, on-screen text) in French to match the existing UI unless asked otherwise.

## Other branches

The README references an alternate version of this project using an MS5611 pressure sensor instead of the BMP085, on a separate `MS5611` branch in the upstream repo. That branch is not present locally by default — don't assume MS5611 support exists in this branch's code.
