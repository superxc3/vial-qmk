# XCMKB SoflePlus 2 Series

Vial-QMK firmware for the XCMKB SoflePlus 2 family. Choose the correct variant for your board based on the comparison table below.

> **Latest firmware branch:** https://github.com/superxc3/vial-qmk/tree/xcmkb

## Variant Comparison

| Feature | `sofleplus2` | `sofleplus2ano` | `sofleplus2legendaryrgb` | `sofleplus2u` |
|---|---|---|---|---|
| **Target Batch** | Latest | Before mid-Dec 2024 | Before mid-Dec 2024 | After mid-Dec 2024 (Older Batch) |
| **Spinning Disc** | Remappable (Ano Disc) | Un-remappable arrow keys | Un-remappable arrow keys | Remappable arrow keys |
| **Right Trackpad** | ✓ | — | — | — |
| **RGB Per Key** | ✓ | ✓ | ✓ | ✓ |
| **RGB Underglow** | — | — | ✓ | ✓ |
| **Signature RGB** | — | — | — | ✓ |
| **Firmware Version** | — | — | — | v5.04 |

## Variant Details

### `sofleplus2` — Latest Batch
- Remappable ano disc
- Right trackpad support
- RGB per key only (no underglow)

### `sofleplus2ano` — Before Mid-Dec 2024
- Un-remappable arrow keys on the spinning disc
- RGB per key only
- No RGB underglow

### `sofleplus2legendaryrgb` — Before Mid-Dec 2024
- Un-remappable arrow keys on the spinning disc
- RGB per key
- RGB underglow supported

### `sofleplus2u` — After Mid-Dec 2024 (Older Batch), Firmware v5.04
- Remappable arrow keys on the spinning disc
- RGB per key + RGB underglow
- Signature RGB

---

## Firmware Compilation

For QMK Vial firmware compilation instructions, refer to:
https://xcmkb-docs.gitbook.io/doc/troubleshooting/wired-build#firmware-compilation
