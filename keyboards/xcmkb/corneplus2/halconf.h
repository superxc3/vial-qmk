#pragma once

#define HAL_USE_I2C TRUE
#define HAL_USE_PAL TRUE

/* HAL_USE_PWM was here for audio (pwm_hardware on GP9). Audio is removed on
 * corneplus2 and nothing else in this build needs PWM -- the WS2812 vendor
 * driver is PIO, backlight and haptic are off. */

#include_next <halconf.h>
