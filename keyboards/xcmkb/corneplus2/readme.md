# CornePLUS2 by XCMKB

A split keyboard with 3x6 vertically staggered keys and 3 thumb keys, based on the
[Corne (crkbd)](https://github.com/foostan/crkbd) design by [foostan](https://github.com/foostan/), modified by
XCMKB with an Azoteq IQS5xx (TPS43) trackpad on the right half, OLEDs and Vial support.
Audio is not fitted on this board (see `keymaps/vialplus/config.h` for how to restore it).

**CornePLUS2 is CornePLUS addressed as a native RP2040 board.** The PCB is still a Pro Micro
footprint populated with an RP2040 Pro Micro replacement, but this board sets
`"processor": "RP2040"` / `"bootloader": "rp2040"` in `keyboard.json` and names every pin `GPxx`,
instead of `xcmkb/corneplus`'s `development_board: promicro` + `CONVERT_TO = rp2040_ce`.

### Why the converter had to go

`platforms/chibios/converters/promicro_to_rp2040_ce/_pin_defs.h` defines *only* the promicro
aliases (`D3 0U`, `F4 29U`, ...) and shadows `platforms/chibios/vendors/RP/_pin_defs.h` via
`#include_next`. In a converted build `GP0`..`GP29` therefore do not exist, so the trackpad,
I2C and split-serial config — all written in `GPxx` names, shared with `xcmkb/sofleplus2` —
fails to compile. The trackpad's RST line is on **GP14**, which is not on the Pro Micro
footprint at all and has no promicro alias, so the converter's naming layer cannot describe
this hardware even in principle.

The trade-off: the module pinout is now baked in. Moving to a kb2040/elite_pi means
re-deriving the `GPxx` numbers in `keyboard.json`, not flipping one `CONVERT_TO` line.

### Pin map (rp2040_ce module)

| Function | CornePLUS (promicro alias) | CornePLUS2 (native) |
|---|---|---|
| Matrix cols | `F4 F5 F6 F7 B1 B3` | `GP29 GP28 GP27 GP26 GP22 GP20` + `GP12` (col 6, disc directions) |
| Matrix rows | `D4 C6 D7 E6` | `GP4 GP5 GP6 GP7` |
| Encoder A / B | `B2` / `B6` | `GP23` / `GP21` |
| WS2812 data | `D3` | `GP0` |
| Audio | `B5` (PWM4 ch B) | *removed* |
| OLED + trackpad I2C1 | `D1` / `D0` | `GP2` (SDA) / `GP3` (SCL) |
| Split serial TX / RX | `B4` / `D2` | `GP8` / `GP1` (full duplex); **disc PCB: `GP9` / `GP1`** |
| Disc click (SW1) | — | `GP8` on the disc PCB, read as a DIP switch (`keymaps/510`) |
| Trackpad RDY | `B0` | `GP13` |
| Trackpad RST | *no alias exists* | `GP14` (soldered to the module pad) |
| USB VBUS detect | via converter `-DUSB_VBUS_PIN=19U` | `GP19` (explicit in `config.h`) |

Keyboard Maintainer: XCMKB
Hardware Supported: CornePLUS PCB (Pro Micro footprint), RP2040 Pro Micro-compatible module
Hardware Availability: [Original PCB & Case Data](https://github.com/foostan/crkbd)

Make example for this keyboard (after setting up your build environment):

```sh
make xcmkb/corneplus2:vialplus
```

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

The RP2040 module supports double-tap-reset to enter the bootloader (`RP2040_BOOTLOADER_DOUBLE_TAP_RESET`
comes from the `GENERIC_PROMICRO_RP2040` board config this board defaults to). Bootmagic Lite (hold
top-left key on the left half, or top-right key on the right half, while plugging in) is also available.

## RGB Matrix

The CornePLUS uses the RGB Matrix feature in place of RGB Light, giving per-key effects. In your keymap's `rules.mk`:

```make
RGBLIGHT_ENABLE = no
RGB_MATRIX_ENABLE = yes
```

And in your `config.h`:

```c

#ifdef RGB_MATRIX_ENABLE
#   define RGB_MATRIX_KEYPRESSES // reacts to keypresses
// #   define RGB_MATRIX_KEYRELEASES // reacts to keyreleases (instead of keypresses)
#   define RGB_DISABLE_WHEN_USB_SUSPENDED // turn off effects when suspended
#   define RGB_MATRIX_FRAMEBUFFER_EFFECTS
// #   define RGB_MATRIX_LED_PROCESS_LIMIT (RGB_MATRIX_LED_COUNT + 4) / 5 // limits the number of LEDs to process in an animation per task run (increases keyboard responsiveness)
// #   define RGB_MATRIX_LED_FLUSH_LIMIT 16 // limits in milliseconds how frequently an animation will update the LEDs. 16 (16ms) is equivalent to limiting to 60fps (increases keyboard responsiveness)
#    define RGB_MATRIX_MAXIMUM_BRIGHTNESS 150 // limits maximum brightness of LEDs to 150 out of 255. Higher may cause the controller to crash. 
#    define RGB_MATRIX_HUE_STEP 8
#    define RGB_MATRIX_SAT_STEP 8
#    define RGB_MATRIX_VAL_STEP 8
#    define RGB_MATRIX_SPD_STEP 10

/* Enable the animations you want/need.  You may need to enable only a small number of these because       *
 * they take up a lot of space.  Enable and confirm that you can still successfully compile your firmware. */
// RGB Matrix Animation modes. Explicitly enabled
// For full list of effects, see:
// https://docs.qmk.fm/#/feature_rgb_matrix?id=rgb-matrix-effects
#    define ENABLE_RGB_MATRIX_ALPHAS_MODS
#    define ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
#    define ENABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
#    define ENABLE_RGB_MATRIX_BREATHING
#    define ENABLE_RGB_MATRIX_BAND_SAT
#    define ENABLE_RGB_MATRIX_BAND_VAL
#    define ENABLE_RGB_MATRIX_BAND_PINWHEEL_SAT
#    define ENABLE_RGB_MATRIX_BAND_PINWHEEL_VAL
#    define ENABLE_RGB_MATRIX_BAND_SPIRAL_SAT
#    define ENABLE_RGB_MATRIX_BAND_SPIRAL_VAL
#    define ENABLE_RGB_MATRIX_CYCLE_ALL
#    define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#    define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
#    define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#    define ENABLE_RGB_MATRIX_CYCLE_OUT_IN
#    define ENABLE_RGB_MATRIX_CYCLE_OUT_IN_DUAL
#    define ENABLE_RGB_MATRIX_CYCLE_PINWHEEL
#    define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#    define ENABLE_RGB_MATRIX_DUAL_BEACON
#    define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#    define ENABLE_RGB_MATRIX_RAINBOW_PINWHEELS
#    define ENABLE_RGB_MATRIX_RAINDROPS
#    define ENABLE_RGB_MATRIX_JELLYBEAN_RAINDROPS
#    define ENABLE_RGB_MATRIX_HUE_BREATHING
#    define ENABLE_RGB_MATRIX_HUE_PENDULUM
#    define ENABLE_RGB_MATRIX_HUE_WAVE
#    define ENABLE_RGB_MATRIX_PIXEL_RAIN
#    define ENABLE_RGB_MATRIX_PIXEL_FLOW
#    define ENABLE_RGB_MATRIX_PIXEL_FRACTAL
// enabled only if RGB_MATRIX_FRAMEBUFFER_EFFECTS is defined
#    define ENABLE_RGB_MATRIX_TYPING_HEATMAP
#    define ENABLE_RGB_MATRIX_DIGITAL_RAIN
// enabled only of RGB_MATRIX_KEYPRESSES or RGB_MATRIX_KEYRELEASES is defined
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_WIDE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_CROSS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTICROSS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_NEXUS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS
#    define ENABLE_RGB_MATRIX_SPLASH
#    define ENABLE_RGB_MATRIX_MULTISPLASH
#    define ENABLE_RGB_MATRIX_SOLID_SPLASH
#    define ENABLE_RGB_MATRIX_SOLID_MULTISPLASH
#endif
```

After this is done, you should be able to use the normal RGB keycodes, but you'll see the RGB Matrix effects in use, giving a much better experience.
