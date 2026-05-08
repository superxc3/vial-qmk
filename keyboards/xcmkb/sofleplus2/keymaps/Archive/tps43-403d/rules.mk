
############ CAPS WORD ############
# read more here https://docs.qmk.fm/features/caps_word, but it may relates to COMMAND_ENABLE = no, already disabled by default so no conflict here.
CAPS_WORD_ENABLE = yes
TRI_LAYER_ENABLE = yes
LEADER_ENABLE = yes


########### VIA VIAL ###########

# VIA VIAL
VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
VIAL_ENCODERS_ENABLE = yes

VIAL_KEYBOARD_UID = {0x12,0x38,0x7D,0x9C,0x1C,0x0E,0x58,0x43}
VIAL_USER_CONFIG_ENABLE = yes


# Reduce size of Vial
QMK_SETTINGS = yes
COMBO_ENABLE = yes
TAP_DANCE_ENABLE = yes 
KEY_OVERRIDE_ENABLE = yes

########### PLUS ###########

POINTING_DEVICE_DRIVER = azoteq_iqs5xx

########### DIP SWITCH ###########
DIP_SWITCH_ENABLE = yes

########### VIAL 0.7.4 ###########
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes

