/*
 * Custom Corne peripheral display
 * P keycap logo (from PARIX) + glitch + battery + connection
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

/* ── P keycap extracted from PARIX logo (cols 4-27, 24px wide x 32px tall) ──
 * QMK page format: 4 pages, 24 columns each
 */
static const uint8_t raw_p_keycap[4 * 24] = {
    /* Page 0 (rows 0-7) — top border */
      0,  0,192,224,112, 48, 48, 48, 48, 48, 48, 48,
     48, 48, 48, 48, 48, 48, 48, 48,112,224,192,  0,
    /* Page 1 (rows 8-15) — upper P letter */
      0,  0,255,255,  0,  0,  0,  0,  0,254, 66, 66,
     66, 66,102, 60,  0,  0,  0,  0,  0,255,255,  0,
    /* Page 2 (rows 16-23) — lower P letter */
      0,  0,255,255,128,  0,  0,  0,  0,  7,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,128,255,255,  0,
    /* Page 3 (rows 24-31) — bottom border */
      0,  0,  7, 15, 31, 31, 31, 31, 31, 31, 31, 31,
     31, 31, 31, 31, 31, 31, 31, 31, 31, 15,  7,  0,
};

#define P_WIDTH  24
#define P_HEIGHT 32
#define P_STRIDE 3  /* ceil(24/8) = 3 bytes per row */

/* ── Glitch engine ── */

static uint16_t glitch_seed = 42;
static uint32_t glitch_next_ms;
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
        /* Horizontal shift in a page */
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
        /* Noise block */
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % (P_WIDTH - 4);
        uint8_t w = 3 + (glitch_rand() % 6);
        for (uint8_t c = col; c < col + w && c < P_WIDTH; c++)
            buf[page * P_WIDTH + c] = (uint8_t)glitch_rand();
    } else if (fx == 2) {
        /* Invert strip */
        uint8_t page = glitch_rand() % 4;
        uint8_t col = glitch_rand() % (P_WIDTH - 4);
        uint8_t w = 4 + (glitch_rand() % 10);
        for (uint8_t c = col; c < col + w && c < P_WIDTH; c++)
            buf[page * P_WIDTH + c] = ~buf[page * P_WIDTH + c];
    } else {
        /* Screen tear */
        uint8_t src = glitch_rand() % 4;
        uint8_t dst = (src + 1 + (glitch_rand() % 3)) % 4;
        uint8_t col = glitch_rand() % (P_WIDTH / 2);
        uint8_t w = P_WIDTH / 3 + (glitch_rand() % (P_WIDTH / 3));
        for (uint8_t c = col; c < col + w && c < P_WIDTH; c++)
            buf[dst * P_WIDTH + c] = buf[src * P_WIDTH + c];
    }
}

/* ── Convert to LVGL I1 image ── */

static void convert_p_to_lvgl_i1(const uint8_t *qmk_buf, uint8_t *out) {
    /* I1 palette */
    out[0] = 0xFF; out[1] = 0xFF; out[2] = 0xFF; out[3] = 0xFF; /* idx 0: white */
    out[4] = 0x00; out[5] = 0x00; out[6] = 0x00; out[7] = 0xFF; /* idx 1: black */

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
    if (with_glitch) {
        apply_glitch(buf);
    }
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

/* ── Battery widget ── */

static lv_obj_t *battery_label;

struct battery_state { uint8_t level; };

static lv_obj_t *battery_icon_label;

static void battery_update_cb(struct battery_state state) {
    char pct[5];
    snprintf(pct, sizeof(pct), "%u%%", state.level);
    lv_label_set_text(battery_label, pct);

    if (state.level > 75) {
        lv_label_set_text(battery_icon_label, "[||||]");
    } else if (state.level > 50) {
        lv_label_set_text(battery_icon_label, "[||| ]");
    } else if (state.level > 25) {
        lv_label_set_text(battery_icon_label, "[||  ]");
    } else if (state.level > 5) {
        lv_label_set_text(battery_icon_label, "[|   ]");
    } else {
        lv_label_set_text(battery_icon_label, "[    ]");
    }
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

struct conn_state { bool connected; };

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

/* ── Screen layout (128x32) ──
 *
 * +------+---------------------+
 * |      |  WiFi status        |
 * | [P]  |                     |
 * |      |  Battery %          |
 * +------+---------------------+
 *  ~30px        ~98px
 */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* P keycap — center */
    p_img = lv_image_create(screen);
    lv_obj_align(p_img, LV_ALIGN_CENTER, 0, 0);
    update_p(false);
    logo_dirty = false;
    last_glitch_time = k_uptime_get();
    glitch_next_ms = 3000;
    lv_timer_create(glitch_timer_cb, 150, NULL);

    /* Connection — top left (default Montserrat font has symbols) */
    conn_label = lv_label_create(screen);
    lv_obj_align(conn_label, LV_ALIGN_TOP_LEFT, 2, 2);
    corne_conn_init();

    /* Battery icon — top right */
    battery_icon_label = lv_label_create(screen);
    lv_obj_set_style_text_font(battery_icon_label, &lv_font_unscii_8, 0);
    lv_obj_align(battery_icon_label, LV_ALIGN_TOP_RIGHT, -2, 2);

    /* Battery % — below icon, smaller font */
    battery_label = lv_label_create(screen);
    lv_obj_set_style_text_font(battery_label, &lv_font_unscii_8, 0);
    lv_obj_align(battery_label, LV_ALIGN_BOTTOM_RIGHT, -2, 0);
    corne_battery_init();

    return screen;
}
