# Makefile for Termputer - Cardputer Advanced Terminal
# Usage: make | make upload | make clean

SKETCH = termputer
FIRMWARE_DIR = firmware
BUILD_DIR = build
TIMESTAMP := $(shell date +"%Y%m%d_%H%M%S")

# Board configuration
FQBN = esp32:esp32:m5stack_cardputer:PartitionScheme=huge_app

# Default serial port
PORT ?= /dev/cu.usbmodem*
BAUD ?= 115200

# Output file names
OUT_APP_ONLY = $(FIRMWARE_DIR)/firmware_app_only.bin
OUT_BOOT_APP = $(FIRMWARE_DIR)/firmware_boot_app.bin

.PHONY: all upload monitor clean help

all:
	@echo "Building Termputer firmware..."
	@mkdir -p $(FIRMWARE_DIR) $(BUILD_DIR)
	arduino-cli compile \
		--fqbn $(FQBN) \
		--output-dir $(BUILD_DIR) \
		$(SKETCH)
	@cp $(BUILD_DIR)/$(SKETCH).ino.bin $(OUT_APP_ONLY)
	@cp $(BUILD_DIR)/$(SKETCH).ino.merged.bin $(OUT_BOOT_APP)
	@echo ""
	@echo "=========================================="
	@echo "Build Complete!"
	@echo "=========================================="
	@echo "App-only binary (482KB):"
	@ls -lh $(OUT_APP_ONLY)
	@echo ""
	@echo "Full merged binary (4MB):"
	@ls -lh $(OUT_BOOT_APP)

upload: all
	@echo "Uploading to Cardputer on $(PORT)..."
	arduino-cli upload \
		--fqbn $(FQBN) \
		--port $(PORT) \
		--input-dir $(BUILD_DIR)
	@echo "✓ Upload complete!"

monitor:
	@if [ -z "$(PORT)" ]; then \
		echo "PORT is required for monitor, e.g. make monitor PORT=/dev/tty.usbmodemXXXX"; \
		exit 1; \
	fi
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "✓ Build directory cleaned"
	@echo ""
	@echo "To also remove firmware binaries:"
	@echo "  rm -rf $(FIRMWARE_DIR)/*.bin"

help:
	@echo "Termputer - Makefile Commands:"
	@echo "  make           - Build firmware"
	@echo "  make upload    - Build and upload to device"
	@echo "  make monitor   - Open serial monitor"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make help      - Show this help"
	@echo ""
	@echo "Options:"
	@echo "  PORT=/dev/ttyUSB0 make upload  - Upload to custom port"
