#include <Arduino.h>

// TFT / LVGL order matters!
#include <TFT_eSPI.h>
#include <lvgl.h>

#define LV_LVGL_TFT_ESPI_IMPLEMENTATION

// Global display object
TFT_eSPI tft = TFT_eSPI();

// function prototypes so setup() can call them
void init_display();
void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);

// ---------------- Pin Mapping ----------------
#define PIN_BL      8   // Backlight LOW = ON
#define PIN_ENC_A   6
#define PIN_ENC_B   7
#define PIN_ENC_BTN 9


// ---------------- Encoder Globals ----------------
static volatile int enc_delta = 0;
static volatile uint8_t prev_state = 0;
static volatile bool enc_pressed = false;

// ---------------- Gray Code Table ----------------
static const int8_t transition_table[4][4] = {
    {  0, -1, +1,  0 },
    { +1,  0,  0, -1 },
    { -1,  0,  0, +1 },
    {  0, +1, -1,  0 }
};

int dial_value = 50;  // start mid-scale

void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)color_p, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

void init_display()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    pinMode(8, OUTPUT);
    digitalWrite(8, LOW);
}

// ---------------- Encoder ISR ----------------
void IRAM_ATTR enc_isr() {
    uint8_t a = digitalRead(PIN_ENC_A);
    uint8_t b = digitalRead(PIN_ENC_B);
    uint8_t state = (a << 1) | b;

    enc_delta += transition_table[prev_state][state];
    prev_state = state;
}

void IRAM_ATTR enc_btn_isr() {
    enc_pressed = !digitalRead(PIN_ENC_BTN);   // active low
}


// ---------------- LVGL Objects ----------------
lv_obj_t* dial_arc;
lv_obj_t* dial_label;

// ---------------- Dial Update Helper ----------------
static inline void dial_set_value(int v) {
    dial_value = v;
    lv_arc_set_value(dial_arc, v);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", v);
    lv_label_set_text(dial_label, buf);
}

void create_dial()
{
    dial_arc = lv_arc_create(lv_scr_act());
    lv_obj_set_size(dial_arc, 220, 220);
    lv_obj_center(dial_arc);

    // Remove knob and square the indicator
    lv_obj_remove_style(dial_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_rounded(dial_arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(dial_arc, false, LV_PART_INDICATOR);

    // Thick arc, near outer edge
    lv_obj_set_style_arc_width(dial_arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_width(dial_arc, 24, LV_PART_INDICATOR);

    // 270° sweep (from ~7 o'clock to 5 o'clock)
    lv_arc_set_bg_start_angle(dial_arc, 145);
    lv_arc_set_bg_end_angle(dial_arc, 35);
    lv_arc_set_start_angle(dial_arc, 145);
    lv_arc_set_end_angle(dial_arc,35);

    lv_obj_set_style_arc_color(dial_arc, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_arc_color(dial_arc, lv_color_hex(0x44CC44), LV_PART_INDICATOR);

    // Center number label
    dial_label = lv_label_create(lv_scr_act());
    lv_obj_center(dial_label);
    lv_obj_set_style_text_font(dial_label, &lv_font_montserrat_48, 0);
    lv_label_set_text_fmt(dial_label, "%d", dial_value);

    // Set initial conditions
    lv_arc_set_value(dial_arc, dial_value);
    lv_label_set_text_fmt(dial_label, "%d", dial_value);
}

// ---------------- Setup ----------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Booting clean test build...");

    // -------- GPIO Setup --------
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, LOW);   // Backlight ON

    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_BTN, INPUT_PULLUP);

    attachInterrupt(PIN_ENC_A,  enc_isr, CHANGE);
    attachInterrupt(PIN_ENC_B,  enc_isr, CHANGE);
    attachInterrupt(PIN_ENC_BTN, enc_btn_isr, CHANGE);

    // -------- LVGL Core Init --------
    lv_init();

    // -------- TFT Driver Init (SPI + GC9A01A) --------
    init_display();

    // -------- LVGL Display Object --------
    lv_display_t* disp = lv_display_create(240, 240);

    static uint16_t buf1[240 * 40];
    static uint16_t buf2[240 * 40];

    lv_display_set_buffers(
        disp,
        buf1, buf2,
        sizeof(buf1),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_display_set_flush_cb(disp, my_flush_cb);

    create_dial();

    Serial.println("Setup complete.");
}

// -------------------- Loop --------------------
void loop()
{
    // -------- LVGL Tick --------
    static uint32_t last = 0;
    uint32_t now = millis();
    lv_tick_inc(now - last);
    last = now;

    lv_timer_handler();   // let LVGL render


    // -------- Encoder Detents --------
    int delta = enc_delta;
    enc_delta = 0;

    if (delta != 0) {
        int new_val = dial_value + (delta > 0 ? +1 : -1);

        // Clamp 0–100
        if (new_val < 0)  new_val = 0;
        if (new_val > 100) new_val = 100;

        dial_set_value(new_val);

        Serial.printf("Dial: %d\n", new_val);
    }


    // -------- Encoder Button --------
    static bool last_btn = false;
    if (enc_pressed != last_btn) {
        Serial.printf("Button: %s\n", enc_pressed ? "PRESSED" : "RELEASED");
        last_btn = enc_pressed;
    }

    delay(5);   // keep CPU cool, LVGL tolerates this fine
}
