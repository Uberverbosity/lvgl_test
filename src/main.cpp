#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// ---------- Globals ----------
TFT_eSPI tft = TFT_eSPI();

// LVGL display buffer
static lv_color_t buf1[240 * 20];
static lv_disp_draw_buf_t draw_buf;

// ---------- LVGL Flush ----------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// ---------- Setup ----------
void setup()
{
    // Serial Fix (USB CDC)
    delay(500);
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nUSB CDC serial active.");

    // TFT
    tft.init();
    tft.setRotation(0);

    // LVGL Init
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, 240 * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // LVGL Test Label
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "LVGL OK");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    Serial.println("LVGL initialized.");
}

// ---------- Loop ----------
void loop()
{
    lv_timer_handler();  // LVGL task handler
    delay(5);
}
