# boot.py — place in CIRCUITPY root (alongside main.py)
# Runs before main.py at every boot.
# Disables USB mass storage so the Pico enumerates ONLY as HID keyboard
# on ALL hosts: Windows, macOS, Linux, Android, iOS OTG.
import usb_hid
import storage
import usb_cdc

# Disable the CIRCUITPY USB drive — host sees pure HID keyboard, no storage
storage.disable_usb_drive()

# Disable serial CDC (optional: keeps it for debugging via UART only)
# Comment next line out if you need serial debug via USB
usb_cdc.disable()

# Enable HID with keyboard only
usb_hid.enable((usb_hid.Device.KEYBOARD,))
