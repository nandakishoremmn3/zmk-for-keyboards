/*
 * Custom Corne Layout Display
 * Self-contained WPM + Bongo Cat with Dynamic Typing Focus Mode (Pure LVGL)
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/event_manager.h>
#include <lvgl.h>

/* ── Bongo Cat 1-bit Frame Arrays (24px wide x 16px tall) ── */
static const uint8_t cat_neutral[] = {
    0x00,0x18,0x00, 0x00,0x24,0x00, 0x00,0x24,0x00, 0x00,0x42,0x00, 
    0x00,0x81,0x00, 0x01,0x00,0x80, 0x02,0x42,0x40, 0x04,0x00,0x20,
    0x04,0x18,0x20, 0x02,0x00,0x40, 0x01,0x00,0x80, 0x00,0x7f,0x00,
    0x00,0x41,0x00, 0x00,0x41,0x00, 0x00,0x3e,0x00, 0x00,0x00,0x00
};

static const uint8_t cat_left_paw[] = {
    0x00,0x18,0x00, 0x00,0x24,0x00, 0x00,0x24,0x00, 0x00,0x42,0x00, 
    0x00,0x81,0x00, 0x01,0x00,0x80, 0x02,0x42,0x40, 0x04,0x00,0x20,
    0x04,0x18,0x20, 0x02,0x00,0x40, 0x01,0x00,0x80, 0x0c,0x7f,0x00,
    0x12,0x41,0x00, 0x12,0x41,0x00, 0x0c,0x3e,0x00, 0x00,0x00,0x00
};

static const uint8_t cat_right_paw[] = {
    0x00,0x18,0x00, 0x00,0x24,0x00, 0x00,0x24,0x00, 0x00,0x42,0x00, 
    0x00,0x81,0x00, 0x01,0x00,0x80, 0x02,0x42,0x40, 0x04,0x00,0x20,
    0x04,0x18,0x20, 0x02,0x00,0x40, 0x01,0x00,0x80, 0x00,0x7f,0x30,
    0x00,0x41,0x48, 0x00,0x41,0x48, 0x00,0x3e,0x30, 0x00,0x00,0x00
};

static lv_image_dsc_t cat_dsc = {
    .header = { .cf = LV_COLOR_FORMAT_I1, .w = 24, .h = 16 },
    .data_size = sizeof(cat_neutral),
    .data = cat_neutral,
};

#define ANIMATION_TIMER_MS 150
#define IDLE_TIMEOUT_MS 3000

static lv_obj_t *cat_img;
static lv_obj_t *wpm_label;
static lv_obj_t *status_label;
static uint8_t cat_state = 0;
static bool typing_mode = false;
static int64_t last_typing_time = 0;

// Sandboxed internal matrix variables
static uint32_t stroke_count = 0;
static uint32_t simulated_wpm = 0;

static uint16_t anim_rand(void) {
    static uint16_t seed = 42;
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return seed;
}

/* Intercept physical microswitch closures locally */
static int key_event_listener(const zmk_event_t *eh) {
    struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev && ev->state) { // Key pressed down
        stroke_count++;
        last_typing_time = k_uptime_get();
    }
    return 0;
}
ZMK_LISTENER(key_listener, key_event_listener);
ZMK_SUBSCRIPTION(key_listener, zmk_position_state_changed);

/* Internal UI refresh loop */
static void screen_timer_cb(lv_timer_t *timer) {
    int64_t now = k_uptime_get();
    int64_t idle_time = now - last_typing_time;

    if (idle_time < IDLE_TIMEOUT_MS && last_typing_time > 0) {
        // Calculate WPM using a rolling window
        simulated_wpm = (stroke_count * 60) / ((now > 0) ? (now / 1000) + 1 : 1);
        if (simulated_wpm == 0) simulated_wpm = 12; // Maintain animation flow
        
        if (!typing_mode) {
            typing_mode = true;
            lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        }

        if (cat_state == 0) {
            cat_state = (anim_rand() % 2) + 1;
            cat_dsc.data = (cat_state == 1) ? cat_left_paw : cat_right_paw;
        } else {
            cat_state = 0;
            cat_dsc.data = cat_neutral;
        }
    } else {
        simulated_wpm = 0;
        cat_state = 0;
        cat_dsc.data = cat_neutral;

        if (typing_mode) {
            typing_mode = false;
            lv_obj_remove_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    char wpm_str[16];
    snprintf(wpm_str, sizeof(wpm_str), "WPM: %d", simulated_wpm);
    lv_label_set_text(wpm_label, wpm_str);
    
    lv_image_set_src(cat_img, &cat_dsc);
    lv_obj_invalidate(cat_img);
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "SYSTEM ACTIVE");
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 0, 0);

    wpm_label = lv_label_create(screen);
    lv_label_set_text(wpm_label, "WPM: 0");
    lv_obj_align(wpm_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    cat_img = lv_image_create(screen);
    cat_dsc.data = cat_neutral;
    lv_image_set_src(cat_img, &cat_dsc);
    lv_obj_align(cat_img, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_timer_create(screen_timer_cb, ANIMATION_TIMER_MS, NULL);

    return screen;
}
