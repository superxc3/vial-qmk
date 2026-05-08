# Gesture Support: Windows PTP vs macOS Mouse Fallback

## Windows PTP Mode

Windows receives raw finger coordinates via the Precision Touchpad (PTP) digitizer interface. The OS handles all gesture recognition natively.

| Gesture | Status | Details |
|---|---|---|
| 1-finger move | Supported | OS reads absolute finger coordinates |
| 1-finger tap | Supported | OS detects quick touch and release |
| 1-finger tap + hold + drag | Supported | OS detects hold after tap |
| 2-finger tap | Supported | Right click |
| 2-finger scroll (vertical) | Supported | Smooth pixel-level scrolling with momentum |
| 2-finger scroll (horizontal) | Supported | Smooth pixel-level scrolling |
| 2-finger pinch zoom | Supported | OS measures distance change between fingers |
| 2-finger rotate | Supported | OS measures angle change between fingers |
| 3-finger tap | Supported | Configurable in Windows Settings |
| 3-finger swipe up | Supported | Task View (default) |
| 3-finger swipe down | Supported | Show Desktop (default) |
| 3-finger swipe left/right | Supported | Switch apps (default) |
| 4-finger tap | Supported | Configurable in Windows Settings |
| 4-finger swipe up/down | Supported | Configurable |
| 4-finger swipe left/right | Supported | Switch desktops (default) |
| Momentum/inertia scrolling | Supported | OS applies physics-based momentum |

## macOS Mouse Fallback Mode

macOS does not support Microsoft's PTP protocol. The firmware converts raw touch data into standard mouse HID reports using a software gesture state machine.

| Gesture | Status | Details |
|---|---|---|
| 1-finger move | Supported | Firmware computes dx/dy deltas, sends as mouse x/y |
| 1-finger tap | Supported | State machine detects quick touch and release |
| Double-tap + drag | Supported | Tap then touch-and-hold within timeout |
| 2-finger tap | Supported | Right click via mouse button 2 |
| 2-finger scroll (vertical) | Supported | Firmware sends discrete scroll ticks |
| 2-finger scroll (horizontal) | Supported | Firmware sends discrete scroll ticks |
| 3-finger tap | Supported | Middle click via mouse button 3 |
| 3-finger swipe up | Supported | Sends Ctrl+Up (Mission Control) |
| 3-finger swipe down | Supported | Sends Ctrl+Down (App Expose) |
| 3-finger swipe left | Supported | Sends Ctrl+Left/Right (Switch Desktop) |
| 3-finger swipe right | Supported | Sends Ctrl+Right/Left (Switch Desktop) |
| 2-finger pinch zoom | Not supported | See explanation below |
| 2-finger rotate | Not supported | See explanation below |
| 4-finger gestures | Not supported | See explanation below |
| Momentum/inertia scrolling | Partial | macOS adds some momentum to mouse wheel but less smooth than native trackpad |

## Why Certain Gestures Cannot Work on macOS

### The Fundamental Constraint

Mouse HID reports only have these output channels:

| Field | Type | Purpose |
|---|---|---|
| x | int8 (-127 to 127) | Relative cursor movement X |
| y | int8 (-127 to 127) | Relative cursor movement Y |
| buttons | 8 bits | Up to 8 on/off button states |
| v | int8 (-127 to 127) | Vertical scroll ticks (discrete) |
| h | int8 (-127 to 127) | Horizontal scroll ticks (discrete) |

Everything the firmware sends to macOS must fit into these fields. There is no way to express complex multi-finger spatial data through this interface.

### Pinch-to-Zoom

Pinch zoom requires the OS to see the changing distance between two finger positions over time. Mouse HID has no zoom axis. The IQS5xx hardware can detect zoom and map it to mouse buttons 7/8, but macOS interprets those as browser forward/back, not zoom. Real pinch-to-zoom requires a multitouch HID descriptor where the OS receives two absolute finger coordinates and computes the pinch itself.

### Rotation

Same limitation as zoom. The OS needs to track the changing angle between two finger positions over time. There is no rotation axis in mouse HID.

### 4-Finger Gestures

The firmware could detect 4 fingers and send keycodes (like 3-finger swipe does), but macOS 4-finger gestures (Launchpad, Show Desktop, etc.) are tied to the native multitouch subsystem and have no equivalent keyboard shortcuts.

### Smooth Pixel-Level Scrolling

Mouse scroll reports send discrete ticks where each tick moves approximately 3 lines. Native macOS trackpad scrolling is pixel-precise with natural momentum and inertia physics. Mouse wheel events receive a different, less smooth momentum behavior from macOS.

## How It Works Internally

### Windows PTP

The firmware sends raw `(x, y, contact_id, tip)` for up to 5 fingers at high frequency via the digitizer USB interface. Windows receives these absolute coordinates and reconstructs all gestures natively. The firmware does no gesture processing.

### macOS Mouse Fallback

1. When the keyboard connects, `digitizer_send_mouse_reports` defaults to `true` (mouse fallback mode)
2. If Windows sends a PTP feature report, the flag switches to `false` (PTP digitizer mode)
3. macOS never sends this feature report, so mouse fallback stays active
4. The firmware runs a software state machine that reads raw IQS5xx touch data and converts it to mouse reports
5. Reports are sent directly via `host_mouse_send()` to bypass macOS suppression of mouse motion from devices with touchpad HID descriptors

### Why Not Apple-Compatible Multitouch?

In theory, if the firmware exposed an Apple-compatible multitouch HID descriptor, macOS could receive raw finger data and do native gestures. However:

- Apple's multitouch HID protocol is proprietary and undocumented
- It would require a completely separate USB interface and descriptor
- It would likely break Windows PTP compatibility unless dynamically switched via OS detection
- Commercial trackpad manufacturers (Kensington, Logitech) use the same approach: raw multitouch for Windows PTP, firmware-processed mouse reports for macOS

## Tuning Parameters

All parameters can be overridden per-keymap in `config.h`.

| Parameter | Default | Description |
|---|---|---|
| `DIGITIZER_MOUSE_SCALE` | 64 | Cursor speed (fixed-point 8.8, 256 = 1.0x) |
| `DIGITIZER_SCROLL_SCALE` | 10 | Scroll speed (fixed-point 8.8, 256 = 1.0x) |
| `DIGITIZER_MOUSE_JITTER_THRESHOLD` | 1 | Ignore touch deltas at or below this value |
| `DIGITIZER_SCROLL_INVERT` | false | Set true for natural (macOS-style) scrolling |
| `DIGITIZER_MOUSE_TAP_DETECTION_TIMEOUT` | 200ms | Max duration for a touch to count as a tap |
| `DIGITIZER_MOUSE_TAP_DISTANCE` | 25 | Max finger movement during a tap |
| `DIGITIZER_MOUSE_SWIPE_TIMEOUT` | 1000ms | Max duration for a 3-finger swipe gesture |
| `DIGITIZER_MOUSE_SWIPE_DISTANCE` | 500 | Min distance to trigger a swipe |
| `DIGITIZER_SWIPE_UP_KC` | KC_LGUI | Keycode sent on 3-finger swipe up |
| `DIGITIZER_SWIPE_DOWN_KC` | KC_ESC | Keycode sent on 3-finger swipe down |
| `DIGITIZER_SWIPE_LEFT_KC` | Mouse Button 3 | Keycode sent on 3-finger swipe left |
| `DIGITIZER_SWIPE_RIGHT_KC` | Mouse Button 4 | Keycode sent on 3-finger swipe right |
