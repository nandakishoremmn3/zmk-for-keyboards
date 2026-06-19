/*
 * Custom Corne Central display
 * WPM + Bongo Cat with Dynamic Typing Focus Mode
 */

#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/peripheral_status.h>
#include <zmk/display/widgets/wpm_status.h>
#include <zmk/wpm.h>
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
#define IDLE_TIMEOUT_MS 3000 // Time in ms before showing battery/connection again

static lv_obj_t *cat_img;
static uint8_t cat_state = 0;
static bool typing_mode = false;
static int64_t last_typing_time = 0;

/* Global handles for elements we need to hide */
static lv_obj_t *peripheral_obj_handle;
static lv_obj_t *battery_obj_handle;

static uint16_t anim_rand(void) {
    static uint16_t seed = 42;
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return seed;
}

/* Timer Callback: Handles animations and interface visibility toggles */
static void screen_timer_cb(lv_timer_t *timer) {
    uint32_t current_wpm = zmk_wpm_get_wpm();
    int64_t now = k_uptime_get();
    
    if (current_wpm > 0) {
        last_typing_time = now;
        
        // If we just started typing, hide system widgets instantly
        if (!typing_mode) {
            typing_mode = true;
            lv_obj_add_flag(peripheral_obj_handle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(battery_obj_handle, LV_OBJ_FLAG_HIDDEN);
        }

        // Animate paws
        if (cat_state == 0) {
            cat_state = (anim_rand() % 2) + 1;
            cat_dsc.data = (cat_state == 1) ? cat_left_paw : cat_right_paw;
        } else {
            cat_state = 0;
            cat_dsc.data = cat_neutral;
        }
    } else {
        cat_state = 0;
        cat_dsc.data = cat_neutral;

        // If typing stopped and idle timeout passed, reveal the system status widgets
        if (typing_mode && (now - last_typing_time > IDLE_TIMEOUT_MS)) {
            typing_mode = false;
            lv_obj_remove_flag(peripheral_obj_handle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(battery_obj_handle, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    lv_image_set_src(cat_img, &cat_dsc);
    lv_obj_invalidate(cat_img);
}

static struct zmk_widget_battery_status battery_widget;
static struct zmk_widget_peripheral_status peripheral_widget;
static struct zmk_widget_wpm_status wpm_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* Built-in peripheral connection status — top left */
    zmk_widget_peripheral_status_init(&peripheral_widget, screen);
    peripheral_obj_handle = zmk_widget_peripheral_status_obj(&peripheral_widget);
    lv_obj_align(peripheral_obj_handle, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Built-in battery power status — top right */
    zmk_widget_battery_status_init(&battery_widget, screen);
    battery_obj_handle = zmk_widget_battery_status_obj(&battery_widget);
    lv_obj_align(battery_obj_handle, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* Built-in WPM readout engine text — bottom left */
    zmk_widget_wpm_status_init(&wpm_widget, screen);
    lv_obj_align(zmk_widget_wpm_status_obj(&wpm_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Custom Bongo Cat layout block — bottom right */
    cat_img = lv_image_create(screen);
    cat_dsc.data = cat_neutral;
    lv_image_set_src(cat_img, &cat_dsc);
    lv_obj_align(cat_img, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* Start execution clock loop */
    lv_timer_create(screen_timer_cb, ANIMATION_TIMER_MS, NULL);

    return screen;
}
