/*
 * Custom Corne central display
 * P keycap logo with glitch + built-in central widgets
 */

#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>
#include <lvgl.h>

/* ── P keycap from PARIX logo (24x32) ── */

static const uint8_t raw_p_keycap[4 * 24] = {
      0,  0,192,224,112, 48, 48, 48, 48, 48, 48, 48,
     48, 48, 48, 48, 48, 48, 48, 48,112,224,192,  0,
      0,  0,255,255,  0,  0,  0,  0,  0,254, 66, 66,
     66, 66,102, 60,  0,  0,  0,  0,  0,255,255,  0,
      0,  0,255,255,128,  0,  0,  0,  0,  7,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,128,255,255,  0,
      0,  0,  7, 15, 31, 31, 31, 31, 31, 31, 31, 31,
     31, 31, 31, 31, 31, 31, 31, 31, 31, 15,  7,  0,
};

#define P_WIDTH  24
#define P_HEIGHT 32
#define P_STRIDE 3

/* ── Glitch engine ── */

static uint16_t glitch_seed = 77;
static uint32_t glitch_next_ms;
static uint8_t glitch_frames_left = 0;

static uint16_t glitch_rand(void) {
    glitch_seed ^= glitch_seed << 7;
    glitch_seed ^= glitch_seed >> 9;
    glitch_seed ^= glitch_seed << 8;
    return glitch_seed;
}

static void apply_glitch(uint8_t *buf) {
    if (glitch_frames_left == 0) return;
    glitch_frames_left--;
    uint8_t fx = glitch_rand() % 4;

    if (fx == 0) {
        uint8_t page = glitch_rand() % 4;
        int8_t shift = (glitch_rand() % 7) - 3;
        if (shift > 0) {
            for (int c = P_WIDTH - 1; c >= shift; c--)
                buf[page * P_WIDTH + c] = buf[page * P_WIDTH + c - shift];
        } else if (shift < 0) {
            for (int c = 0; c < P_WIDTH + shift; c++)
                buf[page * P_WIDTH + c] = buf[page * P_WIDTH + c - shift];
        }
    } else if (fx == 1) {
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % (P_WIDTH - 4);
        uint8_t w = 3 + (glitch_rand() % 6);
        for (uint8_t c = col; c < col + w && c < P_WIDTH; c++)
            buf[page * P_WIDTH + c] = (uint8_t)glitch_rand();
    } else if (fx == 2) {
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % (P_WIDTH - 4);
        uint8_t w = 4 + (glitch_rand() % 10);
        for (uint8_t c = col; c < col + w && c < P_WIDTH; c++)
            buf[page * P_WIDTH + c] = ~buf[page * P_WIDTH + c];
    } else {
        uint8_t src = glitch_rand() % 4;
        uint8_t dst = (src + 1 + (glitch_rand() % 3)) % 4;
        uint8_t col = glitch_rand() % (P_WIDTH / 2);
        uint8_t w = P_WIDTH / 3 + (glitch_rand() % (P_WIDTH / 3));
        for (uint8_t c = col; c < col + w && c < P_WIDTH; c++)
            buf[dst * P_WIDTH + c] = buf[src * P_WIDTH + c];
    }
}

/* ── Logo rendering ── */

static void convert_p_to_lvgl_i1(const uint8_t *qmk_buf, uint8_t *out) {
    out[0] = 0xFF; out[1] = 0xFF; out[2] = 0xFF; out[3] = 0xFF;
    out[4] = 0x00; out[5] = 0x00; out[6] = 0x00; out[7] = 0xFF;
    uint8_t *pixels = out + 8;
    memset(pixels, 0, P_STRIDE * P_HEIGHT);
    for (int page = 0; page < 4; page++) {
        for (int col = 0; col < P_WIDTH; col++) {
            uint8_t val = qmk_buf[page * P_WIDTH + col];
            for (int bit = 0; bit < 8; bit++) {
                if (val & (1 << bit)) {
                    int y = page * 8 + bit;
                    pixels[y * P_STRIDE + (col / 8)] |= (0x80 >> (col % 8));
                }
            }
        }
    }
}

static uint8_t img_buf[8 + P_STRIDE * P_HEIGHT];
static lv_image_dsc_t p_dsc = {
    .header = { .cf = LV_COLOR_FORMAT_I1, .w = P_WIDTH, .h = P_HEIGHT },
    .data_size = sizeof(img_buf),
    .data = img_buf,
};

static lv_obj_t *p_img;
static int64_t last_glitch_time;
static bool logo_dirty = true;

static void update_p(bool with_glitch) {
    uint8_t buf[4 * P_WIDTH];
    memcpy(buf, raw_p_keycap, sizeof(buf));
    if (with_glitch) apply_glitch(buf);
    convert_p_to_lvgl_i1(buf, img_buf);
    p_dsc.data = img_buf;
    p_dsc.header.cf = LV_COLOR_FORMAT_I1;
    lv_image_set_src(p_img, &p_dsc);
    lv_obj_invalidate(p_img);
}

static void glitch_timer_cb(lv_timer_t *timer) {
    int64_t now = k_uptime_get();
    if (glitch_frames_left == 0) {
        if ((now - last_glitch_time) > (int64_t)glitch_next_ms) {
            last_glitch_time = now;
            glitch_frames_left = 2 + (glitch_rand() % 4);
            glitch_next_ms = 2000 + (glitch_rand() % 6000);
        } else if (logo_dirty) {
            update_p(false);
            logo_dirty = false;
        }
        return;
    }
    update_p(true);
    logo_dirty = true;
}

/* ── Screen layout (128x32) ──
 *
 * +------+----------+---------+
 * | [P]  | Layer    | Battery |
 * |      |          +---------+
 * |      | Output   |         |
 * +------+----------+---------+
 */

static struct zmk_widget_battery_status battery_widget;
static struct zmk_widget_layer_status layer_widget;
static struct zmk_widget_output_status output_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* P keycap — left */
    p_img = lv_image_create(screen);
    lv_obj_align(p_img, LV_ALIGN_LEFT_MID, 2, 0);
    update_p(false);
    logo_dirty = false;
    last_glitch_time = k_uptime_get();
    glitch_next_ms = 3000;
    lv_timer_create(glitch_timer_cb, 150, NULL);

    /* Layer — top center */
    zmk_widget_layer_status_init(&layer_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_widget),
                 LV_ALIGN_TOP_MID, 10, 0);

    /* Output status — bottom center */
    zmk_widget_output_status_init(&output_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_widget),
                 LV_ALIGN_BOTTOM_LEFT, 30, 0);

    /* Battery — top right */
    zmk_widget_battery_status_init(&battery_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_widget),
                 LV_ALIGN_TOP_RIGHT, 0, 0);

    return screen;
}
