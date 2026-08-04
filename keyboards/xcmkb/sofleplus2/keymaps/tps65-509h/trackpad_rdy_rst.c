// Copyright 2026 XCMKB
// SPDX-License-Identifier: GPL-2.0-or-later
//
// v5.07 experiment (this keymap only): use the hand-wired IQS5xx RDY (GP13)
// and RST (GP14) lines. See config.h for the pin defines.
//
// RDY -- edge-triggered read gating.
//   The IQS5xx RDY line is a communication-window strobe, not a "motion" line:
//   it rises when a fresh report is ready and a comm window opens (~every
//   AZOTEQ_IQS5XX_REPORT_RATE ms in stream mode) and falls after END_COMMS.
//   QMK's stock DIGITIZER_MOTION_PIN treats the pin as a level gate, which
//   failed two ways in earlier tests on this hardware:
//     1. While RDY is high, digitizer_task re-reads the same 45-byte report
//        every main-loop pass (I2C bus hog -> laggy cursor). The earlier
//        ACTIVE_LOW attempt was worse: it read only when the window was
//        CLOSED (inverted polarity -> stale/corrupt data).
//     2. The pin gates BOTH halves, so the half without the trackpad was
//        gated by a floating GPIO (cursor erratic/dead when left was master).
//   This override keeps the DIGITIZER_MOTION_PIN plumbing (pin init + the
//   gating hook in quantum/digitizer.c) but fixes the semantics:
//     - trackpad side: read once per RDY rising edge = one clean read per
//       comm window, right when it opens (max margin, no half-closed-window
//       race), naturally paced at the chip's report rate;
//     - other side: never touch the pin; pace shared-report consumption on a
//       timer (same cadence as the old DIGITIZER_TASK_THROTTLE_MS).
//   Safety: if RDY stops pulsing (broken wire / wedged chip) we degrade to
//   v5.06-style timed polling, so the trackpad can never go fully dead the
//   way the pcb7 GP25 experiment did.
//
// RST -- hardware reset + recovery.
//   Boot: pulse RST in keyboard_pre_init_user so the chip boots clean BEFORE
//   azoteq_iqs5xx_init() writes its register config (a post-init pulse would
//   wipe that config back to NVM defaults -- wrong rotation/report rate).
//   Runtime: with LP modes disabled the chip must strobe RDY continuously,
//   so "no RDY edge for several seconds" means it is wedged: pulse RST and
//   re-run azoteq_iqs5xx_init(). Rate-limited and capped so a genuinely
//   broken RDY wire causes at most RST_MAX_ATTEMPTS blips before settling
//   into the polling fallback.

#include "quantum.h"

#if defined(DIGITIZER_ENABLE) && defined(DIGITIZER_MOTION_PIN)

#    include "digitizer.h"
#    include "drivers/sensors/azoteq_iqs5xx.h"

#    ifndef TRACKPAD_RST_PIN
#        define TRACKPAD_RST_PIN GP14
#    endif

// Poll cadence when RDY is not usable (matches the old v5.06 throttle).
#    define RDY_FALLBACK_POLL_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)
// No rising edge for this long -> assume RDY is not strobing, use polling.
#    define RDY_FALLBACK_TIMEOUT_MS 250
// No rising edge for this long -> chip presumed wedged, try a hard reset.
#    define RST_RECOVERY_TIMEOUT_MS 3000
// Minimum spacing between recovery resets / cap on consecutive attempts.
#    define RST_RECOVERY_SPACING_MS 10000
#    define RST_MAX_ATTEMPTS 3

static uint32_t last_edge_time = 0;
static uint32_t last_read_time = 0;
static uint32_t last_rst_time  = 0;
static uint32_t last_call_time = 0;
static uint8_t  rst_attempts   = 0;
static bool     last_rdy       = false;

static void trackpad_hard_reset(void) {
    // NRST is active low. Release back to input-high afterwards so we never
    // fight the module's own pull-up.
    setPinOutput(TRACKPAD_RST_PIN);
    writePinLow(TRACKPAD_RST_PIN);
    wait_ms(2);
    setPinInputHigh(TRACKPAD_RST_PIN);
}

void keyboard_pre_init_user(void) {
    // Handedness may not be resolved this early, so pulse on both halves: on
    // the half without a trackpad GP14 drives an unconnected pad - harmless.
    trackpad_hard_reset();
    wait_ms(20); // chip boot headroom before I2C init reaches it
}

bool digitizer_motion_detected(void) {
    const uint32_t now = timer_read32();

    // Large gap since we last ran (USB suspend, blocking init): the silence
    // was ours, not the chip's - restart the observation window instead of
    // treating it as a wedged chip.
    if (TIMER_DIFF_32(now, last_call_time) > 500) {
        last_edge_time = now;
    }
    last_call_time = now;

    if (is_keyboard_left()) {
        // DIGITIZER_RIGHT: no trackpad on this half. Never read the
        // (floating) pin - just pace consumption of the split-synced report
        // like the old DIGITIZER_TASK_THROTTLE_MS did.
        if (TIMER_DIFF_32(now, last_read_time) >= RDY_FALLBACK_POLL_MS) {
            last_read_time = now;
            return true;
        }
        return false;
    }

    // ---- Trackpad side ----
    const bool rdy  = readPin(DIGITIZER_MOTION_PIN);
    const bool edge = rdy && !last_rdy;
    last_rdy        = rdy;

    if (edge) {
        last_edge_time = now;
        rst_attempts   = 0;
    }

    bool want_read = false;
    if (rdy) {
        // Edge = window just opened (normal path). The level catch-up covers
        // a missed low phase (e.g. one long OLED transfer straddling the
        // strobe) or a window the chip holds open unserviced.
        want_read = edge || TIMER_DIFF_32(now, last_read_time) >= 2 * AZOTEQ_IQS5XX_REPORT_RATE;
    } else if (TIMER_DIFF_32(now, last_edge_time) > RDY_FALLBACK_TIMEOUT_MS) {
        // RDY silent: degrade to v5.06-style timed polling so a broken RDY
        // wire can never kill the trackpad.
        want_read = TIMER_DIFF_32(now, last_read_time) >= RDY_FALLBACK_POLL_MS;
    }

    // Global floor: the chip cannot produce data faster than its report
    // rate; this also tames a floating/oscillating pin (rate-limits spurious
    // "edges" instead of hammering the bus like the old level gate).
    if (want_read && TIMER_DIFF_32(now, last_read_time) >= AZOTEQ_IQS5XX_REPORT_RATE - 1) {
        last_read_time = now;
        return true;
    }

    if (TIMER_DIFF_32(now, last_edge_time) > RST_RECOVERY_TIMEOUT_MS && rst_attempts < RST_MAX_ATTEMPTS && TIMER_DIFF_32(now, last_rst_time) > RST_RECOVERY_SPACING_MS) {
        // RDY has been silent for seconds: hardware-reset the chip and
        // reprogram it. Blocks ~150ms, but only runs when the trackpad is
        // already not delivering data.
        trackpad_hard_reset();
        wait_ms(20);
        azoteq_iqs5xx_init();
        last_rst_time  = timer_read32();
        last_edge_time = last_rst_time;
        rst_attempts++;
    }

    return false;
}

#endif // DIGITIZER_ENABLE && DIGITIZER_MOTION_PIN
