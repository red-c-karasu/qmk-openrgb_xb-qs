/* Copyright 2020 Kasper
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include "keymap_introspection.h"
#ifndef RAW_ENABLE
#    error "RAW_ENABLE is not enabled"
#endif

#include "version.h"
#include "quantum.h"
#include "openrgb.h"
#include "raw_hid.h"
#include "string.h"
#include <color.h>

#if !defined(OPENRGB_DIRECT_MODE_STARTUP_RED)
#    define OPENRGB_DIRECT_MODE_STARTUP_RED 0
#endif

#if !defined(OPENRGB_DIRECT_MODE_STARTUP_GREEN)
#    define OPENRGB_DIRECT_MODE_STARTUP_GREEN 0
#endif

#if !defined(OPENRGB_DIRECT_MODE_STARTUP_BLUE)
#    define OPENRGB_DIRECT_MODE_STARTUP_BLUE 255
#endif

RGB                  g_openrgb_direct_mode_colors[RGB_MATRIX_LED_COUNT] = {[0 ... RGB_MATRIX_LED_COUNT - 1] = {OPENRGB_DIRECT_MODE_STARTUP_GREEN, OPENRGB_DIRECT_MODE_STARTUP_RED, OPENRGB_DIRECT_MODE_STARTUP_BLUE}};

/*
 * Protocol translation layer.
 *
 * The OpenRGB host assigns sequential counter values (1, 2, 3, ...) to modes
 * in its fixed constructor order. The counter value is what gets sent back to
 * the firmware via set_mode. The host also uses find() on the enabled_modes
 * list to check for specific protocol IDs (from its Modes enum).
 *
 * Problem: This firmware has three extra effects (PIXEL_RAIN, PIXEL_FLOW,
 * PIXEL_FRACTAL) inserted at firmware enum positions 28-30, shifting all
 * subsequent firmware enum values +3 compared to what the host expects.
 * OPENRGB_DIRECT moved from what would be position 42 to position 45.
 *
 * Solution: This table maps between host protocol IDs and actual firmware
 * enum values. Table order MUST match the host's constructor order so that
 * table index+1 == the counter value the host assigns.
 *
 * Firmware enum (from rgb_matrix_effects.inc compile order):
 *   1  = SOLID_COLOR             17 = CYCLE_OUT_IN_DUAL
 *   2  = ALPHAS_MODS             18 = CYCLE_PINWHEEL
 *   3  = GRADIENT_UP_DOWN        19 = CYCLE_SPIRAL
 *   4  = GRADIENT_LEFT_RIGHT     20 = DUAL_BEACON
 *   5  = BREATHING               21 = RAINBOW_BEACON
 *   6  = BAND_SAT                22 = RAINBOW_PINWHEELS
 *   7  = BAND_VAL                23 = RAINDROPS
 *   8  = BAND_PINWHEEL_SAT       24 = JELLYBEAN_RAINDROPS
 *   9  = BAND_PINWHEEL_VAL       25 = HUE_BREATHING
 *   10 = BAND_SPIRAL_SAT         26 = HUE_PENDULUM
 *   11 = BAND_SPIRAL_VAL         27 = HUE_WAVE
 *   12 = CYCLE_ALL               28 = PIXEL_RAIN        (no host support)
 *   13 = CYCLE_LEFT_RIGHT        29 = PIXEL_FLOW        (no host support)
 *   14 = CYCLE_UP_DOWN           30 = PIXEL_FRACTAL     (no host support)
 *   15 = RAINBOW_MOVING_CHEVRON  31 = TYPING_HEATMAP
 *   16 = CYCLE_OUT_IN            32 = DIGITAL_RAIN
 *                                33-44 = reactive/splash effects
 *                                45 = OPENRGB_DIRECT
 */
typedef struct {
    uint8_t protocol_id;     /* value the host looks for via find() in enabled_modes */
    uint8_t firmware_value;  /* actual value for rgb_matrix_mode()                   */
} protocol_mode_entry;

static const protocol_mode_entry openrgb_mode_map[] = {
    /*  Table is in HOST CONSTRUCTOR ORDER.                              */
    /*  { host_enum_value, firmware_enum_value }                         */

    /* 1. SOLID_COLOR — always enabled */
    { 2,  1  },

#ifndef DISABLE_RGB_MATRIX_ALPHAS_MODS
    /* 2. ALPHA_MOD */
    { 3,  2  },
#endif

#ifndef DISABLE_RGB_MATRIX_GRADIENT_UP_DOWN
    /* 3. GRADIENT_UP_DOWN */
    { 4,  3  },
#endif

#ifndef DISABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
    /* 4. GRADIENT_LEFT_RIGHT */
    { 5,  4  },
#endif

#ifndef DISABLE_RGB_MATRIX_BREATHING
    /* 5. BREATHING */
    { 6,  5  },
#endif

#ifndef DISABLE_RGB_MATRIX_BAND_SAT
    /* 6. BAND_SAT */
    { 7,  6  },
#endif

#ifndef DISABLE_RGB_MATRIX_BAND_VAL
    /* 7. BAND_VAL */
    { 8,  7  },
#endif

#ifndef DISABLE_RGB_MATRIX_BAND_PINWHEEL_SAT
    /* 8. BAND_PINWHEEL_SAT */
    { 9,  8  },
#endif

#ifndef DISABLE_RGB_MATRIX_BAND_PINWHEEL_VAL
    /* 9. BAND_PINWHEEL_VAL */
    { 10, 9  },
#endif

#ifndef DISABLE_RGB_MATRIX_BAND_SPIRAL_SAT
    /* 10. BAND_SPIRAL_SAT */
    { 11, 10 },
#endif

#ifndef DISABLE_RGB_MATRIX_BAND_SPIRAL_VAL
    /* 11. BAND_SPIRAL_VAL */
    { 12, 11 },
#endif

#ifndef DISABLE_RGB_MATRIX_CYCLE_ALL
    /* 12. CYCLE_ALL */
    { 13, 12 },
#endif

#ifndef DISABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
    /* 13. CYCLE_LEFT_RIGHT */
    { 14, 13 },
#endif

#ifndef DISABLE_RGB_MATRIX_CYCLE_UP_DOWN
    /* 14. CYCLE_UP_DOWN */
    { 15, 14 },
#endif

    /* NOTE: Host constructor order differs from firmware enum order here.
     * Host: CYCLE_OUT_IN, CYCLE_OUT_IN_DUAL, RAINBOW_MOVING_CHEVRON
     * Firmware: RAINBOW_MOVING_CHEVRON(15), CYCLE_OUT_IN(16), CYCLE_OUT_IN_DUAL(17)
     * The translation table corrects this so mode names match visuals. */

#ifndef DISABLE_RGB_MATRIX_CYCLE_OUT_IN
    /* 15. CYCLE_OUT_IN — host enum 16, firmware enum 16 */
    { 16, 16 },
#endif

#ifndef DISABLE_RGB_MATRIX_CYCLE_OUT_IN_DUAL
    /* 16. CYCLE_OUT_IN_DUAL — host enum 17, firmware enum 17 */
    { 17, 17 },
#endif

#ifndef DISABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
    /* 17. RAINBOW_MOVING_CHEVRON — host enum 18, firmware enum 15 */
    { 18, 15 },
#endif

#ifndef DISABLE_RGB_MATRIX_CYCLE_PINWHEEL
    /* 18. CYCLE_PINWHEEL */
    { 19, 18 },
#endif

#ifndef DISABLE_RGB_MATRIX_CYCLE_SPIRAL
    /* 19. CYCLE_SPIRAL */
    { 20, 19 },
#endif

#ifndef DISABLE_RGB_MATRIX_DUAL_BEACON
    /* 20. DUAL_BEACON */
    { 21, 20 },
#endif

#ifndef DISABLE_RGB_MATRIX_RAINBOW_BEACON
    /* 21. RAINBOW_BEACON */
    { 22, 21 },
#endif

#ifndef DISABLE_RGB_MATRIX_RAINBOW_PINWHEELS
    /* 22. RAINBOW_PINWHEELS */
    { 23, 22 },
#endif

#ifndef DISABLE_RGB_MATRIX_RAINDROPS
    /* 23. RAINDROPS */
    { 24, 23 },
#endif

#ifndef DISABLE_RGB_MATRIX_JELLYBEAN_RAINDROPS
    /* 24. JELLYBEAN_RAINDROPS */
    { 25, 24 },
#endif

#ifndef DISABLE_RGB_MATRIX_HUE_BREATHING
    /* 25. HUE_BREATHING */
    { 26, 25 },
#endif

#ifndef DISABLE_RGB_MATRIX_HUE_PENDULUM
    /* 26. HUE_PENDULUM */
    { 27, 26 },
#endif

#ifndef DISABLE_RGB_MATRIX_HUE_WAVE
    /* 27. HUE_WAVE */
    { 28, 27 },
#endif

    /* PIXEL_RAIN (fw 28), PIXEL_FLOW (fw 29), PIXEL_FRACTAL (fw 30)    */
    /* are OMITTED — the host has no constructor blocks for them.         */

#if defined(RGB_MATRIX_FRAMEBUFFER_EFFECTS) && !defined(DISABLE_RGB_MATRIX_TYPING_HEATMAP)
    /* 28. TYPING_HEATMAP — host enum 29, firmware enum 31 (shifted +3) */
    { 29, 31 },
#endif

#if defined(RGB_MATRIX_FRAMEBUFFER_EFFECTS) && !defined(DISABLE_RGB_MATRIX_DIGITAL_RAIN)
    /* 29. DIGITAL_RAIN — host enum 30, firmware enum 32 */
    { 30, 32 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE)
    /* 30. SOLID_REACTIVE_SIMPLE */
    { 31, 33 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE)
    /* 31. SOLID_REACTIVE */
    { 32, 34 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_WIDE)
    /* 32. SOLID_REACTIVE_WIDE */
    { 33, 35 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE)
    /* 33. SOLID_REACTIVE_MULTIWIDE */
    { 34, 36 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_CROSS)
    /* 34. SOLID_REACTIVE_CROSS */
    { 35, 37 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_MULTICROSS)
    /* 35. SOLID_REACTIVE_MULTICROSS */
    { 36, 38 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_NEXUS)
    /* 36. SOLID_REACTIVE_NEXUS */
    { 37, 39 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS)
    /* 37. SOLID_REACTIVE_MULTINEXUS */
    { 38, 40 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SPLASH)
    /* 38. SPLASH */
    { 39, 41 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_MULTISPLASH)
    /* 39. MULTISPLASH */
    { 40, 42 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_SPLASH)
    /* 40. SOLID_SPLASH */
    { 41, 43 },
#endif

#if defined(RGB_MATRIX_KEYREACTIVE_ENABLED) && !defined(DISABLE_RGB_MATRIX_SOLID_MULTISPLASH)
    /* 41. SOLID_MULTISPLASH */
    { 42, 44 },
#endif

    /* 42. OPENRGB_DIRECT — always last, always enabled */
    { 1,  45 },
};

#define OPENRGB_MODE_MAP_SIZE (sizeof(openrgb_mode_map) / sizeof(openrgb_mode_map[0]))

static uint8_t raw_hid_buffer[RAW_EPSIZE];

/*
 * Translate a host counter value (1-based) to a firmware rgb_matrix enum value.
 * Returns 0 if the counter is out of range (0 == RGB_MATRIX_NONE).
 */
static uint8_t protocol_to_firmware_mode(uint8_t counter) {
    if (counter < 1 || counter > OPENRGB_MODE_MAP_SIZE) {
        return 0;
    }
    return openrgb_mode_map[counter - 1].firmware_value;
}

/*
 * Translate a firmware rgb_matrix enum value back to the host counter value.
 * Returns 0 if the firmware mode is not in the table (e.g. PIXEL effects).
 */
static uint8_t firmware_to_protocol_mode(uint8_t fw_mode) {
    for (uint8_t i = 0; i < OPENRGB_MODE_MAP_SIZE; i++) {
        if (openrgb_mode_map[i].firmware_value == fw_mode) {
            return i + 1;  /* counter is 1-based */
        }
    }
    return 0;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    switch (*data) {
        case OPENRGB_GET_PROTOCOL_VERSION:
            openrgb_get_protocol_version();
            break;
        case OPENRGB_GET_QMK_VERSION:
            openrgb_get_qmk_version();
            break;
        case OPENRGB_GET_DEVICE_INFO:
            openrgb_get_device_info();
            break;
        case OPENRGB_GET_MODE_INFO:
            openrgb_get_mode_info();
            break;
        case OPENRGB_GET_LED_INFO:
            openrgb_get_led_info(data);
            break;
        case OPENRGB_GET_ENABLED_MODES:
            openrgb_get_enabled_modes();
            break;

        case OPENRGB_SET_MODE:
            openrgb_set_mode(data);
            break;
        case OPENRGB_DIRECT_MODE_SET_SINGLE_LED:
            openrgb_direct_mode_set_single_led(data);
            break;
        case OPENRGB_DIRECT_MODE_SET_LEDS:
            openrgb_direct_mode_set_leds(data);
            break;
    }

    if (*data != OPENRGB_DIRECT_MODE_SET_LEDS) {
        raw_hid_buffer[RAW_EPSIZE - 1] = OPENRGB_END_OF_MESSAGE;
        raw_hid_send(raw_hid_buffer, RAW_EPSIZE);
        memset(raw_hid_buffer, 0x00, RAW_EPSIZE);
    }
}

void openrgb_get_protocol_version(void) {
    raw_hid_buffer[0] = OPENRGB_GET_PROTOCOL_VERSION;
    raw_hid_buffer[1] = OPENRGB_PROTOCOL_VERSION;
}
void openrgb_get_qmk_version(void) {
    raw_hid_buffer[0]    = OPENRGB_GET_QMK_VERSION;
    uint8_t current_byte = 1;
    for (uint8_t i = 0; (current_byte < (RAW_EPSIZE - 2)) && (QMK_VERSION[i] != 0); i++) {
        raw_hid_buffer[current_byte] = QMK_VERSION[i];
        current_byte++;
    }
}
void openrgb_get_device_info(void) {
    raw_hid_buffer[0] = OPENRGB_GET_DEVICE_INFO;
    raw_hid_buffer[1] = RGB_MATRIX_LED_COUNT;
    raw_hid_buffer[2] = MATRIX_COLS * MATRIX_ROWS;

#define MASSDROP_VID 0x04D8
#if VENDOR_ID == MASSDROP_VID
#    define PRODUCT_STRING PRODUCT
#    define MANUFACTURER_STRING MANUFACTURER
#else
#    define PRODUCT_STRING STR(PRODUCT)
#    define MANUFACTURER_STRING STR(MANUFACTURER)
#endif

    uint8_t current_byte = 3;
    for (uint8_t i = 0; (current_byte < ((RAW_EPSIZE - 2) / 2)) && (PRODUCT_STRING[i] != 0); i++) {
        raw_hid_buffer[current_byte] = PRODUCT_STRING[i];
        current_byte++;
    }
    raw_hid_buffer[current_byte] = 0;
    current_byte++;

    for (uint8_t i = 0; (current_byte + 2 < RAW_EPSIZE) && (MANUFACTURER_STRING[i] != 0); i++) {
        raw_hid_buffer[current_byte] = MANUFACTURER_STRING[i];
        current_byte++;
    }
}

void openrgb_get_mode_info(void) {
    const HSV hsv_color = rgb_matrix_get_hsv();
    const uint8_t fw_mode = rgb_matrix_get_mode();

    raw_hid_buffer[0] = OPENRGB_GET_MODE_INFO;
    raw_hid_buffer[1] = firmware_to_protocol_mode(fw_mode);
    raw_hid_buffer[2] = rgb_matrix_get_speed();
    raw_hid_buffer[3] = hsv_color.h;
    raw_hid_buffer[4] = hsv_color.s;
    raw_hid_buffer[5] = hsv_color.v;
}

void openrgb_get_led_info(uint8_t *data) {
    const uint8_t first_led   = data[1];
    const uint8_t number_leds = data[2];

    raw_hid_buffer[0] = OPENRGB_GET_LED_INFO;

    for (uint8_t i = 0; i < number_leds; i++) {
        const uint8_t led_idx = first_led + i;
        const uint8_t data_idx  = i * 7;

        if (led_idx >= RGB_MATRIX_LED_COUNT) {
            raw_hid_buffer[data_idx + 3] = OPENRGB_FAILURE;
        } else {
            raw_hid_buffer[data_idx + 1] = g_led_config.point[led_idx].x;
            raw_hid_buffer[data_idx + 2] = g_led_config.point[led_idx].y;
            raw_hid_buffer[data_idx + 3] = g_led_config.flags[led_idx];
            raw_hid_buffer[data_idx + 4] = g_openrgb_direct_mode_colors[led_idx].r;
            raw_hid_buffer[data_idx + 5] = g_openrgb_direct_mode_colors[led_idx].g;
            raw_hid_buffer[data_idx + 6] = g_openrgb_direct_mode_colors[led_idx].b;
        }

        uint8_t row   = 0;
        uint8_t col   = 0;
        uint8_t found = 0;

        for (row = 0; row < MATRIX_ROWS; row++) {
            for (col = 0; col < MATRIX_COLS; col++) {
                if (g_led_config.matrix_co[row][col] == led_idx) {
                    found = 1;
                    break;
                }
            }

            if (found == 1) {
                break;
            }
        }

        if (col >= MATRIX_COLS || row >= MATRIX_ROWS) {
            raw_hid_buffer[data_idx + 7] = KC_NO;
        }
        else {
            raw_hid_buffer[data_idx + 7] = (u_int8_t) keycode_at_keymap_location(0, row, col);
        }
    }
}

void openrgb_get_enabled_modes(void) {
    raw_hid_buffer[0] = OPENRGB_GET_ENABLED_MODES;
    for (uint8_t i = 0; i < OPENRGB_MODE_MAP_SIZE; i++) {
        raw_hid_buffer[i + 1] = openrgb_mode_map[i].protocol_id;
    }
}

void openrgb_set_mode(uint8_t *data) {
    const uint8_t h     = data[1];
    const uint8_t s     = data[2];
    const uint8_t v     = data[3];
    const uint8_t mode  = data[4];
    const uint8_t speed = data[5];
    const uint8_t save  = data[6];

    raw_hid_buffer[0] = OPENRGB_SET_MODE;

    /* Translate host counter value to firmware enum value */
    const uint8_t fw_mode = protocol_to_firmware_mode(mode);

    if (h > 255 || s > 255 || v > 255 || fw_mode == 0 || fw_mode >= RGB_MATRIX_EFFECT_MAX || speed > 255) {
        raw_hid_buffer[RAW_EPSIZE - 2] = OPENRGB_FAILURE;
        return;
    }

    if (save == 1) {
        rgb_matrix_mode(fw_mode);
        rgb_matrix_set_speed(speed);
        rgb_matrix_sethsv(h, s, v);
    }
    else {
        rgb_matrix_mode_noeeprom(fw_mode);
        rgb_matrix_set_speed_noeeprom(speed);
        rgb_matrix_sethsv_noeeprom(h, s, v);
    }

    raw_hid_buffer[RAW_EPSIZE - 2] = OPENRGB_SUCCESS;
}
void openrgb_direct_mode_set_single_led(uint8_t *data) {
    const uint8_t led = data[1];
    const uint8_t r   = data[2];
    const uint8_t g   = data[3];
    const uint8_t b   = data[4];

    raw_hid_buffer[0] = OPENRGB_DIRECT_MODE_SET_SINGLE_LED;

    if (led >= RGB_MATRIX_LED_COUNT || r > 255 || g > 255 || b > 255) {
        raw_hid_buffer[RAW_EPSIZE - 2] = OPENRGB_FAILURE;
        return;
    }

    g_openrgb_direct_mode_colors[led].r = r;
    g_openrgb_direct_mode_colors[led].g = g;
    g_openrgb_direct_mode_colors[led].b = b;

    raw_hid_buffer[RAW_EPSIZE - 2] = OPENRGB_SUCCESS;
}
void openrgb_direct_mode_set_leds(uint8_t *data) {
    const uint8_t number_leds = data[1];

    for (uint8_t i = 0; i < number_leds; i++) {
        const uint8_t data_idx  = i * 4;
        const uint8_t color_idx = data[data_idx + 2];

        g_openrgb_direct_mode_colors[color_idx].r = data[data_idx + 3];
        g_openrgb_direct_mode_colors[color_idx].g = data[data_idx + 4];
        g_openrgb_direct_mode_colors[color_idx].b = data[data_idx + 5];
    }
}
