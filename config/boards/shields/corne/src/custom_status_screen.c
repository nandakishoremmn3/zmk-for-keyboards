/*
 * Custom Corne display — PARIX logo with glitch effects
 * Ported from QMK Sofle implementation to ZMK/LVGL 9
 */

#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <lvgl.h>

/* 128x32 monochrome logo — PARIX keycap branding
 * QMK page format: 4 pages x 128 columns, each byte = 8 vertical pixels (LSB = top)
 */
static const uint8_t raw_logo[512] = {
    0,  0,  0,  0,  0,  0,192,224,112, 48, 48, 48, 48, 48, 48, 48,
   48, 48, 48, 48, 48, 48, 48, 48,112,224,192,  0,  0,  0,192,224,
  112, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
  112,224,192,  0,  0,  0,192,224,112, 48, 48, 48, 48, 48, 48, 48,
   48, 48, 48, 48, 48, 48, 48, 48,112,224,192,  0,  0,  0,192,224,
  112, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
  112,224,192,  0,  0,  0,192,224,112, 48, 48, 48, 48, 48, 48, 48,
   48, 48, 48, 48, 48, 48, 48, 48,112,224,192,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,255,255,  0,  0,  0,  0,  0,254, 66, 66,
   66, 66,102, 60,  0,  0,  0,  0,  0,255,255,  0,  0,  0,255,255,
    0,  0,  0,  0,  0,192,188,130,188,192,  0,  0,  0,  0,  0,  0,
    0,255,255,  0,  0,  0,255,255,  0,  0,  0,  0,254, 66, 66, 66,
   66,166, 60,  0,  0,  0,  0,  0,  0,255,255,  0,  0,  0,255,255,
    0,  0,  0,  0,  0,  0,  2,  2,254,  2,  2,  0,  0,  0,  0,  0,
    0,255,255,  0,  0,  0,255,255,  0,  0,  0,  0,  2, 12,216, 96,
  216,  4,  2,  0,  0,  0,  0,  0,  0,255,255,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,255,255,128,  0,  0,  0,  0,  7,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,128,255,255,  0,  0,  0,255,255,
  128,  0,  0,  0,  6,  1,  0,  0,  0,  1,  6,  0,  0,  0,  0,  0,
  128,255,255,  0,  0,  0,255,255,128,  0,  0,  0,  7,  0,  0,  0,
    0,  0,  3,  4,  0,  0,  0,  0,128,255,255,  0,  0,  0,255,255,
  128,  0,  0,  0,  0,  0,  4,  4,  7,  4,  4,  0,  0,  0,  0,  0,
  128,255,255,  0,  0,  0,255,255,128,  0,  0,  0,  4,  3,  0,  0,
    0,  3,  4,  0,  0,  0,  0,  0,128,255,255,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  7, 15, 31, 31, 31, 31, 31, 31, 31, 31,
   31, 31, 31, 31, 31, 31, 31, 31, 31, 15,  7,  0,  0,  0,  7, 15,
   31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   31, 15,  7,  0,  0,  0,  7, 15, 31, 31, 31, 31, 31, 31, 31, 31,
   31, 31, 31, 31, 31, 31, 31, 31, 31, 15,  7,  0,  0,  0,  7, 15,
   31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   31, 15,  7,  0,  0,  0,  7, 15, 31, 31, 31, 31, 31, 31, 31, 31,
   31, 31, 31, 31, 31, 31, 31, 31, 31, 15,  7,  0,  0,  0,  0,  0,
};

/* Glitch PRNG */
static uint16_t glitch_seed = 42;

static uint16_t glitch_rand(void) {
    glitch_seed ^= glitch_seed << 7;
    glitch_seed ^= glitch_seed >> 9;
    glitch_seed ^= glitch_seed << 8;
    return glitch_seed;
}

/* Glitch state */
static uint32_t glitch_next_ms = 3000;
static uint8_t glitch_frames_left = 0;

/* Apply glitch to QMK page-format buffer */
static void apply_glitch(uint8_t *buf) {
    if (glitch_frames_left == 0) {
        return;
    }
    glitch_frames_left--;

    uint8_t fx = glitch_rand() % 4;

    if (fx == 0) {
        uint8_t page = glitch_rand() % 4;
        uint8_t start = glitch_rand() % 96;
        int8_t shift = (glitch_rand() % 13) - 6;
        if (shift > 0) {
            for (int c = 127; c >= start + shift; c--) {
                buf[page * 128 + c] = buf[page * 128 + c - shift];
            }
        } else if (shift < 0) {
            for (int c = start; c < start + 32 && c < 128; c++) {
                if (c - shift < 128) {
                    buf[page * 128 + c] = buf[page * 128 + c - shift];
                }
            }
        }
    } else if (fx == 1) {
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % 100;
        uint8_t w = 8 + (glitch_rand() % 24);
        for (uint8_t c = col; c < col + w && c < 128; c++) {
            buf[page * 128 + c] = (uint8_t)glitch_rand();
        }
    } else if (fx == 2) {
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % 80;
        uint8_t w = 20 + (glitch_rand() % 40);
        for (uint8_t c = col; c < col + w && c < 128; c++) {
            buf[page * 128 + c] = ~buf[page * 128 + c];
        }
    } else {
        uint8_t src = glitch_rand() % 4;
        uint8_t dst = (src + 1 + (glitch_rand() % 3)) % 4;
        uint8_t col = glitch_rand() % 64;
        uint8_t w = 32 + (glitch_rand() % 48);
        for (uint8_t c = col; c < col + w && c < 128; c++) {
            buf[dst * 128 + c] = buf[src * 128 + c];
        }
    }
}

/* Convert QMK page-column to LVGL I1 row-major format
 * LVGL I1: 8-byte palette + row-major pixel data (MSB = leftmost)
 */
static void convert_to_lvgl_i1(const uint8_t *qmk_buf, uint8_t *out) {
    /* Palette: index 0 = white (background), index 1 = black (foreground) */
    out[0] = 0xFF; out[1] = 0xFF; out[2] = 0xFF; out[3] = 0xFF; /* white */
    out[4] = 0x00; out[5] = 0x00; out[6] = 0x00; out[7] = 0xFF; /* black */

    uint8_t *pixels = out + 8;
    memset(pixels, 0, 16 * 32);

    for (int page = 0; page < 4; page++) {
        for (int col = 0; col < 128; col++) {
            uint8_t val = qmk_buf[page * 128 + col];
            for (int bit = 0; bit < 8; bit++) {
                if (val & (1 << bit)) {
                    int y = page * 8 + bit;
                    pixels[y * 16 + (col / 8)] |= (0x80 >> (col % 8));
                }
            }
        }
    }
}

/* Image buffer: 8 bytes palette + 512 bytes pixels */
static uint8_t img_buf[8 + 512];

static lv_image_dsc_t logo_dsc = {
    .header = {
        .cf = LV_COLOR_FORMAT_I1,
        .w = 128,
        .h = 32,
    },
    .data_size = sizeof(img_buf),
    .data = img_buf,
};

static lv_obj_t *logo_img;
static int64_t last_glitch_time;

static bool logo_dirty = true;

static void update_logo(bool with_glitch) {
    uint8_t buf[512];
    memcpy(buf, raw_logo, 512);
    if (with_glitch) {
        apply_glitch(buf);
    }
    convert_to_lvgl_i1(buf, img_buf);
    logo_dsc.data = img_buf;
    logo_dsc.header.cf = LV_COLOR_FORMAT_I1; /* force LVGL to re-read */
    lv_image_set_src(logo_img, &logo_dsc);
    lv_obj_invalidate(logo_img);
}

static void glitch_timer_cb(lv_timer_t *timer) {
    int64_t now = k_uptime_get();

    if (glitch_frames_left == 0) {
        /* No active glitch — check if it's time for a new burst */
        if ((now - last_glitch_time) > (int64_t)glitch_next_ms) {
            last_glitch_time = now;
            glitch_frames_left = 2 + (glitch_rand() % 5);
            glitch_next_ms = 2000 + (glitch_rand() % 6000);
        } else if (logo_dirty) {
            /* Restore clean logo after glitch ends */
            update_logo(false);
            logo_dirty = false;
        }
        return;
    }

    /* Active glitch — render glitched frame */
    update_logo(true);
    logo_dirty = true;
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    logo_img = lv_image_create(screen);
    lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, 0);

    /* Initial render — clean logo */
    update_logo(false);
    logo_dirty = false;

    last_glitch_time = k_uptime_get();
    lv_timer_create(glitch_timer_cb, 150, NULL);

    return screen;
}
