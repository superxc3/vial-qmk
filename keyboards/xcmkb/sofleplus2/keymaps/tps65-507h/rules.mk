
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
VIAL_KEYBOARD_UID = {0x4F,0x8B,0x21,0xD6,0x73,0xEA,0x9C,0x05}
VIAL_USER_CONFIG_ENABLE = yes

QMK_SETTINGS = yes
COMBO_ENABLE = yes
TAP_DANCE_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
########### PLUS ###########

# macOS dual-mode: mouse fallback for non-PTP hosts (macOS/Linux)
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = digitizer

# v5.07: IQS5xx RDY (GP13) edge-gated reads + RST (GP14) hard reset/recovery
SRC += trackpad_rdy_rst.c

########### OS DETECTION ###########
OS_DETECTION_ENABLE = yes

########### DIP SWITCH ###########
DIP_SWITCH_ENABLE = yes

########### VIAL 0.7.4 ###########
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes

