#include <M5Cardputer.h>
#include <M5GFX.h>

#if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 0)
#include "USB.h"
#if !ARDUINO_USB_CDC_ON_BOOT
USBCDC USBSerial;
#endif
#endif

// Adjust these pins for your Cardputer Adv wiring (Grove or TX/RX header).
static constexpr int UART_RX_PIN = 1;
static constexpr int UART_TX_PIN = 2;

static constexpr uint32_t kBaudRates[] = {9600, 19200, 38400, 57600, 115200,
                                          230400};
static constexpr size_t kBaudRateCount =
    sizeof(kBaudRates) / sizeof(kBaudRates[0]);

static constexpr uint16_t kStatusBarHeight = 16;
static constexpr uint16_t kFrameMargin     = 4;
static constexpr uint16_t kStatusDefaultBg = TFT_DARKGREY;
static constexpr uint16_t kStatusChangedFg = TFT_YELLOW;
static constexpr uint32_t kUiRefreshMs     = 50;

enum PortMode {
    PORT_USB,
    PORT_UART,
    PORT_BOTH,
};

M5Canvas terminal(&M5Cardputer.Display);
Stream* usb_stream = &Serial;

PortMode active_port  = PORT_BOTH;
PortMode last_rx_port = PORT_UART;
size_t baud_index     = 4;  // default 115200

bool local_echo    = true;
bool filter_ansi   = true;
bool send_crlf     = true;
bool ansi_sequence = false;
bool line_mode     = true;
uint8_t font_scale = 1;

String input_buffer;
bool usb_connected = false;
String last_status_line1;
String last_status_line2;

uint32_t last_ui_push_ms = 0;

#if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 0)
static volatile bool usb_event_connected = false;
static volatile bool usb_event_dirty     = false;

static void usbEventCallback(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    if (event_base != ARDUINO_USB_CDC_EVENTS) {
        return;
    }
    if (event_id == ARDUINO_USB_CDC_CONNECTED_EVENT) {
        usb_event_connected = true;
        usb_event_dirty     = true;
    } else if (event_id == ARDUINO_USB_CDC_DISCONNECTED_EVENT) {
        usb_event_connected = false;
        usb_event_dirty     = true;
    }
}
#endif

static const char* portModeLabel(PortMode mode)
{
    switch (mode) {
        case PORT_USB:
            return "USB";
        case PORT_UART:
            return "UART";
        case PORT_BOTH:
            return "BOTH";
    }
    return "?";
}

static void setupSerial()
{
    #if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 0)
        #if !ARDUINO_USB_CDC_ON_BOOT
    USBSerial.begin();
    USB.begin();
    usb_stream = &USBSerial;
        #else
    Serial.begin(kBaudRates[baud_index]);
    usb_stream = &Serial;
        #endif
    USB.onEvent(usbEventCallback);
        #if defined(USB_SERIAL_IS_DEFINED) || !ARDUINO_USB_CDC_ON_BOOT
    USBSerial.onEvent(usbEventCallback);
        #endif
    #else
    Serial.begin(kBaudRates[baud_index]);
    usb_stream = &Serial;
    #endif
    Serial1.begin(kBaudRates[baud_index], SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
}

static void drawStatusBar()
{
    const uint16_t w = M5Cardputer.Display.width();
    const uint16_t h = M5Cardputer.Display.height();
    const uint16_t y = h - kStatusBarHeight;

    String line1 = String("PORT:") + portModeLabel(active_port) +
                   " BAUD:" + String(kBaudRates[baud_index]) +
                   " RX:" + portModeLabel(last_rx_port) +
                   " USB:" + (usb_connected ? "ON" : "OFF");
    String line2 = String("ECHO:") + (local_echo ? "ON" : "OFF") +
                   " ANSI:" + (filter_ansi ? "ON" : "OFF") +
                   " CRLF:" + (send_crlf ? "ON" : "OFF") +
                   " FONT:" + String(font_scale);

    if (line1 == last_status_line1 && line2 == last_status_line2) {
        return;
    }

    last_status_line1 = line1;
    last_status_line2 = line2;

    M5Cardputer.Display.fillRect(0, y, w, kStatusBarHeight, kStatusDefaultBg);

    M5Cardputer.Display.setCursor(2, y + 2);
    M5Cardputer.Display.setTextColor(
        (active_port == PORT_UART) ? TFT_WHITE : kStatusChangedFg,
        kStatusDefaultBg);
    M5Cardputer.Display.print("PORT:");
    M5Cardputer.Display.print(portModeLabel(active_port));

    M5Cardputer.Display.setTextColor(
        (baud_index == 4) ? TFT_WHITE : kStatusChangedFg, kStatusDefaultBg);
    M5Cardputer.Display.print(" BAUD:");
    M5Cardputer.Display.print(kBaudRates[baud_index]);

    M5Cardputer.Display.setTextColor(TFT_WHITE, kStatusDefaultBg);
    M5Cardputer.Display.print(" RX:");
    M5Cardputer.Display.print(portModeLabel(last_rx_port));

    M5Cardputer.Display.setTextColor(
        usb_connected ? kStatusChangedFg : TFT_WHITE, kStatusDefaultBg);
    M5Cardputer.Display.print(" USB:");
    M5Cardputer.Display.print(usb_connected ? "ON" : "OFF");

    M5Cardputer.Display.setCursor(2, y + 9);
    M5Cardputer.Display.setTextColor(local_echo ? TFT_WHITE : kStatusChangedFg,
                                     kStatusDefaultBg);
    M5Cardputer.Display.print("ECHO:");
    M5Cardputer.Display.print(local_echo ? "ON" : "OFF");

    M5Cardputer.Display.setTextColor(
        filter_ansi ? TFT_WHITE : kStatusChangedFg, kStatusDefaultBg);
    M5Cardputer.Display.print(" ANSI:");
    M5Cardputer.Display.print(filter_ansi ? "ON" : "OFF");

    M5Cardputer.Display.setTextColor(send_crlf ? TFT_WHITE : kStatusChangedFg,
                                     kStatusDefaultBg);
    M5Cardputer.Display.print(" CRLF:");
    M5Cardputer.Display.print(send_crlf ? "ON" : "OFF");

    M5Cardputer.Display.setTextColor(
        (font_scale == 1) ? TFT_WHITE : kStatusChangedFg, kStatusDefaultBg);
    M5Cardputer.Display.print(" FONT:");
    M5Cardputer.Display.print(font_scale);
}

static void terminalPush()
{
    terminal.pushSprite(kFrameMargin, kFrameMargin);
    drawStatusBar();
}

static void terminalWriteChar(char c)
{
    if (filter_ansi) {
        if (ansi_sequence) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                ansi_sequence = false;
            }
            return;
        }
        if (c == 0x1b) {
            ansi_sequence = true;
            return;
        }
    }

    if (c == '\r') {
        terminal.setCursor(0, terminal.getCursorY());
        return;
    }
    if (c == '\n') {
        terminal.println();
        return;
    }
    if (c == '\b') {
        int cursor_x = terminal.getCursorX();
        int cursor_y = terminal.getCursorY();
        int char_w   = terminal.textWidth("W");
        if (cursor_x >= char_w) {
            terminal.setCursor(cursor_x - char_w, cursor_y);
            terminal.print(' ');
            terminal.setCursor(cursor_x - char_w, cursor_y);
        }
        return;
    }

    terminal.print(c);
}

static void terminalWrite(const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        terminalWriteChar(static_cast<char>(data[i]));
    }
}

static void sendByte(uint8_t value)
{
    if (active_port == PORT_USB || active_port == PORT_BOTH) {
        usb_stream->write(value);
    }
    if (active_port == PORT_UART || active_port == PORT_BOTH) {
        Serial1.write(value);
    }
}

static void sendText(const char* text)
{
    while (*text) {
        sendByte(static_cast<uint8_t>(*text));
        ++text;
    }
}

static void handleFnShortcut(char key)
{
    switch (key) {
        case '1':
            active_port = static_cast<PortMode>((active_port + 1) % 3);
            drawStatusBar();
            return;
        case '2':
            baud_index = (baud_index + 1) % kBaudRateCount;
            setupSerial();
            drawStatusBar();
            return;
        case '3':
            local_echo = !local_echo;
            drawStatusBar();
            return;
        case '4':
            filter_ansi = !filter_ansi;
            ansi_sequence = false;
            drawStatusBar();
            return;
        case '7':
            font_scale = (font_scale == 1) ? 2 : 1;
            terminal.setTextSize(font_scale);
            terminal.fillScreen(TFT_BLACK);
            terminal.setCursor(0, 0);
            terminal.println("Font size changed.");
            terminalPush();
            drawStatusBar();
            return;
        case '5':
            terminal.fillScreen(TFT_BLACK);
            terminal.setCursor(0, 0);
            terminalPush();
            return;
        case '6':
            send_crlf = !send_crlf;
            drawStatusBar();
            return;
    }
}

static void handleKeyboard()
{
    if (!M5Cardputer.Keyboard.isChange() ||
        !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    auto& status = M5Cardputer.Keyboard.keysState();

    if (status.fn) {
        for (auto c : status.word) {
            handleFnShortcut(c);
        }
        if (status.del) {
            handleFnShortcut('5');
        }
        return;
    }

    if (status.ctrl) {
        for (auto c : status.word) {
            char upper = (c >= 'a' && c <= 'z') ? c - 32 : c;
            if (upper >= 'A' && upper <= 'Z') {
                sendByte(static_cast<uint8_t>(upper - 'A' + 1));
            } else if (c == '[') {
                sendByte(0x1b);
            } else {
                sendByte(static_cast<uint8_t>(c));
            }
        }
        if (status.del) {
            sendByte(0x08);
        }
        if (status.enter) {
            send_crlf ? sendText("\r\n") : sendText("\r");
        }
        return;
    }

    if (status.del) {
        if (line_mode) {
            if (input_buffer.length() > 0) {
                input_buffer.remove(input_buffer.length() - 1);
                if (local_echo) {
                    terminalWriteChar('\b');
                }
            }
        } else {
            sendByte(0x08);
            if (local_echo) {
                terminalWriteChar('\b');
            }
        }
    }

    for (auto c : status.word) {
        if (line_mode) {
            input_buffer += c;
        } else {
            sendByte(static_cast<uint8_t>(c));
        }
        if (local_echo) {
            terminalWriteChar(c);
        }
    }

    if (status.enter) {
        if (line_mode) {
            if (input_buffer.length() > 0) {
                sendText(input_buffer.c_str());
                input_buffer.clear();
            }
            send_crlf ? sendText("\r\n") : sendText("\r");
        } else {
            send_crlf ? sendText("\r\n") : sendText("\r");
        }
        if (local_echo) {
            terminalWriteChar('\n');
        }
    }
}

static void updateUsbConnection()
{
    bool now_connected = false;
    #if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 0)
    if (usb_event_dirty) {
        usb_event_dirty = false;
        now_connected  = usb_event_connected;
    } else {
        #if !ARDUINO_USB_CDC_ON_BOOT
        now_connected = static_cast<bool>(USBSerial);
        #else
        now_connected = static_cast<bool>(Serial);
        #endif
    }
    #else
    now_connected = static_cast<bool>(Serial);
    #endif
    if (now_connected != usb_connected) {
        usb_connected = now_connected;
        terminal.setTextColor(TFT_YELLOW, TFT_BLACK);
        terminal.printf("[USB %s]\n", usb_connected ? "connected" : "disconnected");
        terminal.setTextColor(TFT_GREEN, TFT_BLACK);
        terminalPush();
        drawStatusBar();
    }
}

static void readSerial()
{
    bool dirty = false;

    while (usb_stream->available() > 0) {
        uint8_t c = static_cast<uint8_t>(usb_stream->read());
        last_rx_port = PORT_USB;
        terminalWrite(&c, 1);
        dirty = true;
    }

    while (Serial1.available() > 0) {
        uint8_t c = static_cast<uint8_t>(Serial1.read());
        last_rx_port = PORT_UART;
        terminalWrite(&c, 1);
        dirty = true;
    }

    if (dirty || (millis() - last_ui_push_ms) > kUiRefreshMs) {
        last_ui_push_ms = millis();
        terminalPush();
    }
}

void setup()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);

    terminal.setColorDepth(8);
    terminal.createSprite(M5Cardputer.Display.width() - (kFrameMargin * 2),
                          M5Cardputer.Display.height() - kStatusBarHeight -
                              (kFrameMargin * 2));
    terminal.fillScreen(TFT_BLACK);
    terminal.setTextColor(TFT_GREEN, TFT_BLACK);
    terminal.setTextScroll(true);
    terminal.setTextWrap(true, true);
    terminal.setTextSize(font_scale);
    terminal.setCursor(0, 0);

    M5Cardputer.Display.drawRect(
        0, 0, M5Cardputer.Display.width(),
        M5Cardputer.Display.height() - kStatusBarHeight, TFT_DARKGREY);

    setupSerial();

    terminal.println("Cardputer Adv Terminal");
    terminal.println("FN+1 Port  FN+2 Baud  FN+3 Echo");
    terminal.println("FN+4 ANSI  FN+5 Clear FN+6 CRLF");
    terminal.println("FN+7 Font size");
    terminal.println("Use RX/TX pins for UART.");
    terminalPush();
}

void loop()
{
    M5Cardputer.update();
    updateUsbConnection();
    handleKeyboard();
    readSerial();
}
