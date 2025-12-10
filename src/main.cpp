#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// ---------------- Display globals ----------------
TFT_eSPI tft = TFT_eSPI();

static uint8_t *lv_framebuf = nullptr;
static lv_display_t *disp;

// ---------------- Encoder button globals ----------------
static volatile bool enc_pressed = false;

// ---------------- Encoder knob globals -----------------
static volatile int enc_delta = 0;
static volatile uint8_t prev_state = 0;

void IRAM_ATTR enc_isr() {
    uint8_t a = digitalRead(6);
    uint8_t b = digitalRead(7);

    uint8_t state = (a << 1) | b;

    static const int8_t table[4][4] = {
        {  0, -1, +1,  0 },
        { +1,  0,  0, -1 },
        { -1,  0,  0, +1 },
        {  0, +1, -1,  0 }
    };

    enc_delta += table[prev_state][state];
    prev_state = state;
}


void IRAM_ATTR enc_btn_isr() {
    enc_pressed = !digitalRead(9);  // Active low
}

// ---------------- Flush Callback (LVGL9) ----------------
void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    // Debug: Print first few flushes to confirm calls
    static int flush_count = 0;
    if (flush_count < 3) {
        Serial.printf("Flush #%d: Area (%d,%d) x %dx%d\n", ++flush_count, area->x1, area->y1, w, h);
    }

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)color_p, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

// ---------------- Setup ----------------
void setup()
{
    // Serial-CDC Fix
    delay(500);
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nUSB CDC serial active.");

    // TFT Init
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);  // NEW: Clear screen to black (confirms TFT works beyond flash)
    Serial.println("TFT init complete.");

    // Encoder knob setup
    pinMode(6, INPUT_PULLUP);
    pinMode(7, INPUT_PULLUP);

    prev_state = (digitalRead(6) << 1) | digitalRead(7);

    attachInterrupt(digitalPinToInterrupt(6), enc_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(7), enc_isr, CHANGE);

    // Encoder button setup
    pinMode(9, INPUT_PULLUP);
    attachInterrupt(9, enc_btn_isr, CHANGE);

    Serial.println("Encoder initialized.");

    // Turn on Backlight
    pinMode(8, OUTPUT);
    digitalWrite(8, LOW);

    // LVGL Begin
    lv_init();
    Serial.println("LVGL init complete.");

    // Allocate 16-bit frame buffer (full screen)
    lv_framebuf = (uint8_t *)malloc(240 * 240 * 2);
    if (lv_framebuf == nullptr) {
        Serial.println("ERROR: Buffer malloc failed! (Low RAM?)");
        while(1);  // Halt
    }
    memset(lv_framebuf, 0, 240 * 240 * 2);  // Black buffer
    Serial.printf("Buffer allocated: %p (size %d bytes)\n", lv_framebuf, 240 * 240 * 2);

    // Create display object (LVGL9 API)
    disp = lv_display_create(240, 240);
    if (disp == nullptr) {
        Serial.println("ERROR: Display create failed!");
        while(1);
    }

    // LVGL9: associate frame buffer
    lv_display_set_buffers(
        disp,
        lv_framebuf,
        NULL,  // Single buffer (direct mode)
        240 * 240 * 2,
        LV_DISPLAY_RENDER_MODE_DIRECT
    );
    Serial.println("Display buffers set.");

    // Register flush callback
    lv_display_set_flush_cb(disp, my_flush_cb);
    Serial.println("Flush CB registered.");

    // NEW: Set default background to white (makes text visible if black default)
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), 0);

    // Simple widget to verify functionality
    lv_obj_t *label = lv_label_create(lv_screen_active());
    if (label == nullptr) {
        Serial.println("ERROR: Label create failed!");
        while(1);
    }
    lv_label_set_text(label, "Hello LVGL9!");
    lv_obj_center(label);  // Auto-invalidates/positions
    Serial.println("Label created & centered.");

    // NEW: Force initial render/invalidation
    lv_obj_invalidate(lv_screen_active());  // Mark screen dirty
    lv_timer_handler();  // Run once in setup for initial flush
    Serial.println("Initial render triggered.");
}

void loop()
{
    // ---------------- LVGL TICK ----------------
    static uint32_t tick_last = 0;
    uint32_t now = millis();
    lv_tick_inc(now - tick_last);
    tick_last = now;

    lv_timer_handler();
    delay(5);   // Completely fine after fixing tick logic


    // ---------------- ENCODER DETENT LOGIC ----------------
    if (enc_delta >= 2) {
        Serial.println("Encoder detent +1");
        enc_delta = 0;
    } 
    else if (enc_delta <= -2) {
        Serial.println("Encoder detent -1");
        enc_delta = 0;
    }



    // ---------------- BUTTON LOGIC ----------------
    static bool last_btn = false;

    if (enc_pressed != last_btn) {
        Serial.printf("Button: %s\n",
                      enc_pressed ? "PRESSED" : "RELEASED");
        last_btn = enc_pressed;
    }
}

