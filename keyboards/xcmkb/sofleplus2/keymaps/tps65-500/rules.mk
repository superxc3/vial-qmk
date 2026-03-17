
############ CAPS WORD ############
# read more here https://docs.qmk.fm/features/caps_word, but it may relates to COMMAND_ENABLE = no, already disabled by default so no conflict here.
CAPS_WORD_ENABLE = yes
TRI_LAYER_ENABLE = yes
LEADER_ENABLE = yes


########### VIA VIAL ###########

# VIA VIAL
# Disabling VIA/Vial for now as requested by user. Can be re-enabled by defining VIAL_ENABLE.
VIA_ENABLE = yes

# DISABLE VIAL FOR MULTI_TOUCH


#ifdef VIAL_ENABLE
# Reduce size of Vial
QMK_SETTINGS = yes
COMBO_ENABLE = no
TAP_DANCE_ENABLE = no
KEY_OVERRIDE_ENABLE = no
#endif
########### PLUS ###########

# macOS dual-mode: mouse fallback for non-PTP hosts (macOS/Linux)
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = digitizer

########### DIP SWITCH ###########
DIP_SWITCH_ENABLE = yes

########### VIAL 0.7.4 ###########
#ifdef VIAL_ENABLE
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes
#endif

