// Copyright 2023 Dasky (@daskygit)
// Copyright 2023 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "compiler_support.h"
#include "i2c_master.h"
#include "util.h"
#include "report.h"
#ifdef POINTING_DEVICE_DRIVER_azoteq_iqs5xx
#    include "pointing_device.h"
#endif

#if defined(AZOTEQ_IQS5XX_TPS43)
#    define AZOTEQ_IQS5XX_WIDTH_MM 43
#    define AZOTEQ_IQS5XX_HEIGHT_MM 40
#    define AZOTEQ_IQS5XX_RESOLUTION_X 2048
#    define AZOTEQ_IQS5XX_RESOLUTION_Y 1792
#elif defined(AZOTEQ_IQS5XX_TPS65)
#    define AZOTEQ_IQS5XX_WIDTH_MM 65
#    define AZOTEQ_IQS5XX_HEIGHT_MM 49
#    define AZOTEQ_IQS5XX_RESOLUTION_X 3072
#    define AZOTEQ_IQS5XX_RESOLUTION_Y 2048
#elif !defined(AZOTEQ_IQS5XX_WIDTH_MM) && !defined(AZOTEQ_IQS5XX_HEIGHT_MM)
#    error "You must define one of the available azoteq trackpads or specify at least the width and height"
#endif

typedef enum {
    AZOTEQ_IQS5XX_UNKNOWN,
    AZOTEQ_IQS550 = 40,
    AZOTEQ_IQS525 = 52,
    AZOTEQ_IQS572 = 58,
} azoteq_iqs5xx_product_numbers_t;
typedef enum {
    AZOTEQ_IQS5XX_ACTIVE,
    AZOTEQ_IQS5XX_IDLE_TOUCH,
    AZOTEQ_IQS5XX_IDLE,
    AZOTEQ_IQS5XX_LP1,
    AZOTEQ_IQS5XX_LP2,
} azoteq_iqs5xx_charging_modes_t;

typedef struct {
    uint8_t h : 8;
    uint8_t l : 8;
} azoteq_iqs5xx_report_rate_t;

typedef struct PACKED {
    bool    single_tap : 1;     // Single tap gesture status
    bool    press_and_hold : 1; // Press and hold gesture status
    bool    swipe_x_neg : 1;    // Swipe in negative X direction status
    bool    swipe_x_pos : 1;    // Swipe in positive X direction status
    bool    swipe_y_pos : 1;    // Swipe in positive Y direction status
    bool    swipe_y_neg : 1;    // Swipe in negative Y direction status
    uint8_t _unused : 2;        // unused
} azoteq_iqs5xx_gesture_events_0_t;

typedef struct PACKED {
    bool    two_finger_tap : 1; // Two finger tap gesture status
    bool    scroll : 1;         // Scroll status
    bool    zoom : 1;           // Zoom gesture status
    uint8_t _unused : 5;        // unused
} azoteq_iqs5xx_gesture_events_1_t;

typedef struct PACKED {
    azoteq_iqs5xx_charging_modes_t charging_mode : 3;      // Indicates current mode
    bool                           ati_error : 1;          //
    bool                           reati_occurred : 1;     //
    bool                           alp_ati_error : 1;      //
    bool                           alp_reati_occurred : 1; //
    bool                           show_reset : 1;         //
} azoteq_iqs5xx_system_info_0_t;

typedef struct PACKED {
    bool    tp_movement : 1;      //
    bool    palm_detect : 1;      //  Palm detect status
    bool    too_many_fingers : 1; // Total finger status
    bool    rr_missed : 1;        // Report rate status
    bool    snap_toggle : 1;      // Change in any snap channel status
    bool    switch_state : 1;     // Status of input pin SW_IN
    uint8_t _unused : 2;          // unused
} azoteq_iqs5xx_system_info_1_t;

typedef struct {
    uint8_t h : 8;
    uint8_t l : 8;
} azoteq_iqs5xx_xy_t;

typedef struct {
    uint8_t                          previous_cycle_time;
    azoteq_iqs5xx_gesture_events_0_t gesture_events_0;
    azoteq_iqs5xx_gesture_events_1_t gesture_events_1;
    azoteq_iqs5xx_system_info_0_t    system_info_0;
    azoteq_iqs5xx_system_info_1_t    system_info_1;
    uint8_t                          number_of_fingers;
    azoteq_iqs5xx_xy_t               x;
    azoteq_iqs5xx_xy_t               y;
} azoteq_iqs5xx_base_data_t;

STATIC_ASSERT(sizeof(azoteq_iqs5xx_base_data_t) == 10, "azoteq_iqs5xx_base_data_t should be 10 bytes");

typedef struct {
    uint8_t            number_of_fingers;
    azoteq_iqs5xx_xy_t x;
    azoteq_iqs5xx_xy_t y;
} azoteq_iqs5xx_report_data_t;

STATIC_ASSERT(sizeof(azoteq_iqs5xx_report_data_t) == 5, "azoteq_iqs5xx_report_data_t should be 5 bytes");

typedef struct PACKED {
    uint8_t h : 8;
    uint8_t l : 8;
} azoteq_iqs5xx_touch_strength_t;

typedef struct PACKED {
    azoteq_iqs5xx_xy_t             x;
    azoteq_iqs5xx_xy_t             y;
    azoteq_iqs5xx_touch_strength_t strength;
    uint8_t                        touch_area;
} azoteq_iqs5xx_absolute_finger_data_t;

_Static_assert(sizeof(azoteq_iqs5xx_absolute_finger_data_t) == 7, "azoteq_iqs5xx_absolute_finger_data_t should be 7 bytes");

typedef struct PACKED {
    azoteq_iqs5xx_absolute_finger_data_t fingers[5];
} azoteq_iqs5xx_digitizer_data_t;

//_Static_assert(sizeof(azoteq_iqs5xx_digitizer_data_t) == 40, "azoteq_iqs5xx_digitizer_data_t should be 40 bytes");

typedef struct PACKED {
    bool sw_input : 1;
    bool sw_input_select : 1;
    bool reati : 1;
    bool alp_reati : 1;
    bool sw_input_event : 1;
    bool wdt : 1;
    bool setup_complete : 1;
    bool manual_control : 1;
} azoteq_iqs5xx_system_config_0_t;

typedef struct PACKED {
    bool event_mode : 1;
    bool gesture_event : 1;
    bool tp_event : 1;
    bool reati_event : 1;
    bool alp_prox_event : 1;
    bool snap_event : 1;
    bool touch_event : 1;
    bool prox_event : 1;
} azoteq_iqs5xx_system_config_1_t;

typedef struct PACKED {
    bool    flip_x : 1;
    bool    flip_y : 1;
    bool    switch_xy_axis : 1;
    bool    palm_reject : 1;
    uint8_t _unused : 4;
} azoteq_iqs5xx_xy_config_0_t;

typedef struct PACKED {
    bool   suspend : 1;
    bool   reset : 1;
    int8_t _unused : 6;
} azoteq_iqs5xx_system_control_1_t;

/* System Control 0 (0x0431) -- IQS5xx-B000 Trackpad Datasheet rev 2.1, section
 * 8.10.7. Every bit is a write-1-to-trigger command, so issuing one is a plain
 * write and not a read-modify-write: there is no state here to preserve, and
 * writing zeros to the other bits triggers nothing. MODE_SELECT is ignored
 * unless MANUAL_CONTROL (System Config 0 bit 7) is set, which init() never sets. */
typedef struct PACKED {
    uint8_t mode_select : 3; // 0=Active 1=Idle-Touch 2=Idle 3=LP1 4=LP2; Manual Mode only
    bool    reseed : 1;      // reseed the trackpad reference values
    bool    alp_reseed : 1;  // reseed the alternate low-power channel LTA
    bool    auto_ati : 1;    // run the ATI algorithm
    bool    _unused : 1;
    bool    ack_reset : 1;   // clears SHOW_RESET in System Info 0 (datasheet 7.4.1)
} azoteq_iqs5xx_system_control_0_t;

STATIC_ASSERT(sizeof(azoteq_iqs5xx_system_control_0_t) == 1, "azoteq_iqs5xx_system_control_0_t should be 1 byte");

/* Smallest `touch_area` (channels grouped into one finger) that counts as a real
 * finger, used by AZOTEQ_IQS5XX_CONFIDENCE_GATE. 2 is the physical floor: a
 * 5.4 x 6.1 mm channel pitch against the module's 7.0 mm minimum finger means
 * anything genuine spans at least two channels. Raise only with baseline data in
 * hand -- 3 would reject light real touches. Lives in the header because the
 * phantom probe reports against the same number.
 */
#ifndef AZOTEQ_IQS5XX_CONFIDENCE_MIN_AREA
#    define AZOTEQ_IQS5XX_CONFIDENCE_MIN_AREA 2
#endif

typedef struct PACKED {
    bool   single_tap : 1;
    bool   press_and_hold : 1;
    bool   swipe_x_minus : 1;
    bool   swipe_x_plus : 1;
    bool   swipe_y_plus : 1;
    bool   swipe_y_minus : 1;
    int8_t _unused : 2;
} azoteq_iqs5xx_single_finger_gesture_enable_t;

typedef struct PACKED {
    bool   two_finger_tap : 1;
    bool   scroll : 1;
    bool   zoom : 1;
    int8_t _unused : 5;
} azoteq_iqs5xx_multi_finger_gesture_enable_t;

typedef struct PACKED {
    azoteq_iqs5xx_single_finger_gesture_enable_t single_finger_gestures;
    azoteq_iqs5xx_multi_finger_gesture_enable_t  multi_finger_gestures;
    uint16_t                                     tap_time;
    uint16_t                                     tap_distance;
    uint16_t                                     hold_time;
    uint16_t                                     swipe_initial_time;
    uint16_t                                     swipe_initial_distance;
    uint16_t                                     swipe_consecutive_time;
    uint16_t                                     swipe_consecutive_distance;
    int8_t                                       swipe_angle;
    uint16_t                                     scroll_initial_distance;
    int8_t                                       scroll_angle;
    uint16_t                                     zoom_initial_distance;
    uint16_t                                     zoom_consecutive_distance;
} azoteq_iqs5xx_gesture_config_t;

STATIC_ASSERT(sizeof(azoteq_iqs5xx_gesture_config_t) == 24, "azoteq_iqs5xx_gesture_config_t should be 24 bytes");

typedef struct {
    uint16_t x_resolution;
    uint16_t y_resolution;
} azoteq_iqs5xx_resolution_t;

#define AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(h, l) ((int16_t)(h << 8) | l)
#define AZOTEQ_IQS5XX_SWAP_H_L_BYTES(b) ((uint16_t)((b & 0xff) << 8) | (b >> 8))

#ifndef AZOTEQ_IQS5XX_REPORT_RATE
#    define AZOTEQ_IQS5XX_REPORT_RATE 10
#endif
#if !defined(POINTING_DEVICE_TASK_THROTTLE_MS) && !defined(POINTING_DEVICE_MOTION_PIN)
// Polling the Azoteq isn't recommended, ensuring we only poll after the report is ready stops any unexpected NACKs
#    define POINTING_DEVICE_TASK_THROTTLE_MS AZOTEQ_IQS5XX_REPORT_RATE + 1
#endif

#ifdef POINTING_DEVICE_DRIVER_azoteq_iqs5xx
extern const pointing_device_driver_t azoteq_iqs5xx_pointing_device_driver;
#endif

#ifdef DIGITIZER_ENABLE
/* Diagnostic snapshot of the last digitizer_driver_get_report() I2C read.
 *
 * Every field down to area[] is ALREADY fetched by the existing single 45-byte
 * read, so exposing it costs zero extra bus traffic and changes no behaviour.
 * (The channel bitmaps below it are the one exception, and exist only in
 * phantom-probe builds, where the same read is extended to 107 bytes.) It exists
 * so a probe can answer the question the driver never asks: when a contact is
 * asserted, does the chip itself agree a finger is there, and is the thing it
 * found finger-sized?
 *
 *   number_of_fingers  - the chip's own count. A slot with strength above
 *                        threshold while this reads 0 is a self-contradictory
 *                        frame, i.e. the contact is fabricated.
 *   strength[] / area[]- per-slot amplitude and touch area. TPS65 needs a 7.0mm
 *                        finger (module datasheet Table 1.3), so a real contact
 *                        cannot have a trivial area.
 *   resets             - count of SHOW_RESET observations after init() cleared
 *                        the boot latch, i.e. SPONTANEOUS device resets. Each
 *                        one silently reverted the chip to its NVM defaults,
 *                        discarding everything init() configured.
 *   system_info_0      - show_reset (chip silently reverted to NVM defaults),
 *                        ati_error / reati_occurred (baseline recalibrated),
 *                        charging_mode (ACTIVE / IDLE / LP1 / LP2).
 *   system_info_1      - palm_detect, too_many_fingers, rr_missed.
 */
typedef struct {
    bool                          read_ok;             // last I2C read succeeded
    uint32_t                      i2c_errors;          // cumulative failed reads since boot
    uint32_t                      resets;              // SHOW_RESET seen set since init acknowledged it
    uint32_t                      reseeds;             // host-issued RESEEDs, build D; always 0 without it
    uint8_t                       previous_cycle_time; // ms the chip's last sensing cycle took
    uint8_t                       number_of_fingers;
    azoteq_iqs5xx_system_info_0_t system_info_0;
    azoteq_iqs5xx_system_info_1_t system_info_1;
    uint16_t                      strength[5];
    uint8_t                       area[5];
#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
    /* Per-channel status bitmaps, 15 Tx rows x 16 bits, bit N = Rx N (only 0-9
     * populated on a 15x10 sensor). Datasheet 8.10.5: the registers always map
     * to Txs and the bits always map to Rxs, and -- crucially -- they do NOT
     * follow the flip/switch axis configuration. So these give the chip's own
     * INTERNAL channel indices, with none of the coordinate-frame ambiguity
     * that comes from inferring a node from the reported x/y.
     *   touch_status - channels whose count exceeded their touch threshold.
     *   prox_status  - channels above the (lower) proximity threshold, i.e. the
     *                  sub-touch activity that PHNOISE could never see.
     * Only compiled in for the phantom probe, and only then does the driver's
     * read grow from 45 to 107 bytes to cover 0x0039-0x0076. */
    uint16_t                      touch_status[15];
    uint16_t                      prox_status[15];
    bool                          alp_prox;
#endif
} azoteq_iqs5xx_digitizer_diag_t;

void azoteq_iqs5xx_get_digitizer_diag(azoteq_iqs5xx_digitizer_diag_t *out);

/* IDLE RESEED (build D, off unless the keymap defines AZOTEQ_IQS5XX_IDLE_RESEED).
 *
 * Datasheet 3.4: "The trackpad reference values are ONLY updated from LP1 and
 * LP2 mode when modes are managed automatically. Thus, if the system is
 * controlled manually, the reference must also be managed and updated manually
 * by the host." And 3.7.2 condition 1: an automatic Re-ATI fires when the
 * REFERENCE drifts outside ATI target +/- drift limit -- it watches the
 * reference, not the counts.
 *
 * Put those together with what this board actually does and there is no exit:
 * after the first few minutes it sits in Idle (charging mode 2) and never
 * returns to LP1, so the references are never refreshed; measured as PHREF
 * byte-identical across 57 dumps spanning 9 h 20 min. And it sits there because
 * azoteq_iqs5xx_init() writes 255 -- "never" -- to the Idle-mode timeout at
 * 0x0586 to keep the chip out of the LP report rates (upstream 62e98327d2,
 * "Azoteq - improve I2C behaviour while polling", #24611). That one byte
 * disables the chip's entire reference-maintenance path. The counts then drift
 * while the reference stays put, and because the reference is the thing that is
 * frozen, the reference-drift Re-ATI can never trigger either. Only a NEGATIVE
 * excursion has an escape (3.7.2's 15-cycle rule); drift in the touch direction
 * has none. Above the proximity threshold the pad also stops being idle enough
 * to drop into LP1, which closes the loop.
 *
 * Measured end to end on 2026-08-30: array-wide delta flat at -9 counts for four
 * hours, then +55.7 and climbing over twenty minutes on an untouched pad, then
 * 642 phantoms in 2 min 32 s once a channel reached the ~99-count touch
 * threshold. At the 3.2 counts/min observed there, a 5-minute reseed caps the
 * excursion at ~16-30 counts -- 3-6x below touch, and the storm cannot start.
 *
 * RESEED (System Control 0 bit 3) is the host-side half the datasheet asks for:
 * it re-stores each channel's reference from the current counts, without
 * re-running ATI and without a reset, so it costs one I2C write and nothing
 * else. It is normally issued only on a frame the chip reports as finger-free;
 * the interval timer, separately, is held only by a contact of touch_area >= 2
 * -- see the argument at the call site for why the raw finger count is the
 * wrong thing to hold it with.
 *
 * NOTE, measured 2026-09-01 and corrected here: "finger-free" is NOT the same
 * as "nothing on the pad". number_of_fingers is a TOUCH-THRESHOLD decision
 * while the reference is a RAW-COUNT quantity, so an object coupling below the
 * touch threshold still reads number_of_fingers == 0 and IS baselined in (an
 * earphone dropped on the pad moved five channels by up to +42 counts). An
 * earlier version of this comment claimed a hand could never be baselined in.
 * That claim was wrong. The gate sits one layer above the quantity it protects;
 * closing it properly needs the proximity bitmap, which is not in the 45-byte
 * production read, so it is deliberately left open. The excursion self-heals at
 * the next reseed after the object leaves.
 */
#ifndef AZOTEQ_IQS5XX_IDLE_RESEED_MS
#    define AZOTEQ_IQS5XX_IDLE_RESEED_MS 300000 // 5 min with no area >= 2 contact
#endif

/* Stuck-contact escape. The normal issue condition (number_of_fingers == 0)
 * deadlocks on a contact that is SUSTAINED rather than repeated: a single stuck
 * channel asserts number_of_fingers on every frame, while area == 1 means it
 * never holds the interval timer, so the timer expires and the reseed can never
 * fire. The reference then stays frozen for as long as the contact lasts --
 * "persists until unplugged", which is the client-reported symptom verbatim.
 *
 * Once the timer has been expired this long with no area >= 2 contact anywhere
 * on the pad, issue the reseed regardless of number_of_fingers. A real hand is
 * always area >= 2 (module datasheet: 7.0 mm minimum finger against a 5.4 x 6.1
 * mm channel pitch) and so holds the timer, so this cannot fire under a resting
 * hand; the only thing that reaches it is a sub-finger contact that has not
 * moved for two whole intervals. Costs nothing extra -- same 45-byte read.
 */
#ifndef AZOTEQ_IQS5XX_IDLE_RESEED_STUCK_MS
#    define AZOTEQ_IQS5XX_IDLE_RESEED_STUCK_MS (2 * AZOTEQ_IQS5XX_IDLE_RESEED_MS)
#endif

/* IDLE-MODE TIMEOUT (0x0586), the other half of the same fault.
 *
 * Datasheet p.20: "A timeout value of 255 will result in a 'never' timeout
 * condition", and the register is Timeout [s] - Idle mode. init() writes 255,
 * so the chip can never fall Idle -> LP1, and 3.4 above says LP1/LP2 is the
 * only place references are refreshed. Upstream chose it for a real reason --
 * the LP report rates open the RDY comms window far more slowly than a polling
 * driver likes, which is the i2cerr trickle #24611 was fixing -- so the default
 * here stays 255 and every other board keeps upstream behaviour.
 *
 * A keymap can override it with a finite number of seconds to let the chip do
 * its own reference maintenance again. That is the direct A/B for the frozen
 * reference: with a finite timeout, PHRUN m3 must start advancing, PHREF must
 * change on a cadence instead of never, alp becomes a live measurement instead
 * of the constant 0 it reads in Idle (figure 4.1: the ALP channel is sensed
 * only in LP1/LP2), and PHDEL cannot integrate past the touch threshold.
 * It is the diagnostic, not the shipping fix -- build D above is, because it
 * buys the same reference maintenance without giving up the 9 ms report rate. */
#ifndef AZOTEQ_IQS5XX_IDLE_TIMEOUT_S
#    define AZOTEQ_IQS5XX_IDLE_TIMEOUT_S 255 // datasheet p.20: 255 = never; upstream 62e98327d2
#endif

i2c_status_t azoteq_iqs5xx_reseed(bool end_session);

#ifdef AZOTEQ_IQS5XX_PHANTOM_PROBE
/* One-shot raw block reads for the probe.
 *
 * The IQS5xx only answers while its comms window is open -- the window the chip
 * itself opens by asserting RDY and that the driver closes with END_COMMS. A
 * read issued from the probe (which deliberately runs AFTER end_session, so its
 * console output can never stall an open window) would be NACKed. So the probe
 * asks for a block here and the driver performs the read inside its own window,
 * just before closing it.
 *
 * request() is a no-op if a request is already queued or an unclaimed result is
 * waiting. take() returns 0 until the read has happened, then the length once,
 * handing back a pointer to the driver's static buffer. Max block is 300 bytes,
 * which is exactly the size of the count / delta / reference arrays. */
#    define AZOTEQ_IQS5XX_PROBE_BLOCK_MAX 300
void     azoteq_iqs5xx_probe_request_block(uint16_t reg, uint16_t len);
uint16_t azoteq_iqs5xx_probe_take_block(const uint8_t **out);
#endif

// Returns chip-detected gesture events from the last digitizer_driver_get_report() call.
// ev0: single-finger gestures (swipe_x_neg/pos, swipe_y_neg/pos, tap, press_and_hold)
// ev1: multi-finger gestures (scroll, zoom, two_finger_tap)
// x_delta: signed relative X — for zoom: >0 = spreading (zoom in), <0 = pinching (zoom out)
void azoteq_iqs5xx_get_digitizer_gesture_events(azoteq_iqs5xx_gesture_events_0_t *ev0, azoteq_iqs5xx_gesture_events_1_t *ev1, int16_t *x_delta);
#endif

bool           azoteq_iqs5xx_init(void);
i2c_status_t   azoteq_iqs5xx_wake(void);
#ifdef POINTING_DEVICE_ENABLE
report_mouse_t azoteq_iqs5xx_get_report(report_mouse_t mouse_report);
#endif
i2c_status_t azoteq_iqs5xx_get_report_rate(azoteq_iqs5xx_report_rate_t *report_rate, azoteq_iqs5xx_charging_modes_t mode, bool end_session);
i2c_status_t azoteq_iqs5xx_set_report_rate(uint16_t report_rate_ms, azoteq_iqs5xx_charging_modes_t mode, bool end_session);
i2c_status_t azoteq_iqs5xx_set_event_mode(bool enabled, bool end_session);
i2c_status_t azoteq_iqs5xx_set_reati(bool enabled, bool end_session);
i2c_status_t azoteq_iqs5xx_set_gesture_config(bool end_session);
i2c_status_t azoteq_iqs5xx_set_xy_config(bool flip_x, bool flip_y, bool switch_xy, bool palm_reject, bool end_session);
i2c_status_t azoteq_iqs5xx_reset_suspend(bool reset, bool suspend, bool end_session);

/* Acknowledge a device reset, clearing SHOW_RESET so the NEXT one is visible.
 * SHOW_RESET latches: until this is called it stays set from the deliberate soft
 * reset in azoteq_iqs5xx_init(), and a later spontaneous reset -- watchdog, ESD,
 * brown-out -- cannot be distinguished from that boot-time one. */
i2c_status_t azoteq_iqs5xx_ack_reset(bool end_session);
i2c_status_t azoteq_iqs5xx_get_base_data(azoteq_iqs5xx_base_data_t *base_data);
void         azoteq_iqs5xx_set_cpi(uint16_t cpi);
uint16_t     azoteq_iqs5xx_get_cpi(void);
uint16_t     azoteq_iqs5xx_get_product(void);
void         azoteq_iqs5xx_setup_resolution(void);
