/*
 * Minimal custom display test for Corne right side
 */

#include <stdbool.h>
#include <zmk/display.h>
#include <lvgl.h>

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "PARIX");
    lv_obj_center(label);

    return screen;
}
