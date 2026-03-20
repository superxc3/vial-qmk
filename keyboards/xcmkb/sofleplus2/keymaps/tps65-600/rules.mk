
############ CAPS WORD ############
# read more here https://docs.qmk.fm/features/caps_word, but it may relates to COMMAND_ENABLE = no, already disabled by default so no conflict here.
CAPS_WORD_ENABLE = yes
TRI_LAYER_ENABLE = yes
LEADER_ENABLE = yes


########### VIA VIAL ###########

VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
VIAL_ENCODERS_ENABLE = yes
VIAL_KEYBOARD_UID = {0x7E,0x3A,0xD5,0x61,0xB0,0x94,0x2F,0xC8}
VIAL_USER_CONFIG_ENABLE = yes

QMK_SETTINGS = yes
COMBO_ENABLE = yes
TAP_DANCE_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
########### PLUS ###########

# macOS dual-mode: mouse fallback for non-PTP hosts (macOS/Linux)
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = digitizer

########### OS DETECTION ###########
OS_DETECTION_ENABLE = yes

########### DIP SWITCH ###########
DIP_SWITCH_ENABLE = yes

########### VIAL 0.7.4 ###########
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes

