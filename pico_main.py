"""
Raspberry Pi Pico - HID Keyboard + UART Bridge v2
Receives JSON commands from ESP8266 via UART → executes as HID keyboard

WIRING:
  Pico GP0 (TX) → ESP8266 D6 (GPIO12) RX
  Pico GP1 (RX) ← ESP8266 D5 (GPIO14) TX
  GND ←→ GND

BAUD: 9600 (matches ESP SoftwareSerial reliable limit)

Install in CIRCUITPY/lib/:
  adafruit_hid (from https://github.com/adafruit/Adafruit_CircuitPython_HID)
"""
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode
import busio
import board
import json
import time

# UART to ESP8266 — 9600 baud matches SoftwareSerial on ESP
uart = busio.UART(board.GP0, board.GP1, baudrate=9600, timeout=0.05)

kbd = Keyboard(usb_hid.devices)
layout = KeyboardLayoutUS(kbd)

# Key name → Keycode map
KEY_MAP = {
    # Modifiers
    "CTRL": Keycode.LEFT_CONTROL, "SHIFT": Keycode.LEFT_SHIFT,
    "ALT": Keycode.LEFT_ALT, "WIN": Keycode.LEFT_GUI,
    "RCTRL": Keycode.RIGHT_CONTROL, "RSHIFT": Keycode.RIGHT_SHIFT,
    "RALT": Keycode.RIGHT_ALT, "RWIN": Keycode.RIGHT_GUI,
    # Function keys
    "F1": Keycode.F1, "F2": Keycode.F2, "F3": Keycode.F3, "F4": Keycode.F4,
    "F5": Keycode.F5, "F6": Keycode.F6, "F7": Keycode.F7, "F8": Keycode.F8,
    "F9": Keycode.F9, "F10": Keycode.F10, "F11": Keycode.F11, "F12": Keycode.F12,
    # Navigation
    "UP": Keycode.UP_ARROW, "DOWN": Keycode.DOWN_ARROW,
    "LEFT": Keycode.LEFT_ARROW, "RIGHT": Keycode.RIGHT_ARROW,
    "HOME": Keycode.HOME, "END": Keycode.END,
    "PGUP": Keycode.PAGE_UP, "PGDN": Keycode.PAGE_DOWN,
    # Special
    "ENTER": Keycode.ENTER, "ESC": Keycode.ESCAPE, "TAB": Keycode.TAB,
    "BKSP": Keycode.BACKSPACE, "DEL": Keycode.DELETE, "INS": Keycode.INSERT,
    "SPACE": Keycode.SPACEBAR, "CAPS": Keycode.CAPS_LOCK,
    "PRTSCR": Keycode.PRINT_SCREEN, "SCRLOCK": Keycode.SCROLL_LOCK,
    "PAUSE": Keycode.PAUSE, "NUMLOCK": Keycode.KEYPAD_NUMLOCK,
    # Letters A-Z — key must be exactly the letter string e.g. "F" not "F_KEY"
    "A": Keycode.A, "B": Keycode.B, "C": Keycode.C, "D": Keycode.D,
    "E": Keycode.E, "F": Keycode.F, "G": Keycode.G, "H": Keycode.H,
    "I": Keycode.I, "J": Keycode.J, "K": Keycode.K, "L": Keycode.L,
    "M": Keycode.M, "N": Keycode.N, "O": Keycode.O, "P": Keycode.P,
    "Q": Keycode.Q, "R": Keycode.R, "S": Keycode.S, "T": Keycode.T,
    "U": Keycode.U, "V": Keycode.V, "W": Keycode.W, "X": Keycode.X,
    "Y": Keycode.Y, "Z": Keycode.Z,
    # Digits
    "0": Keycode.ZERO,  "1": Keycode.ONE,  "2": Keycode.TWO,
    "3": Keycode.THREE, "4": Keycode.FOUR, "5": Keycode.FIVE,
    "6": Keycode.SIX,   "7": Keycode.SEVEN,"8": Keycode.EIGHT,"9": Keycode.NINE,
}

buf = b""

def resolve(k):
    """Return Keycode for a key name string, or None."""
    return KEY_MAP.get(k.upper())

def press_keys(keys_list):
    """Resolve list of key name strings → press all together → release."""
    codes = []
    for k in keys_list:
        kc = resolve(k)
        if kc is not None:
            codes.append(kc)
        else:
            uart.write(('{"warn":"unknown key:' + str(k) + '"}\n').encode())
    if codes:
        kbd.release_all()        # clear any stuck keys first
        time.sleep(0.02)
        kbd.press(*codes)
        time.sleep(0.08)         # hold long enough for OS to register
        kbd.release_all()
        time.sleep(0.02)         # gap between consecutive commands

def process_command(cmd):
    try:
        data = json.loads(cmd)
        ctype = data.get("t")
        val   = data.get("v")

        if ctype == "text":
            layout.write(str(val))

        elif ctype in ("key", "combo"):
            # val is always a list
            if isinstance(val, list):
                press_keys(val)

        elif ctype == "tap":
            # val is a string (single key) or list
            if isinstance(val, list):
                press_keys(val)
            else:
                press_keys([str(val)])

    except Exception as e:
        uart.write(('{"err":"' + str(e) + '"}\n').encode())


print("Pico HID Keyboard bridge ready")
uart.write(b'{"status":"ready"}\n')

while True:
    data = uart.read(64)
    if data:
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.strip()
            if line:
                process_command(line.decode("utf-8", "ignore"))
    time.sleep(0.001)
