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

/* =========================================================================
 * SoflePLUS v1.10 — clean break from the legacy mouse path to the PTP
 * digitizer stack already shipping on sofleplus2 v5.10.
 *
 * Nothing outside keyboards/xcmkb/sofleplus1/ is modified by this keymap.
 * The whole digitizer stack (quantum/digitizer.c,
 * quantum/digitizer_mouse_fallback.c, drivers/sensors/azoteq_iqs5xx.c) is
 * already in the tree and is selected purely by the build flags in rules.mk,
 * so sofleplus2 is not rebuilt, retested or affected in any way.
 * ========================================================================= */

#define EE_HANDS

/* USB VBUS SENSE — REQUIRED ON RP2040, AND THE REASON v1.10 REBOOT-LOOPED.
 *
 * On the original ATmega32U4 this was free: usb_vbus_state() reads the MCU's own
 * OTG VBUS pad (tmk_core/protocol/lufa/usb_util.c:30-36), so master/slave
 * delegation was instant and correct. RP2040 has no equivalent, so the call
 * falls through to the weak stub that just returns true
 * (tmk_core/protocol/usb_util.c:27-35) — and platforms/chibios/chibios_config.h:20-22
 * then FORCE-DEFINES SPLIT_USB_DETECT ("Force this on when dedicated pin is not
 * used") for the whole build.
 *
 * With SPLIT_USB_DETECT, split_util.c:67-76 decides the role by polling for
 * USB_ACTIVE every 10 ms for up to SPLIT_USB_TIMEOUT (2000 ms) inside
 * split_pre_init(). Lose that race — first plug of a new PID, or the host still
 * tearing down the RPI-RP2 mass-storage device on the same port right after a
 * flash — and is_keyboard_master_impl() (split_util.c:180-187) calls
 * usb_disconnect(): usbDisconnectBus() + usbStop(). The board leaves the bus.
 * It is then "slave", so split_post_init() armed the watchdog, and with no
 * master answering on GP1 split_watchdog_task() (split_util.c:114-121) calls
 * mcu_reset() at SPLIT_USB_TIMEOUT + 100 = 2100 ms. Reboot, re-enumerate,
 * repeat: connect / disconnect / connect, which is exactly what was observed.
 *
 * GP19 is the value QMK's own Pro Micro -> RP2040 converters inject
 * (-DUSB_VBUS_PIN=19U in platforms/chibios/converters/promicro_to_rp2040_ce/
 * converter.mk:10, and elite_c_to_rp2040_ce / imera / svlinky), and the same pin
 * sofleplus2 uses (tps65-510h/config.h:50). It is unused by this board: the
 * matrix is GP5-9 / GP20-23 / GP26-27, DIP is GP12-16, WS2812 GP0, split serial
 * GP1, I2C GP2-3, bootloader LED GP17, encoder GP28-29.
 *
 * SPLIT_WATCHDOG_ENABLE deliberately REMOVED with this. Per
 * docs/features/split_keyboard.md:471 it exists only to recover when
 * SPLIT_USB_DETECT delegates both sides as slave; with a dedicated VBUS pin the
 * delegation is deterministic and the watchdog can only turn a transient into a
 * reboot. sofleplus2 does not carry it either. */
#define USB_VBUS_PIN GP19

/* HANDEDNESS NOTE — deliberately left as bare EE_HANDS.
 *
 * The trackpad half (right) is normally also the master, because that is the
 * half the USB cable goes into. With DIGITIZER_RIGHT, transactions.c:804-808
 * makes digitizer_handlers_master() return before it touches the split link
 * when the master IS the trackpad half, so trackpad data never crosses the
 * wire — it goes I2C -> USB locally.
 *
 * Plug into the LEFT half instead and the right becomes slave, so the ~64-byte
 * digitizer_t payload does cross the half-duplex single-wire link every sync.
 * That is a degraded (possibly laggy) mode, not a broken one.
 *
 * MASTER_RIGHT would force the issue but would make a left-side plug fail to
 * enumerate at all, which is worse. Left as-is; documented in the release
 * notes as "plug into the right half". */

/* i2c oled for left */
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3


/* =========================================================================
 * TRACKPAD — Azoteq IQS5xx TPS65
 * ========================================================================= */

#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9 // 9ms (~111Hz); stable alongside RGB
#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
#define DIGITIZER_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)

/* RDY / RST ARE NOT AVAILABLE ON THIS HARDWARE — DO NOT ADD THEM.
 *
 * On sofleplus2 the trackpad has hand-wired RDY (GP13) and RST (GP14) lines and
 * its keymaps define DIGITIZER_MOTION_PIN / TRACKPAD_RST_PIN accordingly.
 * On sofleplus1 those two GPIOs are DIP SWITCH INPUTS
 * (see ../../config.h: DIP_SWITCH_PINS { GP12, GP13, GP14, GP15, GP16 }).
 *
 * Copying those two defines across from a sofleplus2 keymap would silently
 * steal two DIP inputs and break the switches on shipped hardware.
 *
 *   DO NOT define DIGITIZER_MOTION_PIN here.
 *   DO NOT define TRACKPAD_RST_PIN here.
 *
 * The digitizer does not need them: it falls back to timed polling paced by
 * DIGITIZER_TASK_THROTTLE_MS above. tps43-506a and tps65-506a on sofleplus2
 * both run this same driver with no RDY/RST at all. */

/* --- on-sensor gesture engine ------------------------------------------
 * Values taken from sofleplus2 tps65-510. v1.02b had press-and-hold and the
 * swipe gestures ENABLED, which is correct for the old mouse path but wrong
 * under PTP: the host implements those itself, and the hardware gesture
 * firing at the same time is what produces phantom drag and double
 * right-click. They are disabled here for exactly that reason. */
#define AZOTEQ_IQS5XX_HOLD_TIME 300
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 10 // standard distance for the larger TPS65
#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE false // PTP handles this natively; hardware gesture causes phantom drag
#define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE false // PTP handles 2-finger right-click; hardware gesture double-fires
#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE false
#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE false
#define AZOTEQ_IQS5XX_SCROLL_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE 150     // ~3.2mm span change to start zoom (default 50)
#define AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE 80  // ~1.7mm per step once active (default 25)
#define AZOTEQ_IQS5XX_MIN_STRENGTH 50               // noise floor: ignore contacts with strength <= 50

/* --- v5.10 PHANTOM-CLICK FIX -------------------------------------------
 * Ported verbatim from sofleplus2 tps65-510. Both of these live behind
 * #ifdef DIGITIZER_ENABLE in drivers/sensors/azoteq_iqs5xx.c, which is why
 * v1.02b could not reach them on the pointing-device path — the root cause
 * (init writing idle timeout 255 = "never" at azoteq_iqs5xx.c:414-421) is in
 * the SHARED part of init() and therefore applied to v1.02b too.
 *
 * IDLE_RESEED — the cause. Datasheet 3.4 refreshes per-channel references only
 * from LP1/LP2; writing 255 to 0x0586 means the chip never returns there, so
 * the references freeze for the session and drift walks the raw counts into
 * the touch threshold. RESEED re-references every channel from current counts:
 * one I2C write on a transaction the driver already makes.
 *
 * CONFIDENCE_GATE — the residue. touch_area is the number of channels grouped
 * into a contact; single-channel contacts cannot be a >=7mm finger, so they
 * are hidden rather than reported.
 *
 * NOT enabled here: AZOTEQ_IQS5XX_PHANTOM_PROBE. Diagnostic only, and it
 * widens the per-poll I2C read from 45 to 107 bytes. Never in a client build. */
#define AZOTEQ_IQS5XX_IDLE_RESEED
#define AZOTEQ_IQS5XX_IDLE_RESEED_MS 300000     // reseed after 5 min with no area >= 2 contact
#define AZOTEQ_IQS5XX_CONFIDENCE_GATE
// #define AZOTEQ_IQS5XX_CONFIDENCE_MIN_AREA 2  // default; raise only with baseline data

/* Trackpad is on the right half. transactions.c short-circuits the split
 * transaction entirely when the right half is also the master. */
#define SPLIT_DIGITIZER_ENABLE
#define DIGITIZER_RIGHT
#define POINTING_DEVICE_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)

/* --- mouse-fallback gesture tuning (macOS / non-PTP hosts only) ---------
 * ROTATION_270: physical RIGHT gives a decreasing digitizer x, so x is inverted
 * to make native swipe direction come out right. */
#define DIGITIZER_SWIPE_X_INVERT 1
#define DIGITIZER_SWIPE_UP_KC    LCTL(KC_UP)     // macOS Mission Control
#define DIGITIZER_SWIPE_DOWN_KC  LCTL(KC_DOWN)   // macOS App Expose
#define DIGITIZER_SWIPE_LEFT_KC  LCTL(KC_LEFT)   // macOS Previous Desktop
#define DIGITIZER_SWIPE_RIGHT_KC LCTL(KC_RIGHT)  // macOS Next Desktop
#define DIGITIZER_SCROLL_INVERT true             // natural scrolling for macOS


/* =========================================================================
 * SPLIT TRANSPORT — TP_INFO_SYNC
 * ========================================================================= */

/* One master->slave RPC feeding the slave OLED: tp_info_payload_t is 79 bytes
 * packed, pushed every 500ms from housekeeping_task_user().
 *
 * On this board's half-duplex single-wire link that is ~3.4ms of wire time at
 * 230400 baud, twice a second — roughly 0.7% duty, which is why the sofleplus2
 * OLED is safe to run over a TRS-style interconnect.
 *
 * RPC_M2S_BUFFER_SIZE must exceed the payload; the default is 32
 * (split_common/transport.h:26-28), which would silently truncate it. 112
 * matches sofleplus2. Only TP_INFO_SYNC is declared here — sofleplus2 also
 * carries VIALRGB_DIRECT_SYNC / VIALRGB_INDICATOR_SYNC / OLED_SCREEN_B_SYNC for
 * its indicator-role subsystem, which is NOT ported (v1 keeps its own 72-LED
 * layer-colour indicator).
 *
 * NOTE the digitizer itself does not use these buffers at all: it syncs through
 * split_shmem->digitizer.report (transactions.c:811), and does not even reach
 * the wire when the trackpad half is master, which is the normal cabling. */
#define SPLIT_TRANSACTION_IDS_USER TP_INFO_SYNC
#define RPC_M2S_BUFFER_SIZE 112

/* Broadcast master matrix + encoder + trackpad activity to the slave so the
 * slave's last_input_activity_elapsed() tracks ALL input, not just its own
 * keypresses — otherwise one half sleeps while the other stays awake.
 * MUST be a config.h #define: a SPLIT_ACTIVITY_ENABLE = yes line in rules.mk is
 * not a recognised build feature and does nothing. */
#define SPLIT_ACTIVITY_ENABLE


/* =========================================================================
 * OLED + RGB SLEEP
 * ========================================================================= */

/* The keymap owns sleep via g_sleep_timeout, so the core's own OLED auto-off is
 * disabled and RGB_MATRIX_TIMEOUT is only the boot value for the runtime
 * g_rgb_matrix_timeout (rgb_matrix.h:282). The board config.h sets
 * OLED_TIMEOUT 120000; undef first so this override is unambiguous. */
#undef  OLED_TIMEOUT
#define OLED_TIMEOUT 0
#define RGB_MATRIX_TIMEOUT 60000

/* Runtime-configurable shared OLED+RGB sleep timeout, clamped to [1min, 30min].
 * 0/never is deliberately disallowed to protect the OLED from burn-in. */
#define DEFAULT_SLEEP_TIMEOUT_MS  60000    // 1 min
#define MIN_SLEEP_TIMEOUT_MS      60000    // 1 min
#define MAX_SLEEP_TIMEOUT_MS      1800000  // 30 min


/* =========================================================================
 * EEPROM / WEAR LEVELLING
 * ========================================================================= */

/* WITHDRAWN 2026-09-08, DISPROVEN ON HARDWARE 2026-09-09.
 *
 * A block here used to say "PIN THE FLASH SIZE - do not rely on the default", and
 * asserted "4MB CONFIRMED for sofleplus1". Both were wrong, and acting on them is what
 * broke v1.10's USB. The 4 MB figure was never measured, and the module under test
 * cannot use a store at 0x3E0000. The default is correct and safe - see the note
 * immediately below, and do not reinstate the pin.
 *
 * The one true statement in the old block is worth keeping: the store is placed at
 * (FLASH_SIZE - BACKING_SIZE), counted BACKWARDS from the assumed total, so the assumed
 * size and the real chip are decoupled and a wrong assumption silently relocates the
 * EEPROM rather than failing loudly. */
/* DO NOT PIN WEAR_LEVELING_RP2040_FLASH_SIZE. THIS WAS THE v1.10 USB BUG.
 *
 * Fixed and hardware-confirmed 2026-09-09. `#define WEAR_LEVELING_RP2040_FLASH_SIZE
 * (4 * 1024 * 1024)` used to sit here, and it is what made v1.10 enumerate ~10 s late,
 * intermittently fail with "Device Descriptor Request Failed" (Code 43), and never
 * answer Vial. Removing this one line fixed all three instantly.
 *
 * Mechanism: the store is placed BACKWARDS from the assumed total,
 * WEAR_LEVELING_RP2040_FLASH_BASE = FLASH_SIZE - BACKING_SIZE
 * (wear_leveling_rp2040_flash_config.h:31). Pinning 4 MB put it at
 * 0x400000-0x20000 = 0x3E0000; the default (PICO_FLASH_SIZE_BYTES = 2 MB,
 * lib/pico-sdk/.../boards/pico.h:73-74) puts it at 0x1E0000. 0x1E0000 works on the test
 * module and 0x3E0000 does not, so that module's flash is not usably 4 MB.
 *
 * Writes to an unusable address never read back, so the wear-levelling store never
 * validated and the WHOLE EEPROM was re-initialised on EVERY boot: ~31 KB of dynamic
 * keymap through the write log, forcing consolidation erases. That runs in
 * keyboard_setup() BEFORE the USB pull-up (quantum/main.c:38-45) -> the ~10 s silence;
 * writes spilling into keyboard_init() disable interrupts during flash programming right
 * inside the enumeration window -> the intermittent descriptor failures; and the dynamic
 * keymap never persisted -> Vial got nothing back.
 *
 * Leaving it unset is UNIVERSALLY SAFE: 0x1E0000 exists on every RP2040 module of 2 MB or
 * more, including client hardware whose flash size is unknown. Pinning 4 MB breaks any
 * module smaller than 4 MB. sofleplus2 has never pinned it, which is why sofleplus2 - and
 * v1.02b, which also does not pin it - always worked. */

/* Matches sofleplus2: 4:1 backing:logical ratio.
 * NOTE: changing wear-levelling geometry MOVES the EEPROM, so every stored
 * keymap is wiped on the update from v1.02b. Release-note this. */
#undef  WEAR_LEVELING_LOGICAL_SIZE
#define WEAR_LEVELING_LOGICAL_SIZE 32768   // 32KB
#undef  WEAR_LEVELING_BACKING_SIZE
#define WEAR_LEVELING_BACKING_SIZE 131072  // 128KB

/* Vial's dynamic keymap gets everything up to 0x7BFF; the keymap's raw-EEPROM
 * settings block lives above it, at 0x7C00-0x7C63.
 *
 * This is NOT how sofleplus2 does it, deliberately. sofleplus2 puts its raw
 * settings at 0x0FA0-0x1123, which lands INSIDE Vial's dynamic-macro area —
 * macros run from DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (~0x0A00 on this board,
 * after keymap + encoders + QMK settings + tap dance + combos + key overrides)
 * up to DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (nvm_dynamic_keymap.c:109-112). A large
 * macro would overwrite the sniper/OLED settings and vice versa; it is latent
 * there only because few users store more than ~1.4KB of macros.
 *
 * Capping at 0x7BFF costs 1KB of macro space (~29KB remains) and makes the
 * collision impossible. Keep this in sync with the address map in keymap.c.
 *
 * The #undef is required: ../../config.h:62 already defines this as 32767, and
 * redefining it without one is a constraint violation. GCC took the new value
 * anyway, so behaviour was already correct, but it emitted a
 * "'DYNAMIC_KEYMAP_EEPROM_MAX_ADDR' redefined" warning on every translation
 * unit -- noise that buries real warnings, which matters because
 * sofleplus1/rules.mk sets ALLOW_WARNINGS = yes. */
#undef  DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 0x7BFF

#ifdef VIAL_ENABLE
#define DYNAMIC_KEYMAP_MACRO_COUNT 32
#endif


/* =========================================================================
 * OS DETECTION
 * ========================================================================= */

/* Used ONLY for keycode differences (Cmd vs Alt in super-alt-tab, Cmd+D for
 * show-desktop) and the OLED indicator. It plays NO part in the PTP/mouse
 * decision — that is driven entirely by the host's PTP Input Mode feature
 * report at tmk_core/protocol/chibios/usb_main.c:309:
 *
 *     digitizer_send_mouse_reports = (mode != 0x03);
 *
 * Windows and Linux both send mode 0x03 (Precision Touchpad / hid-multitouch)
 * and get PTP; macOS never sends it, so the boot default of "true"
 * (digitizer_mouse_fallback.c:212) leaves it in mouse mode. Automatic.
 *
 * OS_DETECTION_KEYBOARD_RESET is deliberately NOT defined. It resets the MCU
 * on every USB bus reset — i.e. every KVM switch, hub transition and resume —
 * which in the field produced Windows "random reboots", immediate phantom
 * gestures, and Linux failing to enumerate. Do not enable it.
 *
 * OS_DETECTION_INITIAL_TIMEOUT is deliberately NOT set: it appears in several
 * older xcmkb config.h files but does not exist in QMK and has never had any
 * effect. These two do exist and match sofleplus2. */
#define OS_DETECTION_SINGLE_REPORT       // one report per detection cycle
#define OS_DETECTION_DEBOUNCE 300        // 300ms; core default is 250 (os_detection.c:36-37)


/* =========================================================================
 * IDENTITY
 * ========================================================================= */

/* Overrides keyboard.json's "SoflePLUS v1.02b" without touching the board
 * file — keymap config.h is last in QMK's config chain
 * (builddefs/build_keyboard.mk:509 vs :391-403). The product string is the
 * version marker: the device name in Windows tells you what is flashed. */
#undef  PRODUCT
#define PRODUCT "SoflePLUS v1.10"

/* PRODUCT_ID override — REQUIRED, not cosmetic.
 *
 * v1.10 changes the USB descriptor substantially. DIGITIZER_ENABLE makes
 * tmk_core/protocol.mk:13-14 set MOUSE_SHARED_EP = no, so the mouse leaves the
 * shared endpoint and takes its own interface, and the digitizer adds another:
 *
 *   v1.02b:  .0 Keyboard  .1 RawHID  .2 Shared[mouse+extrakey+NKRO]  .3 Console
 *   v1.10 :  .0 Keyboard  .1 RawHID  .2 Mouse  .3 Shared  .4 Console  .5 Digitizer
 *
 * Windows caches a composite device's interface layout against VID+PID+serial.
 * Vial force-defines the same serial for every build
 * (builddefs/build_vial.mk:15, "vial:f64c2b3c"), so keeping PID 0x0287 leaves
 * all three identical to v1.02b while the descriptor underneath has changed.
 * Windows then re-serves the cached 4-interface layout: the device enumerates
 * and shows the new product string, but Raw HID is not exposed and VIAL CANNOT
 * DETECT THE BOARD. Confirmed in the field 2026-09-08.
 *
 * A fresh PID forces a new device instance, so the descriptor is read anew.
 * This is also why tps65-510h works while tps65-510 would hit the same trap —
 * 510h carries its own 0x0288.
 *
 * PIDs in use on this VID (0xFC32):
 *   0x0287  sofleplus1 v1.02b, sofleplus2 tps65-510 / tps65-509h (board default)
 *   0x0288  sofleplus2 tps65-510h, tps65-509
 *   0x0289  sofleplus2 tps43-510
 *   0x028A  sofleplus1 v1.10   <-- this build
 *
 * Bonus: this also lets a sofleplus1 and a sofleplus2 be plugged into one host
 * at the same time, which the shared VID+PID+serial previously prevented.
 * Keep the "vial:" serial prefix intact — Vial detects by it. */
#undef  PRODUCT_ID
#define PRODUCT_ID 0x028A

/* Vial UID unchanged from v1.02b: it only tells the Vial app which layout to
 * load, and the layout is unchanged (still 10 rows x 6 cols). Keeping it means
 * a client's existing .vil still matches this firmware's key positions. */
