// Copyright 2023 Dasky (@daskygit)
// Copyright 2023 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "azoteq_iqs5xx.h"
#include "pointing_device_internal.h"
#include "timer.h" // build D idle reseed: timer_read32 / timer_elapsed32
#include "wait.h"
#include "debug.h"
#ifdef POINTING_DEVICE_ENABLE
#    include "report.h"
#endif
#ifdef DIGITIZER_ENABLE
#    include "digitizer.h"
#endif

#ifndef AZOTEQ_IQS5XX_ADDRESS
#    define AZOTEQ_IQS5XX_ADDRESS (0x74 << 1)
#endif
#ifndef AZOTEQ_IQS5XX_TIMEOUT_MS
#    define AZOTEQ_IQS5XX_TIMEOUT_MS 10
#endif

#define AZOTEQ_IQS5XX_REG_PRODUCT_NUMBER 0x0000
#define AZOTEQ_IQS5XX_REG_PREVIOUS_CYCLE_TIME 0x000C
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_X_POSITION 0x0016
#define AZOTEQ_IQS5XX_REG_SYSTEM_CONTROL_0 0x0431
#define AZOTEQ_IQS5XX_REG_SYSTEM_CONTROL_1 0x0432
#define AZOTEQ_IQS5XX_REG_REPORT_RATE_ACTIVE 0x057A
#define AZOTEQ_IQS5XX_REG_IDLE_MODE_TIMEOUT 0x0586
#define AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_0 0x058E
#define AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_1 0x058F
#define AZOTEQ_IQS5XX_REG_X_RESOLUTION 0x066E
#define AZOTEQ_IQS5XX_REG_XY_CONFIG_0 0x0669
#define AZOTEQ_IQS5XX_REG_Y_RESOLUTION 0x0670
#define AZOTEQ_IQS5XX_REG_SINGLE_FINGER_GESTURES 0x06B7
#define AZOTEQ_IQS5XX_REG_END_COMMS 0xEEEE
/* Per-channel status bitmaps, read only by the phantom probe. Contiguous with
 * the finger data (0x0016 + 35 = 0x0039), so they extend the existing read
 * rather than costing a second transaction. Datasheet 8.10.5. */
#define AZOTEQ_IQS5XX_REG_PROX_STATUS 0x0039  // 32 bytes: 15 Tx rows + ALP
#define AZOTEQ_IQS5XX_REG_TOUCH_STATUS 0x0059 // 30 bytes: 15 Tx rows

/* Touch/snap output debounce. Datasheet 3.5.4 and the 8.10 memory map bit row:
 *   bit7..0 = SNAP_DB_SET[7:6] TOUCH_DB_SET[5:4] SNAP_DB_CLEAR[3:2] TOUCH_DB_CLEAR[1:0]
 * A value of 1 means two consecutive samples must satisfy the condition before
 * the channel's output is asserted. Azoteq ships this at 0 because on a 15x10
 * sensor debouncing costs too much latency -- but 70-89% of every phantom
 * measured lasts exactly one sensing cycle, which is precisely what
 * TOUCH_DB_SET=1 rejects. Read back via PHCFG `touchdb=` before believing it
 * took: the two-bit field split is read off the datasheet's bit-name row rather
 * than stated in prose, and if it is wrong then 0x10 lands in SNAP_DB_CLEAR,
 * which is harmless here because snap is not enabled on this panel. */
#define AZOTEQ_IQS5XX_REG_TOUCH_SNAP_DEBOUNCE 0x067A

// Gesture configuration
#ifndef AZOTEQ_IQS5XX_TAP_ENABLE
#    define AZOTEQ_IQS5XX_TAP_ENABLE true
#endif
#ifndef AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE
#    define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE false
#endif
#ifndef AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE
#    define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE true
#endif
#ifndef AZOTEQ_IQS5XX_SCROLL_ENABLE
#    define AZOTEQ_IQS5XX_SCROLL_ENABLE true
#endif
#ifndef AZOTEQ_IQS5XX_SWIPE_X_ENABLE
#    define AZOTEQ_IQS5XX_SWIPE_X_ENABLE false
#endif
#ifndef AZOTEQ_IQS5XX_SWIPE_Y_ENABLE
#    define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE false
#endif
#ifndef AZOTEQ_IQS5XX_ZOOM_ENABLE
#    define AZOTEQ_IQS5XX_ZOOM_ENABLE false
#endif
#ifndef AZOTEQ_IQS5XX_TAP_TIME
#    define AZOTEQ_IQS5XX_TAP_TIME 0x96
#endif
#ifndef AZOTEQ_IQS5XX_TAP_DISTANCE
#    define AZOTEQ_IQS5XX_TAP_DISTANCE 0x19
#endif
#ifndef AZOTEQ_IQS5XX_HOLD_TIME
#    define AZOTEQ_IQS5XX_HOLD_TIME 0x12C
#endif
#ifndef AZOTEQ_IQS5XX_SWIPE_INITIAL_TIME
#    define AZOTEQ_IQS5XX_SWIPE_INITIAL_TIME 0x64 // 0x96
#endif
#ifndef AZOTEQ_IQS5XX_SWIPE_INITIAL_DISTANCE
#    define AZOTEQ_IQS5XX_SWIPE_INITIAL_DISTANCE 0x12C
#endif
#ifndef AZOTEQ_IQS5XX_SWIPE_CONSECUTIVE_TIME
#    define AZOTEQ_IQS5XX_SWIPE_CONSECUTIVE_TIME 0x0
#endif
#ifndef AZOTEQ_IQS5XX_SWIPE_CONSECUTIVE_DISTANCE
#    define AZOTEQ_IQS5XX_SWIPE_CONSECUTIVE_DISTANCE 0x7D0
#endif
#ifndef AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE
#    define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 0x32
#endif
#ifndef AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE
#    define AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE 0x32
#endif
#ifndef AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE
#    define AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE 0x19
#endif
// Minimum finger strength to register as a real touch.
// Increase in config.h (e.g. 50) to filter EMI/capacitive noise phantom contacts.
// Default 0 preserves original behaviour for boards that don't override.
#ifndef AZOTEQ_IQS5XX_MIN_STRENGTH
#    define AZOTEQ_IQS5XX_MIN_STRENGTH 0
#endif
#ifndef AZOTEQ_IQS5XX_EVENT_MODE
// Event mode can't be used until the pointing code has changed (stuck buttons)
#    define AZOTEQ_IQS5XX_EVENT_MODE false
#endif

#define DIVIDE_UNSIGNED_ROUND(numerator, denominator) (((numerator) + ((denominator) / 2)) / (denominator))
#define AZOTEQ_IQS5XX_INCH_TO_RESOLUTION_X(inch) (DIVIDE_UNSIGNED_ROUND((inch) * (uint32_t)AZOTEQ_IQS5XX_WIDTH_MM * 10, 254))
#define AZOTEQ_IQS5XX_RESOLUTION_X_TO_INCH(px) (DIVIDE_UNSIGNED_ROUND((px) * (uint32_t)254, AZOTEQ_IQS5XX_WIDTH_MM * 10))
#define AZOTEQ_IQS5XX_INCH_TO_RESOLUTION_Y(inch) (DIVIDE_UNSIGNED_ROUND((inch) * (uint32_t)AZOTEQ_IQS5XX_HEIGHT_MM * 10, 254))
#define AZOTEQ_IQS5XX_RESOLUTION_Y_TO_INCH(px) (DIVIDE_UNSIGNED_ROUND((px) * (uint32_t)254, AZOTEQ_IQS5XX_HEIGHT_MM * 10))

#ifdef POINTING_DEVICE_DRIVER_azoteq_iqs5xx
const pointing_device_driver_t azoteq_iqs5xx_pointing_device_driver = {
    .init       = azoteq_iqs5xx_init,
    .get_report = azoteq_iqs5xx_get_report,
    .set_cpi    = azoteq_iqs5xx_set_cpi,
    .get_cpi    = azoteq_iqs5xx_get_cpi,
};
#endif

static uint16_t azoteq_iqs5xx_product_number = AZOTEQ_IQS5XX_UNKNOWN;

static struct {
    uint16_t resolution_x;
    uint16_t resolution_y;
} azoteq_iqs5xx_device_resolution_t;

i2c_status_t azoteq_iqs5xx_wake(void) {
    return i2c_ping_address(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_TIMEOUT_MS);
}

i2c_status_t azoteq_iqs5xx_end_session(void) {
    const uint8_t END_BYTE = 1; // any data
    return i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_END_COMMS, &END_BYTE, 1, AZOTEQ_IQS5XX_TIMEOUT_MS);
}

i2c_status_t azoteq_iqs5xx_get_base_data(azoteq_iqs5xx_base_data_t *base_data) {
    i2c_status_t status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_PREVIOUS_CYCLE_TIME, (uint8_t *)base_data, 10, AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_get_report_rate(azoteq_iqs5xx_report_rate_t *report_rate, azoteq_iqs5xx_charging_modes_t mode, bool end_session) {
    if (mode > AZOTEQ_IQS5XX_LP2) {
        pd_dprintf("IQS5XX - Invalid mode for get report rate.\n");
        return I2C_STATUS_ERROR;
    }
    uint16_t     selected_reg = AZOTEQ_IQS5XX_REG_REPORT_RATE_ACTIVE + (2 * mode);
    i2c_status_t status       = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, selected_reg, (uint8_t *)report_rate, 2, AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_set_report_rate(uint16_t report_rate_ms, azoteq_iqs5xx_charging_modes_t mode, bool end_session) {
    if (mode > AZOTEQ_IQS5XX_LP2) {
        pd_dprintf("IQS5XX - Invalid mode for set report rate.\n");
        return I2C_STATUS_ERROR;
    }
    uint16_t                    selected_reg = AZOTEQ_IQS5XX_REG_REPORT_RATE_ACTIVE + (2 * mode);
    azoteq_iqs5xx_report_rate_t report_rate  = {0};
    report_rate.h                            = (uint8_t)((report_rate_ms >> 8) & 0xFF);
    report_rate.l                            = (uint8_t)(report_rate_ms & 0xFF);
    i2c_status_t status                      = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, selected_reg, (uint8_t *)&report_rate, 2, AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_set_reati(bool enabled, bool end_session) {
    azoteq_iqs5xx_system_config_0_t config = {0};
    i2c_status_t                    status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_0, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_config_0_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        config.reati = enabled;
        status       = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_0, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_config_0_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    }
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_set_event_mode(bool enabled, bool end_session) {
    azoteq_iqs5xx_system_config_1_t config = {0};
    i2c_status_t                    status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_1, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_config_1_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        config.event_mode     = enabled;
        config.touch_event    = true;
        config.tp_event       = true;
        config.prox_event     = false;
        config.snap_event     = false;
        config.reati_event    = false;
        config.alp_prox_event = false;
        config.gesture_event  = true;
        status                = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_1, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_config_1_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    }
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_set_gesture_config(bool end_session) {
    azoteq_iqs5xx_gesture_config_t config = {0};
    i2c_status_t                   status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SINGLE_FINGER_GESTURES, (uint8_t *)&config, sizeof(azoteq_iqs5xx_gesture_config_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    pd_dprintf("azo scroll: %d\n", config.multi_finger_gestures.scroll);
    if (status == I2C_STATUS_SUCCESS) {
        config.single_finger_gestures.single_tap     = AZOTEQ_IQS5XX_TAP_ENABLE;
        config.single_finger_gestures.press_and_hold = AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE;
        config.single_finger_gestures.swipe_x_plus   = AZOTEQ_IQS5XX_SWIPE_X_ENABLE;
        config.single_finger_gestures.swipe_x_minus  = AZOTEQ_IQS5XX_SWIPE_X_ENABLE;
        config.single_finger_gestures.swipe_y_plus   = AZOTEQ_IQS5XX_SWIPE_Y_ENABLE;
        config.single_finger_gestures.swipe_y_minus  = AZOTEQ_IQS5XX_SWIPE_Y_ENABLE;
        config.multi_finger_gestures.two_finger_tap  = AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE;
        config.multi_finger_gestures.scroll          = AZOTEQ_IQS5XX_SCROLL_ENABLE;
        config.multi_finger_gestures.zoom            = AZOTEQ_IQS5XX_ZOOM_ENABLE;
        config.tap_time                              = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_TAP_TIME);
        config.tap_distance                          = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_TAP_DISTANCE);
        config.hold_time                             = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_HOLD_TIME);
        config.swipe_initial_time                    = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_SWIPE_INITIAL_TIME);
        config.swipe_initial_distance                = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_SWIPE_INITIAL_DISTANCE);
        config.swipe_consecutive_time                = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_SWIPE_CONSECUTIVE_TIME);
        config.swipe_consecutive_distance            = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_SWIPE_CONSECUTIVE_DISTANCE);
        config.scroll_initial_distance               = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE);
        config.zoom_initial_distance                 = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE);
        config.zoom_consecutive_distance             = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE);
        status                                       = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SINGLE_FINGER_GESTURES, (uint8_t *)&config, sizeof(azoteq_iqs5xx_gesture_config_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    }
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_set_xy_config(bool flip_x, bool flip_y, bool switch_xy, bool palm_reject, bool end_session) {
    azoteq_iqs5xx_xy_config_0_t config = {0};
    i2c_status_t                status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_XY_CONFIG_0, (uint8_t *)&config, sizeof(azoteq_iqs5xx_xy_config_0_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        if (flip_x) {
            config.flip_x = !config.flip_x;
        }
        if (flip_y) {
            config.flip_y = !config.flip_y;
        }
        if (switch_xy) {
            config.switch_xy_axis = !config.switch_xy_axis;
        }
        config.palm_reject = palm_reject;
        status             = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_XY_CONFIG_0, (uint8_t *)&config, sizeof(azoteq_iqs5xx_xy_config_0_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    }
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

i2c_status_t azoteq_iqs5xx_reset_suspend(bool reset, bool suspend, bool end_session) {
    azoteq_iqs5xx_system_control_1_t config = {0};
    i2c_status_t                     status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONTROL_1, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_control_1_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        config.reset   = reset;
        config.suspend = suspend;
        status         = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONTROL_1, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_control_1_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    }
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

/* Acknowledge a device reset -- System Control 0 bit 7, ACK_RESET.
 *
 * IQS5xx-B000 Trackpad Datasheet rev 2.1, section 7.4.1: "After a reset, the
 * SHOW_RESET bit will be set by the system to indicate the reset event occurred.
 * This bit will clear when the master sets the ACK_RESET, if it becomes set
 * again, the master will know a reset has occurred, and can react appropriately."
 *
 * Nothing acknowledged it before, so SHOW_RESET read 1 from the deliberate reset
 * in init() onwards and spontaneous resets were undetectable. Section 7.5
 * documents a ~500 ms watchdog whose stated purpose is recovering from stuck
 * conditions "which could occur from ESD events or similar", so such a reset is
 * an expected event on a hand-wired board rather than a hypothetical one.
 *
 * A plain write, not a read-modify-write: every bit in this register is
 * write-1-to-trigger, so the zeros carry no meaning. This matches the Linux
 * kernel driver (drivers/input/touchscreen/iqs5xx.c), which writes
 * IQS5XX_ACK_RESET to IQS5XX_SYS_CTRL0 as a whole byte. */
i2c_status_t azoteq_iqs5xx_ack_reset(bool end_session) {
    const azoteq_iqs5xx_system_control_0_t control = {.ack_reset = true};

    i2c_status_t status = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONTROL_0, (const uint8_t *)&control, sizeof(azoteq_iqs5xx_system_control_0_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

/* RESEED: re-store every trackpad channel's reference from its current counts.
 * Same write-1-to-trigger register as ACK_RESET, same plain-write rule. See the
 * header for why the host has to do this at all -- the chip only refreshes
 * references from LP1/LP2, and this board stops visiting LP1. */
i2c_status_t azoteq_iqs5xx_reseed(bool end_session) {
    const azoteq_iqs5xx_system_control_0_t control = {.reseed = true};

    i2c_status_t status = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_SYSTEM_CONTROL_0, (const uint8_t *)&control, sizeof(azoteq_iqs5xx_system_control_0_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (end_session) {
        azoteq_iqs5xx_end_session();
    }
    return status;
}

void azoteq_iqs5xx_set_cpi(uint16_t cpi) {
    if (azoteq_iqs5xx_product_number != AZOTEQ_IQS5XX_UNKNOWN) {
        azoteq_iqs5xx_resolution_t resolution = {0};
        resolution.x_resolution               = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(MIN(azoteq_iqs5xx_device_resolution_t.resolution_x, AZOTEQ_IQS5XX_INCH_TO_RESOLUTION_X(cpi)));
        resolution.y_resolution               = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(MIN(azoteq_iqs5xx_device_resolution_t.resolution_y, AZOTEQ_IQS5XX_INCH_TO_RESOLUTION_Y(cpi)));
        i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_X_RESOLUTION, (uint8_t *)&resolution, sizeof(azoteq_iqs5xx_resolution_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    }
}

uint16_t azoteq_iqs5xx_get_cpi(void) {
    if (azoteq_iqs5xx_product_number != AZOTEQ_IQS5XX_UNKNOWN) {
        azoteq_iqs5xx_resolution_t resolution = {0};
        i2c_status_t               status     = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_X_RESOLUTION, (uint8_t *)&resolution, sizeof(azoteq_iqs5xx_resolution_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
        if (status == I2C_STATUS_SUCCESS) {
            return AZOTEQ_IQS5XX_RESOLUTION_X_TO_INCH(AZOTEQ_IQS5XX_SWAP_H_L_BYTES(resolution.x_resolution));
        }
    }
    return 0;
}

uint16_t azoteq_iqs5xx_get_product(void) {
    i2c_status_t status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_PRODUCT_NUMBER, (uint8_t *)&azoteq_iqs5xx_product_number, sizeof(uint16_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        azoteq_iqs5xx_product_number = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(azoteq_iqs5xx_product_number);
    }
    pd_dprintf("AZOTEQ: Product number %u\n", azoteq_iqs5xx_product_number);
    return azoteq_iqs5xx_product_number;
}

void azoteq_iqs5xx_setup_resolution(void) {
#if !defined(AZOTEQ_IQS5XX_RESOLUTION_X) && !defined(AZOTEQ_IQS5XX_RESOLUTION_Y)
    switch (azoteq_iqs5xx_product_number) {
        case AZOTEQ_IQS550:
            azoteq_iqs5xx_device_resolution_t.resolution_x = 3584;
            azoteq_iqs5xx_device_resolution_t.resolution_y = 2304;
            break;
        case AZOTEQ_IQS572:
            azoteq_iqs5xx_device_resolution_t.resolution_x = 2048;
            azoteq_iqs5xx_device_resolution_t.resolution_y = 1792;
            break;
        case AZOTEQ_IQS525:
            azoteq_iqs5xx_device_resolution_t.resolution_x = 1280;
            azoteq_iqs5xx_device_resolution_t.resolution_y = 768;
            break;
        default:
            // shouldn't be here
            azoteq_iqs5xx_device_resolution_t.resolution_x = 0;
            azoteq_iqs5xx_device_resolution_t.resolution_y = 0;
            break;
    }
#endif
#ifdef AZOTEQ_IQS5XX_RESOLUTION_X
    azoteq_iqs5xx_device_resolution_t.resolution_x = AZOTEQ_IQS5XX_RESOLUTION_X;
#endif
#ifdef AZOTEQ_IQS5XX_RESOLUTION_Y
    azoteq_iqs5xx_device_resolution_t.resolution_y = AZOTEQ_IQS5XX_RESOLUTION_Y;
#endif
}

static i2c_status_t azoteq_iqs5xx_init_status = 1;

bool azoteq_iqs5xx_init(void) {
    i2c_init();
    i2c_ping_address(AZOTEQ_IQS5XX_ADDRESS, 1); // wake
    azoteq_iqs5xx_reset_suspend(true, false, true);
    wait_ms(100);
    i2c_ping_address(AZOTEQ_IQS5XX_ADDRESS, 1); // wake
    if (azoteq_iqs5xx_get_product() != AZOTEQ_IQS5XX_UNKNOWN) {
        azoteq_iqs5xx_setup_resolution();
#ifdef DIGITIZER_ENABLE
        {
            azoteq_iqs5xx_resolution_t resolution = {0};
#if defined(AZOTEQ_IQS5XX_ROTATION_90) || defined(AZOTEQ_IQS5XX_ROTATION_270)
            // switch_xy is applied after these registers are written, so swap values to match
            // the output axes: output X = physical Y axis (resolution_y), output Y = physical X axis (resolution_x).
            // This prevents output X coordinates from exceeding the HID logical max after axis swap.
            resolution.x_resolution = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(azoteq_iqs5xx_device_resolution_t.resolution_y);
            resolution.y_resolution = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(azoteq_iqs5xx_device_resolution_t.resolution_x);
#else
            resolution.x_resolution = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(azoteq_iqs5xx_device_resolution_t.resolution_x);
            resolution.y_resolution = AZOTEQ_IQS5XX_SWAP_H_L_BYTES(azoteq_iqs5xx_device_resolution_t.resolution_y);
#endif
            azoteq_iqs5xx_init_status = i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_X_RESOLUTION, (uint8_t *)&resolution, sizeof(azoteq_iqs5xx_resolution_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
        }
#endif
        azoteq_iqs5xx_init_status = azoteq_iqs5xx_set_report_rate(AZOTEQ_IQS5XX_REPORT_RATE, AZOTEQ_IQS5XX_ACTIVE, false);
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_report_rate(AZOTEQ_IQS5XX_REPORT_RATE, AZOTEQ_IQS5XX_IDLE, false);
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_report_rate(AZOTEQ_IQS5XX_REPORT_RATE, AZOTEQ_IQS5XX_IDLE_TOUCH, false);

        // 255 = "never" (datasheet p.20), i.e. upstream's "don't enter LP1, LP2 states".
        // A keymap can set a finite number of seconds instead; see the argument in the header.
        uint8_t idle_timeout = AZOTEQ_IQS5XX_IDLE_TIMEOUT_S;
        azoteq_iqs5xx_init_status |= i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_IDLE_MODE_TIMEOUT, &idle_timeout, 1, AZOTEQ_IQS5XX_TIMEOUT_MS);
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_event_mode(AZOTEQ_IQS5XX_EVENT_MODE, false);
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_reati(true, false);
#if defined(AZOTEQ_IQS5XX_ROTATION_90)
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(false, true, true, true, false);
#elif defined(AZOTEQ_IQS5XX_ROTATION_180)
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(true, true, false, true, false);
#elif defined(AZOTEQ_IQS5XX_ROTATION_270)
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(true, false, true, true, false);
#else
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(false, false, false, true, false);
#endif
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_gesture_config(false);
#ifdef AZOTEQ_IQS5XX_TOUCH_DEBOUNCE
        /* Chip-side rejection of single-cycle touches. Written inside the comms
         * window opened above and before ack_reset() closes it. Not persisted to
         * NVM, so a power cycle reverts it -- deliberate, this is an experiment. */
        {
            uint8_t debounce = (uint8_t)AZOTEQ_IQS5XX_TOUCH_DEBOUNCE;
            azoteq_iqs5xx_init_status |= i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_TOUCH_SNAP_DEBOUNCE, &debounce, 1, AZOTEQ_IQS5XX_TIMEOUT_MS);
        }
#endif
        /* Clear SHOW_RESET, latched by the deliberate reset_suspend() above.
         * Do this last, after every configuration write, so the flag reads 0
         * only once the chip is fully set up -- from here on a set SHOW_RESET
         * means a spontaneous reset and a chip that has silently reverted to
         * its NVM defaults. Closes the comms window in place of the gesture
         * config write above. */
        azoteq_iqs5xx_init_status |= azoteq_iqs5xx_ack_reset(true);
        wait_ms(AZOTEQ_IQS5XX_REPORT_RATE + 1);
    }

    return azoteq_iqs5xx_init_status == I2C_STATUS_SUCCESS;
};

#ifdef POINTING_DEVICE_ENABLE
report_mouse_t azoteq_iqs5xx_get_report(report_mouse_t mouse_report) {
    report_mouse_t temp_report = {0};

    azoteq_iqs5xx_base_data_t base_data       = {0};
    i2c_status_t              status          = azoteq_iqs5xx_get_base_data(&base_data);
    bool                      ignore_movement = false;

    if (status == I2C_STATUS_SUCCESS) {
#ifdef POINTING_DEVICE_DEBUG
        if (base_data.previous_cycle_time > AZOTEQ_IQS5XX_REPORT_RATE) {
            pd_dprintf("IQS5XX - previous cycle time missed, took: %dms\n", base_data.previous_cycle_time);
        }
#endif
        if (base_data.gesture_events_0.single_tap || base_data.gesture_events_0.press_and_hold) {
            pd_dprintf("IQS5XX - Single tap/hold.\n");
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON1);
        } else if (base_data.gesture_events_1.two_finger_tap) {
            pd_dprintf("IQS5XX - Two finger tap.\n");
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON2);
        } else if (base_data.gesture_events_0.swipe_x_neg) {
            pd_dprintf("IQS5XX - X-.\n");
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON4);
            ignore_movement     = true;
        } else if (base_data.gesture_events_0.swipe_x_pos) {
            pd_dprintf("IQS5XX - X+.\n");
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON5);
            ignore_movement     = true;
        } else if (base_data.gesture_events_0.swipe_y_neg) {
            pd_dprintf("IQS5XX - Y-.\n");
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON6);
            ignore_movement     = true;
        } else if (base_data.gesture_events_0.swipe_y_pos) {
            pd_dprintf("IQS5XX - Y+.\n");
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON3);
            ignore_movement     = true;
        } else if (base_data.gesture_events_1.zoom) {
            if (AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.x.h, base_data.x.l) < 0) {
                pd_dprintf("IQS5XX - Zoom out.\n");
                temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON7);
            } else if (AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.x.h, base_data.x.l) > 0) {
                pd_dprintf("IQS5XX - Zoom in.\n");
                temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON8);
            }
        } else if (base_data.gesture_events_1.scroll) {
            pd_dprintf("IQS5XX - Scroll.\n");
            temp_report.h = CONSTRAIN_HID(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.x.h, base_data.x.l));
            temp_report.v = CONSTRAIN_HID(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.y.h, base_data.y.l));
        }
        if (base_data.number_of_fingers == 1 && !ignore_movement) {
            temp_report.x = CONSTRAIN_HID_XY(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.x.h, base_data.x.l));
            temp_report.y = CONSTRAIN_HID_XY(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.y.h, base_data.y.l));
        }

    } else {
        pd_dprintf("IQS5XX - get report failed, i2c status: %d \n", status);
    }

    return temp_report;
}
#endif

#ifdef DIGITIZER_ENABLE

// Combined struct: base header (gesture events + relative delta) immediately followed by
// absolute finger data. Registers are contiguous: 0x000C (base, 10 bytes) + 0x0016 (fingers, 35 bytes).
typedef struct PACKED {
    azoteq_iqs5xx_base_data_t      base;    // 10 bytes @ 0x000C
    azoteq_iqs5xx_digitizer_data_t fingers; // 35 bytes @ 0x0016
#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
    /* Probe builds only: 62 more bytes on the same transaction, taking the read
     * from 45 to 107 bytes (~1.1ms at 1MHz, against a 9ms sensing cycle). These
     * name the exact channel behind a contact instead of inferring it from x/y. */
    uint8_t prox_status[32];  // @ 0x0039
    uint8_t touch_status[30]; // @ 0x0059
#endif
} azoteq_iqs5xx_combined_digitizer_data_t;

// Last-read gesture events, updated every digitizer_driver_get_report() call.
// Consumed by digitizer_mouse_fallback via azoteq_iqs5xx_get_digitizer_gesture_events().
static azoteq_iqs5xx_gesture_events_0_t digitizer_gesture_ev0     = {0};
static azoteq_iqs5xx_gesture_events_1_t digitizer_gesture_ev1     = {0};
static int16_t                          digitizer_gesture_x_delta = 0; // zoom: >0 = in, <0 = out

// Diagnostic snapshot of the last read. Populated from data the combined read
// already pulls in; read via azoteq_iqs5xx_get_digitizer_diag(). No behaviour
// depends on this -- it exists so a probe can see what the driver is ignoring.
static azoteq_iqs5xx_digitizer_diag_t digitizer_diag = {0};

#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
/* One-shot block reads on behalf of the probe. See the header for why the probe
 * cannot issue these itself: it runs after end_session() and the chip NACKs
 * outside its comms window. The read below happens inside the window, right
 * before it is closed, at most once per frame and only when asked. */
static uint16_t probe_block_req_reg = 0;
static uint16_t probe_block_req_len = 0; // nonzero = a read is queued
static uint16_t probe_block_got_len = 0; // nonzero = a result is waiting
static uint8_t  probe_block_buf[AZOTEQ_IQS5XX_PROBE_BLOCK_MAX];

void azoteq_iqs5xx_probe_request_block(uint16_t reg, uint16_t len) {
    if (probe_block_req_len || probe_block_got_len) return; // one in flight
    if (len == 0 || len > sizeof(probe_block_buf)) return;
    probe_block_req_reg = reg;
    probe_block_req_len = len;
}

uint16_t azoteq_iqs5xx_probe_take_block(const uint8_t **out) {
    if (!probe_block_got_len) return 0;
    if (out) *out = probe_block_buf;
    uint16_t len        = probe_block_got_len;
    probe_block_got_len = 0;
    return len;
}
#endif

#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
/* Implemented by the keymap (tps65-510h/phantom_probe.c). Called once per
 * successful read. Deliberately NOT routed through digitizer_task_kb(): that
 * hook is declared __attribute__((weak)) in quantum/digitizer.h, so a keymap
 * override is itself emitted weak and which of the two weak definitions the
 * linker keeps is link-order dependent. A direct call is deterministic.
 * Compiled out entirely unless the keymap defines AZOTEQ_IQS5XX_PHANTOM_PROBE. */
extern void phantom_probe_sample(const digitizer_t *report);
#endif

// Returns the diagnostic snapshot from the last digitizer_driver_get_report() call.
// Only meaningful on the half that owns the sensor; on the other half of a split
// board this stays zeroed because no I2C read ever happens there.
void azoteq_iqs5xx_get_digitizer_diag(azoteq_iqs5xx_digitizer_diag_t *out) {
    if (out) *out = digitizer_diag;
}

// Returns hardware gesture events captured during the last successful I2C read.
// x_delta: signed relative X from base_data — for zoom, positive = spreading (in), negative = pinching (out).
void azoteq_iqs5xx_get_digitizer_gesture_events(azoteq_iqs5xx_gesture_events_0_t *ev0, azoteq_iqs5xx_gesture_events_1_t *ev1, int16_t *x_delta) {
    if (ev0)     *ev0     = digitizer_gesture_ev0;
    if (ev1)     *ev1     = digitizer_gesture_ev1;
    if (x_delta) *x_delta = digitizer_gesture_x_delta;
}

#ifdef AZOTEQ_IQS5XX_CONFIDENCE_GATE
/* Latched per slot: set once a contact has ever shown a real finger's footprint.
 * Cleared one frame AFTER the contact is fully released -- see the release-edge
 * note in the gate below; contact_was_down[] is what makes that edge visible. */
static bool contact_confident[5] = {0};
static bool contact_was_down[5]  = {0};
#endif

digitizer_t digitizer_driver_get_report(digitizer_t digitizer_report) {
    azoteq_iqs5xx_combined_digitizer_data_t combined = {0};
    azoteq_iqs5xx_wake();

    // Single I2C read: gesture header (0x000C, 10 bytes) + absolute finger data (0x0016, 35 bytes),
    // plus, in phantom-probe builds only, the prox and touch channel bitmaps (0x0039, 62 bytes).
    i2c_status_t status = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, AZOTEQ_IQS5XX_REG_PREVIOUS_CYCLE_TIME, (uint8_t *)&combined, sizeof(azoteq_iqs5xx_combined_digitizer_data_t), AZOTEQ_IQS5XX_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        // Store gesture events for digitizer_mouse_fallback to consume.
        digitizer_gesture_ev0     = combined.base.gesture_events_0;
        digitizer_gesture_ev1     = combined.base.gesture_events_1;
        digitizer_gesture_x_delta = AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(combined.base.x.h, combined.base.x.l);

        // Diagnostics only -- none of this feeds the report.
        digitizer_diag.read_ok             = true;
        digitizer_diag.previous_cycle_time = combined.base.previous_cycle_time;
        digitizer_diag.number_of_fingers   = combined.base.number_of_fingers;
        digitizer_diag.system_info_0       = combined.base.system_info_0;
        digitizer_diag.system_info_1       = combined.base.system_info_1;

        /* init() cleared the boot latch, so a set SHOW_RESET here is a
         * SPONTANEOUS reset: watchdog, brown-out, or a glitch on NRST.
         * Acknowledge it immediately -- still inside the comms window -- so the
         * flag re-arms and the reset after this one is counted too.
         *
         * This is not only bookkeeping. A reset returns the chip to its NVM
         * defaults, discarding the resolution, report rates, axis flips, palm
         * reject and gesture configuration that init() wrote. Nothing re-applies
         * them, so the chip keeps reporting -- with the wrong scaling, the wrong
         * axes, and gestures we deliberately disabled back on. Count first;
         * recovery is a separate change and should not ship until the counter
         * shows this actually happens. */
        if (combined.base.system_info_0.show_reset) {
            digitizer_diag.resets++;
            azoteq_iqs5xx_ack_reset(false);
        }

#ifdef AZOTEQ_IQS5XX_IDLE_RESEED
        /* Build D -- the host half of datasheet 3.4's reference management; the
         * header carries the argument. Still inside the comms window, so it
         * costs one write on the transaction the driver was making anyway.
         *
         * Two conditions, and they are deliberately different:
         *
         *  - The RESEED itself normally issues only on a frame the chip reports
         *    as finger-free. This is a weaker guarantee than it looks: see the
         *    header, an object coupling below the touch threshold still reads
         *    number_of_fingers == 0 and does get baselined in. It is enough to
         *    keep a real (area >= 2) finger out of the reference, which is what
         *    it is here for.
         *  - The timer is held by REAL activity only, meaning a contact the chip
         *    grouped from two or more channels (touch_area >= 2). It is NOT held
         *    by the raw finger count. That looks like the wrong gate and was, in
         *    the first draft, for the reason the header gives -- a contact we hid
         *    is still user activity. The 2026-08-30 storm shows why it cannot be:
         *    a storm asserts number_of_fingers on essentially every frame for as
         *    long as it lasts, so a raw-count gate makes the timer un-expirable
         *    exactly when a reseed is the only thing that would end it.
         *    2138 of the 2148 phantoms captured on this hardware reported area == 1,
         *    the other 10 all inside the last 19 s of a storm, and no real contact
         *    in the baseline has ever sustained a single-channel touch across an
         *    interval, so "no area >= 2 contact for the whole interval"
         *    is a storm or an idle pad, and both want the same treatment.
         *
         * The timer is reset on the reseed as well as on activity, so this fires
         * at most once per interval rather than every frame. */
        static uint32_t idle_since  = 0;
        bool            real_finger = false;
        for (int i = 0; i < 5; i++) {
            if (combined.fingers.fingers[i].touch_area >= 2) {
                real_finger = true;
                break;
            }
        }
        if (real_finger) {
            idle_since = timer_read32();
        } else {
            /* Normal path: wait for a finger-free frame. Escape path: a stuck
             * single-channel contact asserts number_of_fingers forever while
             * never holding the timer (area == 1), so the normal path deadlocks
             * exactly when the reseed is the only thing that would clear it.
             * After AZOTEQ_IQS5XX_IDLE_RESEED_STUCK_MS with no area >= 2 contact,
             * issue anyway -- see the header for why a hand cannot reach this. */
            const uint32_t idle_for = timer_elapsed32(idle_since);
            if (idle_for >= AZOTEQ_IQS5XX_IDLE_RESEED_MS && (combined.base.number_of_fingers == 0 || idle_for >= AZOTEQ_IQS5XX_IDLE_RESEED_STUCK_MS)) {
                if (azoteq_iqs5xx_reseed(false) == I2C_STATUS_SUCCESS) {
                    digitizer_diag.reseeds++;
                }
                idle_since = timer_read32();
            }
        }
#endif

#ifdef AZOTEQ_IQS5XX_CONFIDENCE_GATE
        bool gate_hidden[5] = {0};
#endif
        for (int i = 0; i < 5; i++) {
            uint16_t strength = AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(combined.fingers.fingers[i].strength.h, combined.fingers.fingers[i].strength.l);
            digitizer_diag.strength[i]              = strength;
            digitizer_diag.area[i]                  = combined.fingers.fingers[i].touch_area;
            digitizer_report.contacts[i].x          = AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(combined.fingers.fingers[i].x.h, combined.fingers.fingers[i].x.l);
            digitizer_report.contacts[i].y          = AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(combined.fingers.fingers[i].y.h, combined.fingers.fingers[i].y.l);
            digitizer_report.contacts[i].tip        = strength > AZOTEQ_IQS5XX_MIN_STRENGTH ? 1 : 0;
            digitizer_report.contacts[i].type       = FINGER;
            digitizer_report.contacts[i].confidence = 1;
#ifdef AZOTEQ_IQS5XX_CONFIDENCE_GATE
            /* `touch_area` is the number of sensor channels the chip grouped into
             * this finger (datasheet 5.2.5). Every one of the 1506 phantoms
             * captured on this hardware reported area == 1 -- a single Tx/Rx
             * crossing -- and no real contact in the baseline ever did.
             *
             * That is not a statistical fluke, it is geometry. The TPS65 panel is
             * 13 Tx x 9 Rx over 65 x 49 mm, so the channel pitch is 5.4 x 6.1 mm,
             * and the module datasheet specifies a 7.0 mm minimum finger. A
             * supported finger therefore always covers at least two channels;
             * area == 1 is, by the module's own specification, not a finger.
             * Amplitude cannot separate them: phantoms run 2.8-5.6x the touch
             * threshold, above the peak channel of the lightest real touch.
             *
             * HOW the verdict is delivered was changed on 2026-08-29 after the
             * first build reached hardware. Reporting the contact with
             * confidence = 0 -- on the theory that the host would see it and
             * ignore it -- cost tap-to-click and scroll inertia outright on
             * Windows PTP. Two reasons, both real:
             *   - the release frame. digitizer.c reports a contact once more with
             *     tip = 0 to close it out, and the first version cleared the latch
             *     on exactly that frame, so every contact ended unconfident. The
             *     tip-up frame is the one Windows turns into a click and reads
             *     lift-off velocity from for inertia.
             *   - 23% of real contacts (27 of 115 measured) start at area == 1 and
             *     grow, so the host saw confidence flip 0 -> 1 mid-contact, and a
             *     two-finger gesture saw its second finger arrive late.
             * So the gate now hides rather than flags: an ungated contact is not
             * reported at all, a promoted one is reported normally and fully
             * confident from that frame on, and the host is never asked to
             * interpret a partial contact. Cost is 1-2 frames (9-18 ms) of onset
             * delay on that 23%, against a 148 ms median real contact.
             *
             * This is still a symptom mask over a fault with a physical address
             * -- see azoteq_iqs5xx.h for the datasheet argument. */
            const bool tip_now = digitizer_report.contacts[i].tip;
            if (tip_now) {
                if (combined.fingers.fingers[i].touch_area >= AZOTEQ_IQS5XX_CONFIDENCE_MIN_AREA) {
                    contact_confident[i] = true;
                }
            } else if (!contact_was_down[i]) {
                /* Released for a whole frame: re-arm for the next contact. NOT on
                 * the release frame itself -- digitizer.c reports a contact once
                 * more with tip = 0 to close it out, and that final report has to
                 * carry the same verdict as the rest of the contact. */
                contact_confident[i] = false;
            }
            contact_was_down[i] = tip_now;

            /* Hide, do not flag. The host is never shown an unconfident contact:
             * a contact that has not earned its footprint is simply not reported,
             * and one that has is reported normally from that frame on. See the
             * block after phantom_probe_sample() for where tip is actually cleared
             * -- the probe must still see the raw contact. */
            gate_hidden[i] = tip_now && !contact_confident[i];
#endif
        }
#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
        /* Big-endian, one 16-bit word per Tx row, bit N = Rx N (8.10.5). The
         * prox block carries two extra bytes for the ALP channel, whose status
         * is bit 0 of the trailing word. */
        for (uint8_t tx = 0; tx < 15; tx++) {
            digitizer_diag.touch_status[tx] = ((uint16_t)combined.touch_status[tx * 2] << 8) | combined.touch_status[tx * 2 + 1];
            digitizer_diag.prox_status[tx]  = ((uint16_t)combined.prox_status[tx * 2] << 8) | combined.prox_status[tx * 2 + 1];
        }
        digitizer_diag.alp_prox = (combined.prox_status[31] & 0x01) != 0;

        /* Still inside the comms window: service a block read the probe asked
         * for. At most one per frame, and the probe only asks every few minutes,
         * so the 3ms a 300-byte read costs at 1MHz is not in the hot path. */
        if (probe_block_req_len) {
            if (i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, probe_block_req_reg, probe_block_buf, probe_block_req_len, AZOTEQ_IQS5XX_TIMEOUT_MS) == I2C_STATUS_SUCCESS) {
                probe_block_got_len = probe_block_req_len;
            }
            probe_block_req_len = 0;
        }
#endif
        azoteq_iqs5xx_end_session();
#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
        // After end_session() on purpose: the probe can emit console output, and
        // that must never stall an open IQS5xx comms window.
        phantom_probe_sample(&digitizer_report);
#endif
#ifdef AZOTEQ_IQS5XX_CONFIDENCE_GATE
        /* Last thing, and deliberately after the probe: PHSUM / PHCH / PH DOWN
         * count raw sensor contacts, so the gate must not touch the report until
         * the probe has read it. From here on the contact does not exist as far
         * as the host is concerned. */
        for (int i = 0; i < 5; i++) {
            if (gate_hidden[i]) {
                digitizer_report.contacts[i].tip = 0;
            }
        }
#endif
    } else {
        // Clear gesture events on I2C failure so stale bits don't fire keycodes.
        digitizer_diag.read_ok = false;
        digitizer_diag.i2c_errors++;
        memset(&digitizer_gesture_ev0, 0, sizeof(digitizer_gesture_ev0));
        memset(&digitizer_gesture_ev1, 0, sizeof(digitizer_gesture_ev1));
        digitizer_gesture_x_delta = 0;
    }
    return digitizer_report;
}
#endif
