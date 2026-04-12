/*
 * Custom Corne display — PARIX logo with glitch effects
 * Ported from QMK Sofle implementation to ZMK/LVGL 9
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <lvgl.h>

/* 128x32 monochrome logo — PARIX keycap branding
 * Stored in QMK page format: 4 pages x 128 columns, each byte is 8 vertical pixels
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

/* Glitch effect state */
static uint16_t glitch_seed = 42;
static uint32_t glitch_next_ms = 3000;
static uint8_t glitch_frames_left = 0;

static uint16_t glitch_rand(void) {
    glitch_seed ^= glitch_seed << 7;
    glitch_seed ^= glitch_seed >> 9;
    glitch_seed ^= glitch_seed << 8;
    return glitch_seed;
}

/* Convert QMK page-column format to LVGL horizontal bitmap
 * QMK: 4 pages x 128 cols, each byte = 8 vertical pixels (LSB = top)
 * LVGL I1: row-major, 128 bits per row = 16 bytes per row, 32 rows, MSB first
 */
static void convert_logo_to_lvgl(const uint8_t *qmk_buf, uint8_t *lvgl_buf) {
    memset(lvgl_buf, 0, 16 * 32);
    for (int page = 0; page < 4; page++) {
        for (int col = 0; col < 128; col++) {
            uint8_t val = qmk_buf[page * 128 + col];
            for (int bit = 0; bit < 8; bit++) {
                if (val & (1 << bit)) {
                    int y = page * 8 + bit;
                    int x = col;
                    lvgl_buf[y * 16 + (x / 8)] |= (0x80 >> (x % 8));
                }
            }
        }
    }
}

static void apply_glitch(uint8_t *buf) {
    if (glitch_frames_left == 0) {
        return;
    }
    glitch_frames_left--;

    uint8_t fx = glitch_rand() % 4;

    if (fx == 0) {
        /* Horizontal slice shift */
        uint8_t row = (glitch_rand() % 4) * 8;
        uint8_t h = 4 + (glitch_rand() % 8);
        int8_t shift = (glitch_rand() % 13) - 6;
        for (int y = row; y < row + h && y < 32; y++) {
            if (shift > 0) {
                for (int c = 127; c >= shift; c--) {
                    int src_byte = y * 16 + ((c - shift) / 8);
                    int src_bit = 7 - ((c - shift) % 8);
                    int dst_byte = y * 16 + (c / 8);
                    int dst_bit = 7 - (c % 8);
                    if (buf[src_byte] & (1 << src_bit)) {
                        buf[dst_byte] |= (1 << dst_bit);
                    } else {
                        buf[dst_byte] &= ~(1 << dst_bit);
                    }
                }
            } else if (shift < 0) {
                for (int c = 0; c < 128 + shift; c++) {
                    int src_c = c - shift;
                    if (src_c >= 128) continue;
                    int src_byte = y * 16 + (src_c / 8);
                    int src_bit = 7 - (src_c % 8);
                    int dst_byte = y * 16 + (c / 8);
                    int dst_bit = 7 - (c % 8);
                    if (buf[src_byte] & (1 << src_bit)) {
                        buf[dst_byte] |= (1 << dst_bit);
                    } else {
                        buf[dst_byte] &= ~(1 << dst_bit);
                    }
                }
            }
        }
    } else if (fx == 1) {
        /* Noise block */
        uint8_t row = glitch_rand() % 28;
        uint8_t col = glitch_rand() % 100;
        uint8_t w = 8 + (glitch_rand() % 24);
        uint8_t h = 2 + (glitch_rand() % 6);
        for (int y = row; y < row + h && y < 32; y++) {
            for (int c = col; c < col + w && c < 128; c++) {
                int byte_idx = y * 16 + (c / 8);
                int bit_idx = 7 - (c % 8);
                if (glitch_rand() & 1) {
                    buf[byte_idx] |= (1 << bit_idx);
                } else {
                    buf[byte_idx] &= ~(1 << bit_idx);
                }
            }
        }
    } else if (fx == 2) {
        /* Invert strip */
        uint8_t row = glitch_rand() % 28;
        uint8_t h = 2 + (glitch_rand() % 6);
        uint8_t col = glitch_rand() % 80;
        uint8_t w = 20 + (glitch_rand() % 40);
        for (int y = row; y < row + h && y < 32; y++) {
            for (int c = col; c < col + w && c < 128; c++) {
                int byte_idx = y * 16 + (c / 8);
                int bit_idx = 7 - (c % 8);
                buf[byte_idx] ^= (1 << bit_idx);
            }
        }
    } else {
        /* Screen tear: duplicate rows */
        uint8_t src_row = glitch_rand() % 24;
        uint8_t dst_row = (src_row + 4 + (glitch_rand() % 8)) % 32;
        uint8_t h = 2 + (glitch_rand() % 4);
        for (int i = 0; i < h && src_row + i < 32 && dst_row + i < 32; i++) {
            memcpy(&buf[(dst_row + i) * 16], &buf[(src_row + i) * 16], 16);
        }
    }
}

/* LVGL image buffers */
static uint8_t logo_pixel_buf[512];
static const uint8_t palette[] = {
    0x00, 0x00, 0x00, 0xFF, /* index 0: black */
    0xFF, 0xFF, 0xFF, 0xFF, /* index 1: white */
};
static uint8_t logo_combined[sizeof(palette) + sizeof(logo_pixel_buf)];

static lv_image_dsc_t logo_dsc = {
    .header = {
        .cf = LV_COLOR_FORMAT_I1,
        .w = 128,
        .h = 32,
    },
    .data_size = sizeof(logo_combined),
    .data = logo_combined,
};

static lv_obj_t *logo_img;
static int64_t last_glitch_time;

static void glitch_timer_cb(lv_timer_t *timer) {
    int64_t now = k_uptime_get();
    int64_t elapsed = now - last_glitch_time;

    if (glitch_frames_left == 0 && elapsed > (int64_t)glitch_next_ms) {
        last_glitch_time = now;
        glitch_frames_left = 2 + (glitch_rand() % 5);
        glitch_next_ms = 2000 + (glitch_rand() % 6000);
    }

    convert_logo_to_lvgl(raw_logo, logo_pixel_buf);
    apply_glitch(logo_pixel_buf);

    memcpy(logo_combined, palette, sizeof(palette));
    memcpy(logo_combined + sizeof(palette), logo_pixel_buf, sizeof(logo_pixel_buf));

    logo_dsc.data = logo_combined;
    lv_image_set_src(logo_img, &logo_dsc);
    lv_obj_invalidate(logo_img);
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_size(screen, 128, 32);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    logo_img = lv_image_create(screen);
    lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, 0);

    /* Initial render */
    convert_logo_to_lvgl(raw_logo, logo_pixel_buf);
    memcpy(logo_combined, palette, sizeof(palette));
    memcpy(logo_combined + sizeof(palette), logo_pixel_buf, sizeof(logo_pixel_buf));
    logo_dsc.data = logo_combined;
    lv_image_set_src(logo_img, &logo_dsc);

    last_glitch_time = k_uptime_get();

    /* Glitch animation timer — fires every 150ms */
    lv_timer_create(glitch_timer_cb, 150, NULL);

    return screen;
}
