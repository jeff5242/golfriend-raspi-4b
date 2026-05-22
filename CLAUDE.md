# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Golfriend Raspi-4B** is a PyQt5 kiosk application for golf driving range ball dispensing machines. It runs on Raspberry Pi (linaro/pi users) and can be developed/tested on macOS. The app displays a standby screen, accepts a transaction code via barcode scanner or keypad, communicates with a remote API to authorize ball dispensing, and controls a physical ball motor via GPIO.

## Running the Application

```bash
# On Raspberry Pi (production)
cd /home/linaro/Desktop/golfsrc/
python3 MediaPlayer.py

# Via startup script (disables screen sleep, sets DISPLAY)
bash run_golfsrc.sh

# On macOS (development) — GPIO is mocked automatically
cd Desktop/golfriend4b/
python3 MediaPlayer.py
```

## Dependencies

- Python 3
- PyQt5 (GUI framework)
- `requests` (HTTP API calls)
- `RPi.GPIO` (production only; auto-mocked on macOS via `MockGPIO` in `BallControl.py`)

## Architecture

All source lives in `Desktop/golfriend4b/`. There is no package manager or build system — files are deployed directly to the Raspberry Pi desktop.

### Core Files

| File | Purpose |
|------|---------|
| `MediaPlayer.py` | Main entry point. Contains `MediaPlayer` (fullscreen kiosk widget) and all dialog classes: `TakeBallDialog`, `TakeBallConfirmDialog`, `ErrorDialog`, `SuccessDialog` |
| `HttpCmdLibrary.py` | API client (`build` class). Handles `take_ball`, `take_ball_confirm`, `take_ball_set`, `take_ball_ping` endpoints against `golfpoint.milkidea.com` |
| `BallControl.py` | GPIO motor control. `BallControl` class drives pin 18 (output/motor) and reads pin 17 (light sensor). Includes macOS `MockGPIO` fallback |
| `log.py` | `mLog` wrapper around Python `logging` with `RotatingFileHandler`. Writes to `golfTest.log` on the device desktop |

### Platform Detection

Each module independently detects the platform and sets `BASE_PATH` / `DEVICE_PATH`:
- **macOS**: `~/Desktop/golfsrc`
- **Linux (linaro)**: `/home/linaro/Desktop/golfsrc`
- **Linux (pi)**: `/home/pi/Desktop/.Project/MediaPlayer`

### User Flow (Ball Dispensing)

1. **Standby** — fullscreen `standby.jpg` image, focus enforced every 2-3 seconds
2. **Trigger** — barcode scanner sends keystrokes (digits + Enter/Tab/%) or user presses keys
3. **TakeBallDialog** — checks WiFi, displays transaction code input, 10s timeout
4. **API call** — `SendTakeBallCmd()` with SHA1-signed payload → server returns ball count, motor timing
5. **TakeBallConfirmDialog** — shows ball count, auto-confirms after 2s (QR path) or waits for Enter
6. **API call** — `SendTakeBallConfirmCmd()` confirms the transaction
7. **SuccessDialog** — displays ball info, starts motor via `BallControl.StartMotor()`
8. **Motor control** — two modes: timed (`response_type != 2`) or plate-counting (`response_type == 2`)
9. **Return to standby**

### Special Inputs

- `7777777` in TakeBallDialog → `sudo shutdown -h now`
- `Escape` → quit application
- `Space` → pause/resume

### API Authentication

All API calls use SHA1 HMAC: `sha1("salt" + timestamp + transaction_code + device_code)`. The device code is read from `Desktop/DeviceName` file.

## Versioned Backup Files

Files like `MediaPlayer-v1*.py`, `MediaPlayer-v2*.py`, etc. are historical snapshots with Chinese descriptions of known issues in their filenames. `MediaPlayer.py` is always the current working version.

## Hardware

- **Motor output**: GPIO pin 18 (BCM mode)
- **Light sensor input**: GPIO pin 17 (BCM mode)
- Ball dispensing: motor runs for a calculated duration or counted plate cycles
