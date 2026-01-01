# Cardputer Adv Terminal

![Cardputer Adv Terminal](img\IMG_6746.jpeg)

Terminal emulator for ESP32-S3 Cardputer Adv. It shows serial data on the
built-in screen and sends keyboard input to USB CDC and/or UART.

## Features

- USB CDC and UART serial input/output
- Line mode (send on Enter)
- Local echo toggle
- ANSI filter toggle
- CRLF toggle (default ON)
- Status bar with live settings and connection state
- Font size toggle
- Screen frame/margins for comfortable reading

## Build and Upload (Arduino CLI)

Requirements:
- Arduino CLI installed
- ESP32 core installed with FQBN: esp32:esp32:m5stack_cardputer
- M5Cardputer library installed

Build:

```
make build
```

Upload:

```
make upload PORT=/dev/tty.usbmodemXXXX
```

Monitor (optional):

```
make monitor PORT=/dev/tty.usbmodemXXXX BAUD=115200
```

## Cardputer Controls

- FN+1: Change PORT (USB -> UART -> BOTH)
- FN+2: Change BAUD
- FN+3: Toggle ECHO
- FN+4: Toggle ANSI filter
- FN+5: Clear screen
- FN+6: Toggle CRLF
- FN+7: Toggle font size

Notes:
- Line mode is default: typed text is sent only after Enter.
- Status bar highlights values when they differ from defaults.

## Raspberry Pi Zero 2 W (USB host via OTG)

The Pi must act as USB host. Use the OTG port with an OTG adapter.

### OTG host mode (config + commands)

Enable USB host mode for the OTG port:

1) Edit `/boot/config.txt` and add:

```
dtoverlay=dwc2,dr_mode=host
```

2) Reboot:

```
sudo reboot
```

3) Verify host mode is working (after reboot, with Cardputer plugged in):

```
lsusb
dmesg | tail -n 50
```

1) Connect Cardputer -> Pi Zero 2 W OTG port.
2) Verify device appears on the Pi:

```
dmesg | tail -n 50
ls /dev/ttyACM* /dev/ttyUSB*
```

You should see `/dev/ttyACM0`.

3) Start a getty on `/dev/ttyACM0` so the login banner appears on the
   Cardputer:

```
sudo systemctl enable --now serial-getty@ttyACM0.service
```

4) Make it auto-start on every plug (udev rule):

Create `/etc/udev/rules.d/99-ttyACM0-getty.rules` with:

```
ACTION=="add", KERNEL=="ttyACM0", TAG+="systemd", ENV{SYSTEMD_WANTS}="serial-getty@ttyACM0.service"
```

Reload udev and reconnect:

```
sudo udevadm control --reload
sudo udevadm trigger
```

Now the Cardputer should show the Debian login banner whenever it is plugged
into the Pi.

## UART (TX/RX) Option

If you prefer GPIO UART, wire:
- Pi TX -> Cardputer RX
- Pi RX -> Cardputer TX
- GND -> GND

Set `UART_RX_PIN` and `UART_TX_PIN` in `termputer/termputer.ino`, then set
`PORT:UART` or `PORT:BOTH` on the Cardputer.

## Files

- `termputer/termputer.ino` - main sketch
- `Makefile` - Arduino CLI build/upload
- `IMG_6746.jpeg` - project photo
