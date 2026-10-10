# CornePLUS2 — working notes

Scoped to `keyboards/xcmkb/corneplus2`. Read this before changing anything in this
directory. Keep the **Status log** at the bottom current — it is the progress trace.

## What this board is

Corne (3x6 + 3 thumb) split, RP2040, with an **Azoteq IQS5xx TPS43 trackpad on the
right half**, dual OLEDs, per-key RGB, one encoder per half, and Vial.

Relation to the neighbouring trees — get this right before touching anything:

| Tree | State | Trackpad |
|---|---|---|
| `keyboards/crkbd` | untracked leftover, superseded by `d506dfd591` | Pimoroni **trackball**, no digitizer |
| `keyboards/xcmkb/corneplus` | frozen; kept as the last known-good artifact | TPS43 config present but **cannot compile** |
| `keyboards/xcmkb/corneplus2` | **active** | TPS43, right half |
| `keyboards/xcmkb/sofleplus2` | the reference implementation | TPS43 / TPS65, right half |

Trackpad config here is meant to stay in lockstep with `sofleplus2`. When the
sofleplus2 keymaps gain a digitizer fix, port it here rather than re-deriving it.

## Hard rules

1. **Never add `CONVERT_TO` back.** The converter ships its own `_pin_defs.h` that
   shadows `platforms/chibios/vendors/RP/_pin_defs.h` via `#include_next`, so
   `GP0`..`GP29` become undefined and every `GPxx` config borrowed from sofleplus2
   fails to compile at its first *use* (the `#define` itself looks fine, so the error
   lands far from the cause). This is exactly why `corneplus` is stuck. Beyond that,
   the trackpad RST is on **GP14**, which is not on the Pro Micro footprint and has no
   promicro alias — the converter cannot describe this hardware at all.
2. **Name every pin `GPxx`.** No `B5`/`D3`/`F4` anywhere, including comments.
3. **Do not build.** The user compiles and flashes. Verify statically instead:
   check `#ifdef` guards, that referenced identifiers exist, and that call sites match.
   Say plainly that it was not compiled.
4. **No Claude attribution in commit messages.**

## Pin map (rp2040_ce module, addressed natively)

Two PCB revisions share this tree. They differ only on `GP8`/`GP9`/`GP12`; the
**keymap** picks the revision: `510` = disc PCB, `vialplus` = original PCB.

| Function | GP | Note |
|---|---|---|
| Matrix cols | `GP29 GP28 GP27 GP26 GP22 GP20` + `GP12` | col 6 (`GP12`) = disc directions; open on the original PCB |
| Matrix rows | `GP4 GP5 GP6 GP7` | |
| Encoder A / B | `GP23` / `GP21` | one per half, same pins |
| WS2812 data | `GP0` | 54 LEDs, 27+27 |
| I2C1 SDA / SCL | `GP2` / `GP3` | OLED **and** trackpad share this bus @ 1 MHz |
| Split serial TX / RX | `GP8` / `GP1` original PCB; **`GP9` / `GP1` disc PCB** | full duplex, two TRRS conductors |
| Disc click SW1 | `GP8` (disc PCB) | DIP switch, replayed onto `[3][0]` / `[7][0]` |
| Trackpad RDY | `GP13` | comm-window strobe, edge-gated |
| Trackpad RST | `GP14` | active low; soldered to the module pad |
| USB VBUS detect | `GP19` | explicit; the converter used to inject this |
| *free* | `GP9` (original PCB only) | freed when audio was removed |

**Pin-reading trap (cost a round-trip, 2026-09-25):** the first pin list for the
disc PCB came back mirrored — `GP23`/`GP16` instead of `GP8`/`GP12`. `GP23`↔`GP8`
are the same position on opposite long edges of the Pro Micro footprint, and
`GP16`↔`GP12` are opposite ends of the bottom edge. If a pin read off the PCB
collides with something already assigned, check the mirror position first.

Old promicro aliases, if you need to read `corneplus`: `D3`=GP0, `D2`=GP1, `D1`=GP2,
`D0`=GP3, `D4`=GP4, `C6`=GP5, `D7`=GP6, `E6`=GP7, `B4`=GP8, `B5`=GP9, `D5`=GP12,
`B0`=GP13, `B3`=GP20, `B6`=GP21, `B1`=GP22, `B2`=GP23, `F7`=GP26, `F6`=GP27,
`F5`=GP28, `F4`=GP29. `GP14` has no alias.

## Layout

`BOARD` is not set and defaults to `GENERIC_PROMICRO_RP2040` (same as sofleplus2).
That board supplies `RP2040_BOOTLOADER_DOUBLE_TAP_RESET` and I2C1 defaults, and —
unlike the converter's `QMK_PM2040` — has no `halconf.h`, so `HAL_USE_ADC`/`HAL_USE_SPI`
are off. Neither is used here.

Handedness is `EE_HANDS`. The trackpad half is selected by `DIGITIZER_RIGHT` +
`SPLIT_DIGITIZER_ENABLE`; `trackpad_rdy_rst.c` skips the pad-less half via
`is_keyboard_left()`, so the floating RDY pin is never read there.

Matrix is 8x7 (`MATRIX_COLS` 7 since 2026-09-25). Disc keys, as in sofleplus2:

| | left half | right half |
|---|---|---|
| left / up / right / down | `[0][6] [1][6] [2][6] [3][6]` | `[4][6] [5][6] [6][6] [7][6]` |
| click (DIP index 0 / 1) | `[3][0]` | `[7][0]` |

The click slots are thumb-row positions with no switch (thumbs are cols 3-5).
Both halves are configured identically because the PCB is reversible; a half
without a disc just reads idle. Layout macro: `LAYOUT_split_3x6_3_disc` (52 keys:
the 42 of `LAYOUT_split_3x6_3`, then left disc L/U/R/D/click, right disc
L/U/R/D/click). `LAYOUT_split_3x6_3` / `_3x5_3` are unchanged, so `vialplus`
still builds; its `vial.json` only had `cols` bumped to 7.

Build: `make xcmkb/corneplus2:vialplus`

## Status log

### 2026-09-07 — board created, converted to native RP2040
Forked from `xcmkb/corneplus` and converted. Not compiled.
- `keyboard.json`: `development_board: promicro` → `processor`/`bootloader` RP2040;
  all matrix/encoder/ws2812 pins renamed to `GPxx`; PID `0x0001` → `0x0002`.
- `rules.mk`: `CONVERT_TO = rp2040_ce` removed.
- `config.h`: `USB_VBUS_PIN GP19` declared explicitly (the converter injected
  `-DUSB_VBUS_PIN=19U`; dropping the converter dropped it). Dead `USE_SERIAL_PD2`
  removed. New `VIAL_KEYBOARD_UID`.
- `OS_DETECTION_KEYBOARD_RESET` turned **off**, matching `d216a0e7f6` on sofleplus2
  (it caused the Windows reboot-on-connect, phantom gestures, Linux non-enumeration).
  `OS_DETECTION_ENABLE` stays on — the keymap uses it.
- Audio removed at the user's request; `HAL_USE_PWM` / `RP_PWM_USE_PWM4` came out with
  it (nothing else uses PWM — ws2812 is PIO). The old `RP_PWM_USE_TIM1` line was a
  no-op symbol that does not exist in ChibiOS' RP2040 port. `GP9` is now free.
- Verified statically: both JSON files parse; no AVR pin name in any active `#define`;
  21 assigned GP pins, no conflicts; halconf/mcuconf both use `#include_next`.
- User confirmed: encoder `GP21`/`GP23` correct, RGB on `GP0` correct, trackpad on the
  right half correct, RDY `GP13` / RST `GP14` correct, split link full duplex.

### 2026-09-25 — disc (4-way + click) on `keymaps/510`, EEPROM fix
Not compiled. Disc PCB pins per the user: SW1 click `GP8`, direction line `GP12`
(row-vs-column unknown to the user — wired as a column, see Open), split TX
moved `GP8`→`GP9`.
- `keyboard.json`: `GP12` appended as col 6; new `LAYOUT_split_3x6_3_disc`.
- `corneplus2.c`: `hand_swap_config` extended to 7 columns (swap-hands is off).
- `510/config.h`: TX `GP9`; `DIP_SWITCH_PINS { GP8 }` + `SPLIT_DIP_SWITCH_ENABLE`;
  EEPROM block (below). `510/rules.mk`: `DIP_SWITCH_ENABLE = yes`.
- `510/keymap.c`: `dip_switch_update_user()` replays index 0/1 onto `[3][0]`/`[7][0]`
  (`KEY_EVENT`, no `| 1` — see root CLAUDE.md); all 10 layers moved to the disc
  layout (L0 = arrows + `MS_BTN1`, L1-3 transparent, L4-9 `XXXXXXX`).
- `510/vial.json`: `cols` 7; both discs drawn under the outer columns, each behind
  its own toggle, "no left disc" / "no right disc" (layout options 2 / 3).
- **EEPROM (pre-existing, found here):** no `WEAR_LEVELING_LOGICAL_SIZE`, so the
  EEPROM was the RP2040 default 4 KB while `keymap.c` stores settings up to 0x1123
  — colours, indicators, OLED labels and sleep timeout could never be saved, and
  Vial's macro buffer (default end = last EEPROM byte) ran over 0x0FA0.., so a
  macro reset would zero the trackpad/RGB/OLED settings. Fixed: sofleplus2's 32 KB /
  128 KB wear-leveling sizes, `DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 0x0F9F` (~1.5 KB of
  macros left), sleep timeout 0x10DC→0x0FB8 (it sat inside `indicator_config_t`,
  same bug sofleplus2 fixed 2026-09-02), `_Static_assert`s on the whole map.
  sofleplus2 deliberately left untouched (user's call).
- Verified statically: both JSON files parse; 52 args in every layer; every
  `LAYOUT_split_3x6_3_disc` position appears exactly once in `510/vial.json` and
  vice versa; KLE-computed disc positions overlap nothing; `GP8`/`GP9`/`GP12`
  each have exactly one owner.

### 2026-10-10 — RGB split fix, default indicators, underglow in vial-web
Not compiled.
- `keyboard.json`: `rgb_matrix.split_count [27, 27]` added. It was only under
  `rgblight` (disabled), so `RGB_MATRIX_SPLIT` was undefined: each half rendered all
  54 LEDs on a 27-LED strip (right half mirrored the left's indices, right-half
  indicators/per-key colours never showed), no RGB config sync to the slave, and
  reactive effects on the master only. Board-level, so `vialplus` gets it too.
- `510/keymap.c`: default indicators — Caps = LED 14 (left GUI thumb `[3][3]`),
  roles 1-9 = the 12 underglow LEDs (0-5, 27-32). Boards that already saved an
  indicator config keep it (EEPROM magic valid) until reassigned.
- vial-web (`superxc3/vial-web`, not vial-gui): LEDs with no matrix position were
  dropped in `reload_vialrgb_direct_leds()`; now kept and drawn as numbered squares
  under the keymap in the per-key and indicator tabs (`build_led_only_keys()` in
  `editor/rgb_configurator.py`).
- DIP core re-checked against upstream (local commits `769ac6d7a2`, `21956083a9`):
  per-hand indexing and the transport merge are sound for this board. Extra
  behaviour noted: with the other half unplugged, the master's own click is only
  processed on reconnect attempts (`SPLIT_CONNECTION_CHECK_TIMEOUT`, 500 ms default).

### 2026-10-10 — per-LED indicator colours (VIALRGB 0x48)
Not compiled. Each LED in a role can now have its own colour.
- Core `quantum/vialrgb.c/h`: new GET/SET sub-command 0x48 (chunks of 9 LEDs;
  GET reply starts with magic 0x49). Hooks are weak; the GET stub returns false so
  keymaps without it keep echoing, and hosts fall back to one colour per role.
  **Only `corneplus2/510` implements it so far** — sofleplus2-lineage keymaps still
  behave the old way until ported.
- `510/keymap.c`: `g_indicator_led_colors[10][54][3]`, rendered instead of the role
  colour; a 0x46 SET repaints whole roles (old-host compatibility); EEPROM
  0x1130 magic 'IC' + 1620 bytes at 0x1132 (asserted below the 32 KB end); slave
  sync via new RPC `VIALRGB_INDICATOR_LED_SYNC` (one role per packet, 83 bytes ≤
  `RPC_M2S_BUFFER_SIZE` 112; dirty roles first, then one role per second).
- vial-web: indicator tab swatch is a brush (click adds / recolours / removes),
  "Paint All" button; `.vil` and local saves carry `led_colors`. Tested against a
  Python model of the firmware (new and echo-only).

### 2026-10-10 — wake-up "Aurora tide" animation
Not compiled. `510/keymap.c` (`AURORA_WAKE_ENABLE` in config.h): ~5 s blue/cyan
aurora on plug-in or PC start-up, then a left-to-right dissolve into the user's
effect. Trigger = fresh USB enumeration (CONFIGURED not from SUSPEND), or 3 s of
power without enumeration; 120 s replay guard swallows the BIOS→OS second
enumeration; wake from sleep does not play. Both halves boot dark; master sends
its sync_timer start over RPC `AURORA_SYNC`. 500 ms dark settle for the trackpad
baseline (shared rail). Drawn in `rgb_matrix_indicators_advanced_user`, so it
never changes saved mode/brightness and does not play with RGB off; indicators
are paused while it runs. Preview: Python port in the session scratchpad.
**First flash: check for trackpad phantom touches right after plug-in.**
**Reworked same day (concept A + sync fix):** two broad tides left -> right, ~4.5 s,
second tide washes the user's effect in. Timing is now each half's own
`timer_read32()`: plug-in runs from power-on on both halves with no message (the
old sync_timer start-time RPC let the slow trackpad half miss it when the cable
was on the left -- the start time could arrive before the slave's first timer
sync, and the negative elapsed wrapped to "finished"). PC start-up sends
"elapsed ms" instead; the slave converts it in its main loop, not the ISR.

### 2026-10-10 — sleep sync, phantom fix, trackpad rotation (510)
Not compiled.
- `SPLIT_ACTIVITY_ENABLE` added (config.h #define): one half sometimes slept alone
  because each half only counted its own input. sofleplus2 has had it since v5.06.
- Phantom-click fix enabled: `AZOTEQ_IQS5XX_IDLE_RESEED` (+ `_MS 300000`) and
  `AZOTEQ_IQS5XX_CONFIDENCE_GATE`. The code is in the shared driver but opt-in;
  every other 510 keymap set them, corneplus2 did not.
- `AZOTEQ_IQS5XX_ROTATION_90` -> `_270`: cursor was inverted on both axes on
  hardware. Resolves the old "ROTATION_90 untested" open item.
- `SPLIT_TRANSPORT_MIRROR` added: reactive RGB on the slave now sees master-half
  presses (before: slave-half press showed on both, master-half press on master only).
- `apply_scroll_invert()` ported from sofleplus2 v5.11: the Vial scroll-direction
  settings / SCROLL_DIR_V/H keys were stored but never applied (dead on corneplus2).
- Sync audit vs sofleplus2 tps43-510 (2026-10-10): split defines identical except
  HAPTIC (not fitted) and WATCHDOG (corneplus2 only); same RPC set + cadences
  (direct colours 16 ms, indicators 3 s/on change, TP_INFO 500 ms) plus
  VIALRGB_INDICATOR_LED_SYNC and AURORA_SYNC. Phantom handling identical: driver
  opt-ins, same trackpad_rdy_rst.c, OS_DETECTION_KEYBOARD_RESET off, console off.

### 2026-10-10 — v5.11: `keymaps/511`
Copy of 510 (everything above) + disc click in the matrix tester (`vmatrix_note` →
`via_switch_matrix_extra_user`) + PRODUCT "CornePLUS2 TPS43 v5.11". Knobs show in
the matrix tester via core 0x54 (no keymap code). Released alongside the sofle 511s;
see root CLAUDE.md "v5.11 host protocol additions". Not compiled.

## Open / unverified

- **Never flashed.** No part of this board has run on hardware yet.
- **Disc, first flash:** (1) press each direction on each half — the row→direction
  order (row 0..3 = L/U/R/D) is copied from sofleplus2, not read off this PCB, and
  on a reversible PCB the right-hand disc may come out left/right mirrored; fix by
  swapping keycodes + `vial.json` positions, not wiring. (2) If *no* direction
  registers at all, `GP12` is a row, not a column: move it to `matrix_pins.rows`
  (5 rows/half → `MATRIX_ROWS` 10) and re-derive the layout; a wrong guess reads
  as dead keys, never as phantom presses. (3) Click on the slave half inherits the
  two known DIP-path defects in the root CLAUDE.md (no debounce, no last-known-good
  hold) — not fixed here by design.
- First boot after the EEPROM change resets EEPROM once (new wear-leveling
  geometry) — expected.
- `keymaps/vialplus` still has the 4 KB EEPROM bug (same settings map, no
  wear-leveling defines); only `510` was fixed.
- `AZOTEQ_IQS5XX_ROTATION_90` axis mapping is untested (inherited TODO from
  `corneplus`). sofleplus2's `ROTATION_180` swaps `SWIPE_LEFT_KC`/`RIGHT_KC`;
  its `ROTATION_270` sets `DIGITIZER_SWIPE_X_INVERT`. Which — if either — `ROTATION_90`
  needs is unknown. Confirm cursor and swipe direction first, then adjust.
- RGB: no default layer-indicator LEDs assigned; the physical LED map has not been
  walked (`keymap.c` ~line 437).
- Not tracked in git yet.

### 2026-10-11 — DIP fix, bootmagic corners, corner layout note
- Core DIP fix (root CLAUDE.md "Split DIP switch") applies to 510 and 511 alike.
- Bootmagic (511): `[0][0]` top-left on the left half, `[4][0]` top-right on the right.
  These are the outer top keys of LAYOUT_split_3x6_3 (42 keys). The `[0][1]`/`[4][1]`
  defaults in post_config.h are the outer keys of the 36-key LAYOUT_split_3x5_3 instead,
  where `[0][0]` does not exist. Use `[0][1]`/`[4][1]` only if a board ever runs 3x5_3.
- Bootmagic does not erase EEPROM (bootmagic_reset_eeprom() is a no-op in 511 keymap.c).
- **36-key vs 42-key is a separate firmware build.** 511 is LAYOUT_split_3x6_3 (42), and
  the Bootmagic corners are compile-time, so a 36-key board (LAYOUT_split_3x5_3) needs its
  own keymap with `[0][1]`/`[4][1]`. Don't make one build accept both corners: on 42 keys
  `[0][1]` is the second key (Q on the default layout), so plugging in with Q held would
  enter the bootloader.
- Reset layout (vial-web Keymap tab, "Reset layout...") sends VIA 0x06, the firmware's
  `dynamic_keymap_reset()`. It restores layers and encoders only.
- Work for v5.11 is on local branches (`vial-qmk: feature/v511`, `vial-web:
  feature/v511-reset-layout`), changes uncommitted until reviewed.

