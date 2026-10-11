/* Copyright 2020 Josef Adamcik
 * Modification for VIA support and RGB underglow by Jens Bonk-Wiltfang
 * Modification for Vial support by Drew Petersen
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

// clang-format off

#pragma once

/* ============================================================================
 * sofleplus2u — Signature RGB board (72 LEDs: 7 underglow + 29 per-key per side)
 *
 * Port of sofleplus2/keymaps/tps65-510h (horizontal TPS65, ROTATION_180,
 * PID 0x0288) with the v5.10 feature set. Kept from the u's own tps65-504:
 *   - I2C1 at 100 kHz and the 16 ms task throttle (bus filter caps, see below)
 *   - no RDY/RST pin defines (GP13/GP14 are not wired on this batch)
 *   - VIALRGB_SPLIT_LEFT 36 and the swapped encoder entries in vial.json
 * Fixed for 72 LEDs in keymap.c: EEPROM indicator/OLED blocks moved above the
 * 216-byte direct-colour block, and the default indicator LEDs re-pointed at
 * the thumb keys (the base indices land on underglow here).
 *
 * The block below is the base keymap's own investigation history, kept
 * verbatim for reference; where it talks about 1 MHz I2C, this board runs
 * 100 kHz (see I2C1_CLOCK_SPEED).
 * ============================================================================ */

/* ============================================================================
 * v5.10h — PHANTOM-CONTACT INSTRUMENTATION BUILD (branch: phantom-510h)
 *
 * Byte-for-byte copy of tps65-507h, which is the configuration the phantom
 * reproduces on. Nothing here is a fix. The purpose of this keymap is to make
 * ONE phantom episode measurable, so we can see what a fabricated contact
 * actually looks like (strength / touch_area / number_of_fingers) instead of
 * guessing thresholds.
 *
 * DELIBERATELY LEFT BROKEN: I2C1_CLOCK_SPEED stays at 1MHz (see line ~50).
 * The datasheet caps the IQS5xx bus at 400kHz, so that IS out of spec and IS a
 * suspect — but dropping it now would risk suppressing the fault before we have
 * captured a single episode. Change it only as a SEPARATE build, after data.
 *
 * Symptom being chased: with NOBODY touching the board (idle / video / gaming
 * on a different keyboard), the cursor emits repeated left clicks, occasional
 * right clicks, and steps ~5cm horizontally and vertically. Reproduced on both
 * Windows and Linux, which are both driven from the PTP path — the path with
 * zero temporal filtering (drivers/sensors/azoteq_iqs5xx.c:480 derives `tip`
 * from a single sample, and confidence is hardcoded 1 at :479).
 * ============================================================================ */


#define EE_HANDS
//#define INIT_EE_HANDS_LEFT       // Auto-program EEPROM as LEFT on every boot (survives EEPROM resets)
//#define MASTER_LEFT
//#define MASTER_RIGHT
#define USB_VBUS_PIN GP19


#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP4
#define SERIAL_USART_RX_PIN GP1

/* i2c oled for left*/
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3
#define I2C1_CLOCK_SPEED 100000   // 100kHz standard mode — 1MHz FM+ too fast for 100pF filter caps on SDA/SCL (C1546); rise time ≈1µs at 4.7kΩ pull-up (kept from u tps65-504)
                                  // The base tps65-510h keeps 1MHz (out of the IQS5xx's 400kHz spec, left so
                                  // for its phantom investigation). This board's bus cannot do that anyway.

#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9
//#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
#define AZOTEQ_IQS5XX_ROTATION_180 /*for tps65*/
// Poll both tasks at 16ms: each cycle costs ~5ms I2C (10-byte PD + 45-byte digitizer at
// 100kHz), giving 11ms free for RGB vs only 5ms at the base keymap's 10ms throttle.
// (kept from u tps65-504). These throttles are only honoured because
// DIGITIZER_MOTION_PIN is NOT defined below — see the RDY/RST note.
#define POINTING_DEVICE_TASK_THROTTLE_MS 16
#define DIGITIZER_TASK_THROTTLE_MS 16
//#define AZOTEQ_IQS5XX_ROTATION_90 /*for tps65 rotate version*/

/* RDY/RST — deliberately NOT defined on sofleplus2u.
 * The base tps65-510h defines DIGITIZER_MOTION_PIN GP13 / TRACKPAD_RST_PIN GP14
 * for hand-wired IQS5xx RDY/RST lines and builds trackpad_rdy_rst.c. This
 * batch has neither pin connected, and defining DIGITIZER_MOTION_PIN makes
 * quantum/digitizer.c #undef DIGITIZER_TASK_THROTTLE_MS, so pacing would come
 * from a floating GP13 (8-18 ms reads) instead of the 16 ms budget above that
 * the 100 kHz bus needs; it would also run up to three bogus boot-time
 * hard-reset + re-ATI cycles on a chip that never sees the RST line. Timed
 * polling here is the same path tps65-506a shipped on. If a unit is ever
 * hand-wired, add the two defines and `SRC += trackpad_rdy_rst.c` back from
 * the base keymap. */



/* azoteq gesture settings */
// https://github.com/qmk/qmk_firmware/blob/master/docs/feature_pointing_device.md#gesture-settings
/* azoteq config: optimized for larger TPS65 dimensions */
#define AZOTEQ_IQS5XX_HOLD_TIME 300 // Default 300 v3.02
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 10 // Standard distance for larger TPS65 trackpad
#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE false // Disabled: Windows PTP handles this natively; hardware gesture conflicts with PTP causing phantom drag
#define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE false // Disabled: Windows PTP handles 2-finger right-click natively; hardware gesture causes double right-click
#define AZOTEQ_IQS5XX_SCROLL_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_ENABLE true //(Optional) Enable zoom gestures Zoom Out (Mouse Button 7) / Zoom In (Mouse Button 8)
// Zoom thresholds: TPS65 = 47.3 units/mm. Default initial=50 (~1mm) fires during normal scroll.
// Raised to ~3mm initial so only deliberate pinch/spread triggers zoom, not scroll finger variation.
#define AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE 150     // ~3.2mm span change to start zoom (default 50)
#define AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE 80  // ~1.7mm per step once zoom is active (default 25)
#define AZOTEQ_IQS5XX_MIN_STRENGTH 50               // Noise floor: ignore contacts with strength ≤50 to prevent phantom touches from EMI/capacitive noise



/* ============================================================================
 * v5.10h — PHANTOM MITIGATION (RELEASE BUILD)
 *
 * This is the client build. The diagnostic probe lives in tps65-510h-probe;
 * this keymap is that one with the probe removed and nothing else changed. The two mitigations below (BUILD A and BUILD D) are
 * both ON and are what this release ships; BUILD B and BUILD C are recorded
 * for history and are NOT enabled. Validated 2026-09-01 on the probe build over a
 * 2 h 41 m gaming session: zero contacts, reference tracked continuously,
 * no Re-ATI, none of the drift that precedes a storm.
 * The probe (AZOTEQ_IQS5XX_PHANTOM_PROBE, phantom_probe.c, the 107-byte read)
 * lives only in tps65-510h-probe and must not be added here.
 *
 * ONE VARIABLE PER SESSION was the rule while these were being tested: a storm
 * takes 1h42m-2h26m of gaming to onset and the rate varies 8x between sessions
 * with an identical setup, so two changes at once cannot be attributed. That
 * rule still applies to anything changed here -- if a client reports a
 * regression, revert ONE define and retest, do not change both.
 * ------------------------------------------------------------------------- */

/* --- BUILD A (this one): the mitigation, tested against a known storm -------
 * A contact whose `touch_area` never reaches 2 is not reported to the host at
 * all. It still appears in the probe log, so the phantom is measured and
 * silenced in the same session.
 *
 * Geometry, not luck -- 13 Tx x 9 Rx over 65 x 49 mm is a 5.4 x 6.1 mm pitch
 * against the module datasheet's 7.0 mm minimum finger, so anything genuine
 * spans two channels or more. All 1506 captured phantoms had area == 1; only
 * 2 of 115 real contacts ever did.
 *
 * REVISED 2026-08-29 after the first build was flashed. The original delivered
 * the verdict as confidence = 0 on a reported contact, and on Windows PTP that
 * cost tap-to-click and scroll inertia outright: the release frame carried
 * confidence = 0 (that is the frame Windows turns into a click), and 23% of
 * real contacts start at area 1 and grow, so the host also saw confidence flip
 * mid-contact. Hiding the contact instead costs 1-2 frames (9-18 ms) of onset
 * delay on that 23% and nothing else; the host only ever sees whole, confident
 * contacts. If taps or gestures still misbehave, this define is the first thing
 * to comment out -- everything else on the branch is diagnostic.
 *
 * Report rate deliberately LEFT AT 9 for this session: reproduce the storm
 * under known conditions, confirm the clicks stop, and confirm real touches and
 * gestures are unaffected. Changing the rate at the same time risks suppressing
 * the storm and leaving the gate unvalidated. */
#define AZOTEQ_IQS5XX_CONFIDENCE_GATE
// #define AZOTEQ_IQS5XX_CONFIDENCE_MIN_AREA 2   // default; raise only with baseline data

/* --- BUILD B: does sensor timing matter? -----------------------------------
 * `rrmiss` is set on 100% of phantom frames and `cyc` runs 9-22ms against a
 * configured 9ms, so the chip never meets its budget -- a standing defect on
 * every board in the batch, independent of the phantom. 16 asks 62.5Hz instead
 * of 111Hz and widens each Tx/Rx crossing's sampling window from ~77us to
 * ~137us, so a periodic aggressor must land on a different set of channels.
 * Change line ~70 (AZOTEQ_IQS5XX_REPORT_RATE) from 9 to 16 for this session.
 * Cost: the poll throttle follows to ~59Hz and pointer feel visibly suffers.
 * If 16 changes nothing, ship 12 (83Hz asked, ~77Hz polled) -- `cyc`'s median
 * is 10-11ms, so a 12ms budget is met most of the time and the feel is close to
 * unchanged. */

/* --- BUILD C: chip-side single-cycle rejection -----------------------------
 * Writes 0x0067A = TOUCH_DB_SET 1: two consecutive samples above threshold
 * before a channel's touch output asserts. 70-89% of every phantom measured
 * lasts exactly one sensing cycle. Costs one cycle of latency on real touches
 * against a 54ms (5-frame) shortest real tap, so re-run the finger baseline.
 * Not persisted to NVM -- a power cycle reverts it.
 * CONFIRM IT TOOK: the next PHCFG line must read `touchdb=10`. If it reads 00
 * the write failed; if the board misbehaves, unplug and it is gone. */
// #define AZOTEQ_IQS5XX_TOUCH_DEBOUNCE 0x10

/* --- BUILD D: the reference is never refreshed ------------------------------
 * Datasheet 3.4 says trackpad references are only updated from LP1/LP2 mode and
 * that a manually managed system must refresh them from the host; 3.7.2 says the
 * automatic Re-ATI watches the REFERENCE against the ATI target, not the counts.
 * This board leaves LP1 a few minutes after boot and never goes back (PHRUN m3
 * shows it), so neither mechanism runs: PHREF was byte-identical across 57 dumps
 * spanning 9 h 20 min, and a drift in the touch direction has no escape at all.
 *
 * RESEED re-baselines every channel from its current counts -- one I2C write, no
 * ATI, no reset, no latency -- after the chip has reported zero fingers for
 * AZOTEQ_IQS5XX_IDLE_RESEED_MS (default 5 min).
 *
 * This is the first candidate that could explain the MODE A idle-overnight
 * phantom on boards with no RDY/RST, so test it there, not only here.
 * CONFIRM IT TOOK: `rsd=` on PHRUN must climb while the pad is untouched, and
 * PHDEL must return to the +/-5 band after each one. */
#define AZOTEQ_IQS5XX_IDLE_RESEED
#define AZOTEQ_IQS5XX_IDLE_RESEED_MS 300000

/* RUN 1 (2026-08-31) -- the confirming A/B for the frozen reference, build D OFF
 * so this is the only variable. The driver otherwise writes 255 to the Idle-mode
 * timeout at 0x0586, which the datasheet (p.20) defines as "never": the chip can
 * then never fall into LP1, and 3.4 refreshes trackpad references ONLY from
 * LP1/LP2. Measured consequence on 2026-08-30: PHREF byte-identical for 5 h while
 * PHDEL climbed +55.7 counts on an untouched pad, then 642 phantoms in 2 min 32 s.
 *
 * 60 s of no activity now drops the chip to LP1 and lets it reseed its own
 * references. Expect: PHRUN m3 advancing (it has been 0 since t=307s on every run
 * so far), PHREF changing on a cadence, alp going non-zero for the first time
 * ever, PHDEL returning to ~0 after each LP1 visit and never reaching the ramp,
 * p1=0 across a gaming session. Also expect a trickle of i2cerr from the slower
 * LP RDY window, and a touch after idle may land one LP cycle late -- that cost
 * is exactly why this is the diagnostic and build D is the shipping candidate.
 * Set back to 255 (or delete the line) to restore upstream behaviour. */
// #define AZOTEQ_IQS5XX_IDLE_TIMEOUT_S 60   <- RUN 1, PASSED 2026-08-30, now off again.
// Left commented so the A/B can be re-run in one line. It stays the diagnostic, not the
// fix: measured cost was i2cerr ~1.3/min, poll rate ~29 frames/s against ~86, and up to
// ~160 ms to wake from LP2. Build D above buys the same reference maintenance at the
// full 9 ms report rate, which is why RUN 2 is build D alone with to_idle back at 255.
// 1-finger hardware swipe disabled: indistinguishable from fast cursor movement, causes accidental
// back/forward. Layer-based swipe (swipe2_layer) is the correct approach for deliberate swipe.
#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE false
#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE false

/* macOS-friendly 3-finger swipe keycodes (mouse fallback mode only, Windows PTP unaffected) */
/* ROTATION_270: physical RIGHT → decreasing digitizer x → invert x for correct native swipe direction */
#define DIGITIZER_SWIPE_X_INVERT 1
#define DIGITIZER_SWIPE_UP_KC    LCTL(KC_UP)     // macOS Mission Control
#define DIGITIZER_SWIPE_DOWN_KC  LCTL(KC_DOWN)   // macOS App Exposé
#define DIGITIZER_SWIPE_LEFT_KC  LCTL(KC_LEFT)   // macOS Previous Desktop
#define DIGITIZER_SWIPE_RIGHT_KC LCTL(KC_RIGHT)  // macOS Next Desktop

/* Mouse fallback tuning for macOS */
#define DIGITIZER_SCROLL_INVERT true            // Natural scrolling for macOS




/* KVM OS detection */

  /* v5.07h FIELD FIX: OS_DETECTION_KEYBOARD_RESET disabled.
   * The macro reboots the MCU (soft_reset_keyboard -> mcu_reset) whenever USB
   * drops out of CONFIGURED -- quantum/os_detection.c:87-99. Every KVM switch,
   * resume and hub transition is a bus reset, so it fires constantly in the
   * field and explains all three client reports:
   *   - Windows "random reboots": the reset itself, by design.
   *   - Phantom gestures: a reset re-inits digitizer_send_mouse_reports to true
   *     (digitizer_mouse_fallback.c:212), which is cleared only by the host's
   *     PTP Input Mode feature report (usb_main.c:307-319). So the board comes
   *     back in macOS mouse-fallback mode on a Windows host -- tap-to-click,
   *     zoom, scroll and the swipe layers all live -- until Windows renegotiates.
   *     shutdown_quantum() also clears only the keyboard report, never the
   *     digitizer contacts, so a reset mid-touch leaves a stuck PTP contact.
   *   - Linux never enumerating: the reset lands mid-enumeration and loops until
   *     the kernel gives up on the port.
   * Safe to disable: the keymap's mac/win branches read detected_host_os(),
   * which returns the live detected_os (os_detection.c:179-181) and does not
   * depend on this macro or on the report callback. */
  // #define OS_DETECTION_KEYBOARD_RESET

  // Only send one report per OS detection cycle
  #define OS_DETECTION_SINGLE_REPORT

  // NOTE: OS_DETECTION_INITIAL_TIMEOUT does not exist in QMK -- it appears only
  // in our own config.h files and has never had any effect. Left in place to
  // record intent; the actual Linux fix is the disabled macro above.
  #define OS_DETECTION_INITIAL_TIMEOUT 2500
  #define OS_DETECTION_DEBOUNCE 300          // Extended from 200ms to 300ms for stability

/* Power Management - OLED and RGB sleep/wake */
#undef OLED_TIMEOUT
#define OLED_TIMEOUT 0                // core OLED auto-off disabled; keymap owns sleep via g_sleep_timeout
#define RGB_MATRIX_TIMEOUT 60000      // boot default for g_rgb_matrix_timeout (1 min); overridden at runtime

/* v5.06: runtime-configurable shared OLED+RGB sleep timeout (set in Vial, whole minutes).
 * Stored internally in ms; clamped to [1 min, 30 min] — 0/never is disallowed to
 * protect the OLED from burn-in. */
#define DEFAULT_SLEEP_TIMEOUT_MS  60000    // 1 min
#define MIN_SLEEP_TIMEOUT_MS      60000    // 1 min
#define MAX_SLEEP_TIMEOUT_MS      1800000  // 30 min
#define RGB_MATRIX_SLEEP             // Enable RGB sleep when suspended
#define RGB_MATRIX_DEFAULT_ON true   // RGB on by default

/* v5.06: sync OLED/RGB sleep across both halves. Master broadcasts matrix +
 * encoder + pointing-device (trackpad) activity to the slave, so the slave's
 * last_input_activity_elapsed() tracks ALL input, not just its own keypresses.
 * Fixes one half sleeping while the other stays awake (e.g. from trackpad use).
 * NOTE: must be a config.h #define — the SPLIT_ACTIVITY_ENABLE=yes line in
 * rules.mk is not a recognised build feature and does nothing. */
#define SPLIT_ACTIVITY_ENABLE

/* Standardise the VialRGB "Customise" per-key effect with general effects and
 * layer indicators: track the Vial brightness slider against the max-brightness
 * ceiling instead of double-scaling against 255 (which rendered it ~39% dimmer). */
#define VIALRGB_CUSTOMISE_TRACK_MAX_BRIGHTNESS

/* v5.06 update to standardise layer indicator colour with the RGB effects.
 * Indicators track the Vial brightness slider but render at this % of the
 * ceiling, so a steady indicator doesn't out-shine the (usually sub-ceiling)
 * RGB effects. 100 = full ceiling; lower = dimmer indicators. */
#define INDICATOR_BRIGHTNESS_PCT 65




/* Keyboard name override for this keymap */
#undef PRODUCT
#define PRODUCT "SoflePLUS2 TPS65 Horizontal v5.11h Signature RGB"

/* USB Product ID -- distinct per variant so several boards can be plugged in at
 * once. builddefs/build_vial.mk force-defines SERIAL_NUMBER="vial:f64c2b3c" on
 * EVERY Vial build, and usb_descriptor.c prefers that static string over the
 * per-MCU hardware ID, so every board reports the same serial. Windows collapses
 * identical VID+PID+serial into one device instance, which is why a second board
 * silently fails to appear. The serial cannot be changed -- the Vial app finds a
 * keyboard by the "vial:" prefix -- so the PID is the only lever that does not
 * require editing core.
 *
 * Allocation: 0x0287 board default = tps65-510 (vertical), 0x0288 = tps65-510h
 * (horizontal), 0x0289 = tps43-510. SCOPE: the PID is in the FIRMWARE, not the
 * unit, so two boards both flashed tps65-510h still collide; per-unit
 * coexistence needs a unique serial, i.e. a separate build per board.
 * Keymap config.h is last in QMK's config chain, so it overrides keyboard.json.
 * sofleplus2u uses the same allocation (per keymap variant, not per board), so
 * a sofleplus2u and a sofleplus2 both flashed tps65-510h collide on one host
 * exactly as two sofleplus2 would. */
#undef  PRODUCT_ID
#define PRODUCT_ID 0x0288



/* Vial UID: this board's own. Its vial.json differs from sofleplus2/tps65-510h
 * (the encoder entries are swapped, as on the u's tps65-504), so the two
 * layouts must not share a UID. (The UID has no role in USB enumeration — that
 * is VID/PID/serial — so it does not affect plugging two boards in at once.) */
#ifdef VIAL_ENABLE
#define VIAL_KEYBOARD_UID {0x9E, 0x08, 0x7E, 0x7C, 0x95, 0x0F, 0xB3, 0x28}
#endif

/* --- v5.11 ---------------------------------------------------------------------
 * Split messages for the per-LED indicator colours and the wake-up animation
 * (keymap.c). The board config.h lists the v5.10 set; extend it here. */
#undef SPLIT_TRANSACTION_IDS_USER
#define SPLIT_TRANSACTION_IDS_USER VIALRGB_DIRECT_SYNC, VIALRGB_INDICATOR_SYNC, OLED_SCREEN_B_SYNC, TP_INFO_SYNC, VIALRGB_INDICATOR_LED_SYNC, AURORA_SYNC

/* Wake-up "Aurora tide" animation on plug-in / PC start-up (keymap.c). Comment
 * out to drop it entirely. */
#define AURORA_WAKE_ENABLE
