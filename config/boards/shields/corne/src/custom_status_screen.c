/*
 * Custom Corne peripheral display
 * PARIX logo with glitch effects + battery + connection status
 */

#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <lvgl.h>

/* ── PARIX logo bitmap (QMK page format: 4 pages x 128 cols) ── */

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

/* ── Glitch engine ── */

static uint16_t glitch_seed = 42;
static uint32_t glitch_next_ms = 3000;
static uint8_t glitch_frames_left = 0;

static uint16_t glitch_rand(void) {
    glitch_seed ^= glitch_seed << 7;
    glitch_seed ^= glitch_seed >> 9;
    glitch_seed ^= glitch_seed << 8;
    return glitch_seed;
}

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
            for (int c = 127; c >= start + shift; c--)
                buf[page * 128 + c] = buf[page * 128 + c - shift];
        } else if (shift < 0) {
            for (int c = start; c < start + 32 && c < 128; c++)
                if (c - shift < 128)
                    buf[page * 128 + c] = buf[page * 128 + c - shift];
        }
    } else if (fx == 1) {
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % 100;
        uint8_t w = 8 + (glitch_rand() % 24);
        for (uint8_t c = col; c < col + w && c < 128; c++)
            buf[page * 128 + c] = (uint8_t)glitch_rand();
    } else if (fx == 2) {
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % 80;
        uint8_t w = 20 + (glitch_rand() % 40);
        for (uint8_t c = col; c < col + w && c < 128; c++)
            buf[page * 128 + c] = ~buf[page * 128 + c];
    } else {
        uint8_t src = glitch_rand() % 4;
        uint8_t dst = (src + 1 + (glitch_rand() % 3)) % 4;
        uint8_t col = glitch_rand() % 64;
        uint8_t w = 32 + (glitch_rand() % 48);
        for (uint8_t c = col; c < col + w && c < 128; c++)
            buf[dst * 128 + c] = buf[src * 128 + c];
    }
}

/* ── Logo image rendering ── */

static void convert_to_lvgl_i1(const uint8_t *qmk_buf, uint8_t *out) {
    /* I1 palette: index 0 = white (bg), index 1 = black (fg) */
    out[0] = 0xFF; out[1] = 0xFF; out[2] = 0xFF; out[3] = 0xFF;
    out[4] = 0x00; out[5] = 0x00; out[6] = 0x00; out[7] = 0xFF;

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

static uint8_t img_buf[8 + 512]; /* palette + 32 rows */
static lv_image_dsc_t logo_dsc = {
    .header = { .cf = LV_COLOR_FORMAT_I1, .w = 128, .h = 32 },
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
    logo_dsc.header.cf = LV_COLOR_FORMAT_I1;
    lv_image_set_src(logo_img, &logo_dsc);
    lv_obj_invalidate(logo_img);
}

static void glitch_timer_cb(lv_timer_t *timer) {
    int64_t now = k_uptime_get();

    if (glitch_frames_left == 0) {
        if ((now - last_glitch_time) > (int64_t)glitch_next_ms) {
            last_glitch_time = now;
            glitch_frames_left = 2 + (glitch_rand() % 5);
            glitch_next_ms = 2000 + (glitch_rand() % 6000);
        } else if (logo_dirty) {
            update_logo(false);
            logo_dirty = false;
        }
        return;
    }

    update_logo(true);
    logo_dirty = true;
}

/* ── Battery widget ── */

static lv_obj_t *battery_label;

struct battery_state {
    uint8_t level;
};

static void battery_update_cb(struct battery_state state) {
    char text[9];
    if (state.level > 95) {
        snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_FULL);
    } else if (state.level > 65) {
        snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_3);
    } else if (state.level > 35) {
        snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_2);
    } else if (state.level > 5) {
        snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_1);
    } else {
        snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_EMPTY);
    }
    lv_label_set_text(battery_label, text);
}

static struct battery_state battery_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(corne_battery, struct battery_state,
                            battery_update_cb, battery_get_state)
ZMK_SUBSCRIPTION(corne_battery, zmk_battery_state_changed);

/* ── Connection widget ── */

static lv_obj_t *conn_label;

struct conn_state {
    bool connected;
};

static void conn_update_cb(struct conn_state state) {
    lv_label_set_text(conn_label,
        state.connected ? LV_SYMBOL_WIFI " " LV_SYMBOL_OK
                        : LV_SYMBOL_WIFI " " LV_SYMBOL_CLOSE);
}

static struct conn_state conn_get_state(const zmk_event_t *eh) {
    return (struct conn_state){
        .connected = zmk_split_bt_peripheral_is_connected(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(corne_conn, struct conn_state,
                            conn_update_cb, conn_get_state)
ZMK_SUBSCRIPTION(corne_conn, zmk_split_peripheral_status_changed);

/* ── Screen layout ──
 *
 * 128x32 display:
 * +------------------------------------------+
 * |          PARIX logo (128x22)             |
 * +--------------------+---------------------+
 * | WiFi status        |        Battery icon |
 * +--------------------+---------------------+
 */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* Logo — top 22 pixels */
    logo_img = lv_image_create(screen);
    lv_obj_align(logo_img, LV_ALIGN_TOP_MID, 0, 0);
    update_logo(false);
    logo_dirty = false;
    last_glitch_time = k_uptime_get();
    lv_timer_create(glitch_timer_cb, 150, NULL);

    /* Status bar — bottom, overlays logo */
    lv_obj_t *status_bar = lv_obj_create(screen);
    lv_obj_set_size(status_bar, 128, 12);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);

    /* Connection status — left */
    conn_label = lv_label_create(status_bar);
    lv_obj_align(conn_label, LV_ALIGN_LEFT_MID, 2, 0);
    corne_conn_init();

    /* Battery — right */
    battery_label = lv_label_create(status_bar);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -2, 0);
    corne_battery_init();

    return screen;
}
