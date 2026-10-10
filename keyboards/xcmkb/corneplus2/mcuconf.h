#pragma once

#include_next <mcuconf.h>

#undef RP_I2C_USE_I2C1
#define RP_I2C_USE_I2C1 TRUE

/* RP_PWM_USE_PWM4 was here for audio (GP9 = PWM slice 4, channel B); removed
 * with audio. The old RP_PWM_USE_TIM1 line went with it -- that symbol does
 * not exist in ChibiOS' RP2040 port and never did anything. */
