
############ CAPS WORD ############
# read more here https://docs.qmk.fm/features/caps_word, but it may relates to COMMAND_ENABLE = no, already disabled by default so no conflict here.
CAPS_WORD_ENABLE = yes


############ SENTENCE CASE ############	https://getreuer.info/posts/keyboards/sentence-case/
##SRC += features/sentence_case.c #will only recall if able to toggle per word

########### VIA VIAL ###########

# VIA VIAL
VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
VIAL_ENCODERS_ENABLE = yes

# Reduce size of Vial
QMK_SETTINGS = yes
COMBO_ENABLE = yes
TAP_DANCE_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes

# Vial 0.7.4 features, ported from sofleplus2 v5.10. Hardware-independent.
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes


########### OS SWITCH ###########
# Used for keycode differences (Cmd vs Alt in super-alt-tab, Cmd+D for
# show-desktop), the OLED OS badge, and the OS_DETECTION_TOGGLE keycode.
# NOT used for the PTP/mouse decision — see the note in config.h.
OS_DETECTION_ENABLE = yes
KEYBOARD_HOOK_ENABLE = yes
SRC += quantum/os_detection.c


########### TRACKPAD — PTP DIGITIZER ###########
#
# v1.10 clean break: the legacy relative-mouse driver is gone, replaced by the
# PTP digitizer stack already shipping on sofleplus2 v5.10. This block is a
# 1:1 copy of tps65-510/rules.mk:25-26 plus the board-level digitizer lines.
#
# BOTH are needed, and this is not a contradiction:
#
#   DIGITIZER_ENABLE + azoteq_iqs5xx  reads the sensor and sends PTP contacts.
#   POINTING_DEVICE_ENABLE + digitizer registers digitizer_pointing_device_driver
#                                     (digitizer_mouse_fallback.c:220) so the
#                                     pointing pipeline runs too.
#
# The pointing pipeline is what makes pointing_device_task_user() fire, which is
# where TRACKPAD_TOGGLE actually gates the trackpad. Without it that keycode
# would change the OLED readout and nothing else. The board rules.mk already
# sets POINTING_DEVICE_ENABLE = yes; it is repeated here so this file states the
# whole configuration in one place rather than relying on inheritance.
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = digitizer

DIGITIZER_ENABLE = yes
DIGITIZER_DRIVER = azoteq_iqs5xx

# Kept ON. The digitizer's macOS mouse-fallback path calls host_mouse_send()
# directly (digitizer_mouse_fallback.c:587), bypassing QMK's mousekey OR, so the
# DIP switch's KC_BTN1 is merged back in by digitizer_pre_send_user() in
# keymap.c. Matches sofleplus2.
MOUSEKEY_ENABLE = yes


########### CONSOLE ###########
# Overrides CONSOLE_ENABLE = yes in sofleplus1/rules.mk, for this keymap only.
# Both shipping sofleplus2 keymaps do the same (tps65-510h/rules.mk:39).
#
# The console costs a whole USB interface. With DIGITIZER_ENABLE the mouse also
# leaves the shared endpoint (tmk_core/protocol.mk:13-14), so leaving console on
# would give this build six interfaces:
#   .0 Keyboard  .1 RawHID  .2 Mouse  .3 Shared  .4 Console  .5 Digitizer
# Turning it off drops it to five. Console output was only ever useful for the
# phantom probe, which is not in this build, and on the Debian client's dmesg the
# interfaces that failed to bind were always the last-probed ones — one fewer is
# strictly less exposure. Set back to yes if you ever need hid_listen output.
CONSOLE_ENABLE = no
