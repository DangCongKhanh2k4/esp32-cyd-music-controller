#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// CYD Dedicated Touch SPI Pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Apple Media Service (AMS) UUIDs
static NimBLEUUID amsServiceUUID("77000001-A6E7-4B26-8E2E-2E05EC70A756");
static NimBLEUUID amsEntityUpdateUUID("2B880001-D9F0-449B-B98A-E44A3584E23D");
static NimBLEUUID amsEntityAttributeUUID("2B880002-D9F0-449B-B98A-E44A3584E23D");

SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// NimBLE HID Remote
NimBLEHIDDevice* hid;
NimBLECharacteristic* inputMedia;
bool connected = false;
bool isEncrypted = false;
bool discoveryStarted = false;
bool isPlaying = false; // Track phone media playback state live!
bool needRedraw = false;
uint16_t activeConnHandle = BLE_HS_CONN_HANDLE_NONE;
uint16_t amsEntityUpdateValHandle = 0;

// HID Report Descriptor
const uint8_t MediaReportDescriptor[] = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, 0x01, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x07,
    0x09, 0xB5, 0x09, 0xB6, 0x09, 0xB7,
    0x09, 0xCD, 0x09, 0xE9, 0x09, 0xEA, 0x09, 0xE2,
    0x81, 0x02, 0x75, 0x01, 0x95, 0x01, 0x81, 0x01,
    0xC0
};

#define KEY_NEXT      0x01
#define KEY_PREV      0x02
#define KEY_PLAYPAUSE 0x08
#define KEY_VOL_UP    0x10
#define KEY_VOL_DOWN  0x20

void sendMediaKey(uint8_t keyMask) {
    if (!connected) return;
    uint8_t report[1] = { keyMask };
    inputMedia->setValue(report, sizeof(report));
    inputMedia->notify();
    delay(20);
    report[0] = 0x00;
    inputMedia->setValue(report, sizeof(report));
    inputMedia->notify();
}

// Minimalist All-Black Background & Neon Blue Outlines Theme (565 RGB for Inverted Display)
#define C_BG          TFT_BLACK       // Pure Pitch Black Background
#define C_CARD        TFT_BLACK       // Pure Pitch Black inside buttons
#define C_BLUE_BORDER 0x03FF          // Bright Neon Blue Border
#define C_NEON_BLUE   0x14FF          // Electric Neon Blue Text/Icons
#define C_CYAN        TFT_CYAN        // Pure Cyan Icons
#define C_WHITE       TFT_WHITE       // Pure Crisp White
#define C_GRAY        0x7BEF          // Muted Gray
#define C_DARK_HEADER TFT_BLACK       // Pure Pitch Black Header

// Button zones (Landscape 320x240, Rotation 1)
struct Zone { int x, y, w, h; };
Zone zVolDown = { 10,  35, 145, 70 };
Zone zVolUp   = { 165, 35, 145, 70 };
Zone zPrev    = { 10,  125, 90, 100 };
Zone zPlay    = { 115, 125, 90, 100 };
Zone zNext    = { 215, 125, 90, 100 };

bool inZone(int tx, int ty, Zone &z) {
    return (tx >= z.x && tx <= z.x + z.w && ty >= z.y && ty <= z.y + z.h);
}

void drawVolDown(Zone &z, uint16_t fg) {
    int cx = z.x + z.w/2, cy = z.y + z.h/2;
    tft.fillRect(cx-18, cy-8, 12, 16, fg);
    tft.fillTriangle(cx-6, cy-8, cx+6, cy-18, cx+6, cy+18, fg);
    tft.fillTriangle(cx-6, cy+8, cx+6, cy-18, cx+6, cy+18, fg);
    tft.fillRect(cx+14, cy-2, 14, 4, fg);
}

void drawVolUp(Zone &z, uint16_t fg) {
    int cx = z.x + z.w/2, cy = z.y + z.h/2;
    tft.fillRect(cx-22, cy-8, 12, 16, fg);
    tft.fillTriangle(cx-10, cy-8, cx+2, cy-18, cx+2, cy+18, fg);
    tft.fillTriangle(cx-10, cy+8, cx+2, cy-18, cx+2, cy+18, fg);
    tft.fillRect(cx+10, cy-2, 16, 4, fg);
    tft.fillRect(cx+16, cy-8, 4, 16, fg);
}

void drawPrevIcon(Zone &z, uint16_t fg) {
    int cx = z.x + z.w/2, cy = z.y + z.h/2;
    tft.fillRect(cx-20, cy-16, 4, 32, fg);
    tft.fillTriangle(cx-12, cy, cx+2, cy-16, cx+2, cy+16, fg);
    tft.fillTriangle(cx+2, cy, cx+16, cy-16, cx+16, cy+16, fg);
}

void drawPlayIcon(Zone &z, uint16_t fg) {
    int cx = z.x + z.w/2, cy = z.y + z.h/2;
    tft.fillTriangle(cx-14, cy-20, cx-14, cy+20, cx+18, cy, fg);
}

void drawPauseIcon(Zone &z, uint16_t fg) {
    int cx = z.x + z.w/2, cy = z.y + z.h/2;
    tft.fillRect(cx-14, cy-20, 10, 40, fg);
    tft.fillRect(cx+4,  cy-20, 10, 40, fg);
}

void drawNextIcon(Zone &z, uint16_t fg) {
    int cx = z.x + z.w/2, cy = z.y + z.h/2;
    tft.fillTriangle(cx-16, cy-16, cx-16, cy+16, cx-2, cy, fg);
    tft.fillTriangle(cx-2, cy-16, cx-2, cy+16, cx+12, cy, fg);
    tft.fillRect(cx+16, cy-16, 4, 32, fg);
}

typedef void (*IconFn)(Zone&, uint16_t);

void drawButton(Zone &z, uint16_t bg, IconFn icon, uint16_t fg, uint16_t borderColor = C_BLUE_BORDER) {
    tft.fillRoundRect(z.x, z.y, z.w, z.h, 14, bg);
    tft.drawRoundRect(z.x, z.y, z.w, z.h, 14, borderColor);
    tft.drawRoundRect(z.x+1, z.y+1, z.w-2, z.h-2, 13, borderColor); // Crisp double border outline
    icon(z, fg);
}

void drawUI() {
    tft.fillScreen(C_BG);
    
    // Header Bar (All-Black with Neon Blue Separator)
    tft.fillRect(0, 0, 320, 26, C_DARK_HEADER);
    tft.drawFastHLine(0, 26, 320, C_BLUE_BORDER);
    
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_CYAN, C_DARK_HEADER);
    tft.drawString("BT TOUCH REMOTE", 10, 13, 2);
    
    tft.setTextDatum(MR_DATUM);
    if (connected) {
        tft.setTextColor(C_CYAN, C_DARK_HEADER);
        tft.drawString("CONNECTED", 310, 13, 2);
    } else {
        tft.setTextColor(C_GRAY, C_DARK_HEADER);
        tft.drawString("PAIRING...", 310, 13, 2);
    }

    // Volume buttons (Black interior, Neon Blue borders & icons)
    drawButton(zVolDown, C_CARD, drawVolDown, C_CYAN);
    drawButton(zVolUp, C_CARD, drawVolUp, C_CYAN);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_NEON_BLUE, C_BG);
    tft.drawString("VOL -", zVolDown.x + zVolDown.w/2, zVolDown.y + zVolDown.h + 10, 2);
    tft.drawString("VOL +", zVolUp.x + zVolUp.w/2, zVolUp.y + zVolUp.h + 10, 2);
    
    // Media buttons (Black interior, Neon Blue borders & icons)
    drawButton(zPrev, C_CARD, drawPrevIcon, C_CYAN);
    if (isPlaying) {
        drawButton(zPlay, C_CARD, drawPauseIcon, C_CYAN, C_BLUE_BORDER);
    } else {
        drawButton(zPlay, C_CARD, drawPlayIcon, C_CYAN, C_BLUE_BORDER);
    }
    drawButton(zNext, C_CARD, drawNextIcon, C_CYAN);
}

void flashBtn(Zone &z, IconFn icon, uint16_t restoreBg, uint16_t restoreFg = C_CYAN, uint16_t restoreBorder = C_BLUE_BORDER) {
    tft.fillRoundRect(z.x, z.y, z.w, z.h, 14, C_CYAN);
    icon(z, C_BG);
    delay(80);
    drawButton(z, restoreBg, icon, restoreFg, restoreBorder);
}

// Handler for incoming GATT Notify events (AMS Media Status)
int customGapHandler(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_NOTIFY_RX) {
        uint16_t attrHandle = event->notify_rx.attr_handle;
        uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
        
        if (len >= 3) {
            uint8_t buf[64];
            os_mbuf_copydata(event->notify_rx.om, 0, len, buf);
            
            // EntityID 0 = Player Entity, AttributeID 1 = PlaybackInfo
            if (buf[0] == 0 && buf[1] == 1 && len >= 4) {
                char stateChar = (char)buf[3];
                bool newPlaying = (stateChar == '1' || buf[3] == 1);
                if (newPlaying != isPlaying) {
                    isPlaying = newPlaying;
                    needRedraw = true;
                    Serial.printf("[AMS] Live Playback State Updated: %s\n", isPlaying ? "PLAYING" : "PAUSED");
                }
            }
        }
    }
    return 0;
}

// Characteristic Discovery Callback for AMS
static int gatt_disc_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0 && chr != NULL) {
        NimBLEUUID uuid((const ble_uuid128_t*)&chr->uuid);
        if (uuid == amsEntityUpdateUUID) {
            amsEntityUpdateValHandle = chr->val_handle;
            Serial.printf("[AMS] Found EntityUpdate Char (0x%04X)\n", amsEntityUpdateValHandle);
            
            // Enable CCCD notification (write 0x0001)
            uint8_t cccdVal[] = { 0x01, 0x00 };
            ble_gattc_write_flat(conn_handle, amsEntityUpdateValHandle + 1, cccdVal, sizeof(cccdVal), NULL, NULL);
            
            // Subscribe to Player Entity (0): PlaybackInfo Attribute (1)
            uint8_t subCmdPlayer[] = { 0, 1 };
            ble_gattc_write_flat(conn_handle, amsEntityUpdateValHandle, subCmdPlayer, sizeof(subCmdPlayer), NULL, NULL);
            Serial.println("[AMS] Subscribed to Live Player Playback State!");
        }
    }
    return 0;
}

// Service Discovery Callback
static int gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
    if (error->status == 0 && service != NULL) {
        NimBLEUUID uuid((const ble_uuid128_t*)&service->uuid);
        if (uuid == amsServiceUUID) {
            Serial.println("[AMS] Discovered AMS Service! Discovering characteristics...");
            ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle, gatt_disc_chr_cb, NULL);
        }
    }
    return 0;
}

void discoverServices(uint16_t conn_handle) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || discoveryStarted) return;
    discoveryStarted = true;
    Serial.printf("[AMS] Discovering AMS Service on conn_handle %d ...\n", conn_handle);
    ble_gattc_disc_all_svcs(conn_handle, gatt_disc_svc_cb, NULL);
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        connected = true;
        isEncrypted = false;
        discoveryStarted = false;
        activeConnHandle = desc->conn_handle;
        needRedraw = true;
        Serial.println("[BLE] Connected!");
        NimBLEDevice::startSecurity(desc->conn_handle);
    }
    void onDisconnect(NimBLEServer* pServer) override {
        connected = false;
        isEncrypted = false;
        discoveryStarted = false;
        activeConnHandle = BLE_HS_CONN_HANDLE_NONE;
        isPlaying = false;
        needRedraw = true;
        Serial.println("[BLE] Disconnected.");
        NimBLEDevice::startAdvertising();
    }
};

class SecurityCallbacks : public NimBLESecurityCallbacks {
    uint32_t onPassKeyRequest() override { return 0; }
    void onPassKeyNotify(uint32_t passkey) override {}
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        if (desc->sec_state.encrypted) {
            isEncrypted = true;
            Serial.println("[BLE] Encryption complete!");
        }
    }
    bool onConfirmPIN(uint32_t pin) override { return true; }
};

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n==================================================");
    Serial.println(">>> ESP32 CYD Remote - Minimalist All-Black Theme <<<");
    Serial.println(">>> Device: 'BT Touch Remote'                  <<<");
    Serial.println("==================================================");

    // Backlight
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    // Display init
    tft.init();
    tft.setRotation(1);       // Rotation 1
    tft.invertDisplay(true);  // Fix CYD inverted colors

    // Touch SPI init on dedicated CYD touch pins (CLK=25, MISO=39, MOSI=32, CS=33)
    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    drawUI();

    // BLE init
    NimBLEDevice::init("BT Touch Remote");
    NimBLEDevice::setCustomGapHandler(customGapHandler);
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityCallbacks(new SecurityCallbacks());

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    hid = new NimBLEHIDDevice(pServer);
    inputMedia = hid->inputReport(1);
    hid->manufacturer("Apple Inc.");
    hid->pnp(0x02, 0x05ac, 0x022c, 0x0100);
    hid->hidInfo(0x00, 0x01);
    hid->reportMap((uint8_t*)MediaReportDescriptor, sizeof(MediaReportDescriptor));
    hid->startServices();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->setAppearance(0x0942);
    pAdv->addServiceUUID(hid->hidService()->getUUID());
    pAdv->addServiceUUID(amsServiceUUID); // AMS Service
    pAdv->setScanResponse(true);
    pAdv->start();
    Serial.println("[BLE] Advertising...");
}

void loop() {
    static bool pendingDiscovery = false;
    static unsigned long encryptedTime = 0;

    if (isEncrypted && !pendingDiscovery && !discoveryStarted) {
        pendingDiscovery = true;
        encryptedTime = millis();
    }

    if (pendingDiscovery && (millis() - encryptedTime >= 1000)) {
        pendingDiscovery = false;
        discoverServices(activeConnHandle);
    }

    if (needRedraw) {
        needRedraw = false;
        drawUI();
    }

    // Touch handling
    static unsigned long lastTouch = 0;
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        
        if (p.z > 200 && p.x > 0 && p.y > 0) {
            int sx = map(p.x, 200, 3800, 0, 320);
            int sy = map(p.y, 200, 3800, 0, 240);
            sx = constrain(sx, 0, 319);
            sy = constrain(sy, 0, 239);

            if (millis() - lastTouch > 250) {
                lastTouch = millis();
                Serial.printf("[TOUCH] screen(%d,%d)\n", sx, sy);

                if (inZone(sx, sy, zVolDown)) {
                    Serial.println("[ACTION] VOL DOWN");
                    flashBtn(zVolDown, drawVolDown, C_CARD);
                    sendMediaKey(KEY_VOL_DOWN);
                } else if (inZone(sx, sy, zVolUp)) {
                    Serial.println("[ACTION] VOL UP");
                    flashBtn(zVolUp, drawVolUp, C_CARD);
                    sendMediaKey(KEY_VOL_UP);
                } else if (inZone(sx, sy, zPrev)) {
                    Serial.println("[ACTION] PREV");
                    flashBtn(zPrev, drawPrevIcon, C_CARD);
                    sendMediaKey(KEY_PREV);
                } else if (inZone(sx, sy, zPlay)) {
                    Serial.println("[ACTION] PLAY/PAUSE TOGGLE");
                    isPlaying = !isPlaying;
                    drawUI();
                    sendMediaKey(KEY_PLAYPAUSE);
                } else if (inZone(sx, sy, zNext)) {
                    Serial.println("[ACTION] NEXT");
                    flashBtn(zNext, drawNextIcon, C_CARD);
                    sendMediaKey(KEY_NEXT);
                }
            }
        }
    }
    delay(10);
}
