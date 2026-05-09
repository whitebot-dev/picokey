# PicoKey — Remote HID Keyboard System

## Architecture
```
[Your Computer] <──USB HID──> [Pico] <──UART TX/RX──> [ESP8266] <──WiFi──> [Internet]
                                                                               ↕
                                                                    [PHP Server / Your Domain]
                                                                               ↕
                                                                    [Web Dashboard (mobile/PC)]
```

## Wiring
```
Pico GP0 (TX) ──────────────── ESP8266 RX (D7/GPIO13)
Pico GP1 (RX) ──────────────── ESP8266 TX (D8/GPIO15)
Pico GND      ──────────────── ESP8266 GND
Pico VSYS(3.3V) ─── (optional) ESP8266 3.3V (if not powered separately)
```
> ⚠️ Logic levels: Both Pico and ESP8266 are 3.3V — no level shifter needed.

---

## File Structure
```
/
├── pico_main.py                  → Copy to Pico as main.py
├── esp8266_firmware/
│   └── esp8266_bridge.ino        → Flash to ESP8266 via Arduino IDE
└── web/
    ├── index.html                → Dashboard UI
    └── api.php                   → PHP backend
```

---

## Setup Steps

### 1. Pico Setup
1. Install CircuitPython on Pico (download from circuitpython.org)
2. Install libraries via CIRCUITPY/lib/:
   - `adafruit_hid` (from Adafruit CircuitPython HID bundle)
3. Copy `pico_main.py` → `CIRCUITPY/main.py`

### 2. ESP8266 Setup
1. Install Arduino IDE + ESP8266 board package
2. Install libraries: `ArduinoJson`, `ESP8266HTTPClient`
3. Edit `esp8266_bridge.ino`:
   - Set `WIFI_SSID` and `WIFI_PASS`
   - Set `SERVER_URL` to `https://yourdomain.com/pico/api.php`
   - Set `DEVICE_TOKEN` (random 32-char string, same in api.php)
4. Flash to ESP8266

### 3. Server Setup (Shared Hosting)
1. Create folder `/public_html/pico/` on your host
2. Upload `api.php` and `index.html` to that folder
3. Create writable `data/` subfolder: `chmod 750 data/`
4. Edit `api.php`:
   - Set `TOKEN` to same value as ESP8266 firmware
5. First login password is `picokey` — change it via:
   ```php
   // Run once to reset password:
   file_put_contents('data/auth.json', json_encode(['hash' => password_hash('YOUR_NEW_PW', PASSWORD_BCRYPT)]));
   ```

### 4. Usage
- Open `https://yourdomain.com/pico/` on any device
- Login with your password
- Type in the text field → text types on the Pico-connected computer
- Use modifier hold buttons to build combos
- Add custom macros (Win+R, etc.) via "+ Add Macro"

---

## API Reference (ESP8266 ↔ PHP)
| Action     | Method | Auth   | Description              |
|------------|--------|--------|--------------------------|
| poll       | GET    | Token  | Get next queued command  |
| heartbeat  | POST   | Token  | Device keepalive         |
| send       | POST   | Session| Enqueue command          |
| status     | POST   | Session| Device online status     |
| get_combos | POST   | Session| Fetch saved macros       |
| save_combos| POST   | Session| Save macros              |
| clear_queue| POST   | Session| Empty command queue      |
| login      | POST   | —      | Authenticate dashboard   |

## Command JSON Format (Pico protocol)
```json
{"t":"text","v":"hello world"}
{"t":"tap","v":"ENTER"}
{"t":"combo","v":["CTRL","C"]}
{"t":"combo","v":["WIN","R"]}
```

## Supported Keys
Modifiers: CTRL SHIFT ALT WIN RALT RCTRL RSHIFT RWIN  
Navigation: UP DOWN LEFT RIGHT HOME END PGUP PGDN  
Special: ENTER ESC TAB BKSP DEL INS SPACE CAPS PRTSCR SCRLOCK PAUSE NUMLOCK  
Function: F1–F12  
Letters: A–Z (combine with modifiers for shortcuts)
