ARDUINO_CLI ?= arduino-cli
FQBN ?= esp32:esp32:m5stack_cardputer
SKETCH ?= termputer
PORT ?= /dev/cu.usbmodem*
BAUD ?= 115200

.PHONY: all build upload monitor

all: build

build:
	$(ARDUINO_CLI) compile --fqbn $(FQBN) --export-binaries $(SKETCH)
	@cp $(SKETCH)/build/esp32.esp32.m5stack_cardputer/$(SKETCH).ino.merged.bin firmware.bin

upload: build
	$(ARDUINO_CLI) upload --fqbn $(FQBN) $(if $(PORT),--port $(PORT),) $(SKETCH)

monitor:
	@if [ -z "$(PORT)" ]; then \
		echo "PORT is required for monitor, e.g. make monitor PORT=/dev/tty.usbmodemXXXX"; \
		exit 1; \
	fi
	$(ARDUINO_CLI) monitor --port $(PORT) --config baudrate=$(BAUD)
