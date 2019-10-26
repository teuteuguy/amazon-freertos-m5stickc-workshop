/**
 * @file m5stickc_demo_config.h
 */

#ifndef _M5STICKC_DEMO_CONFIG_H_
#define _M5STICKC_DEMO_CONFIG_H_

#include "stdint.h"

/* To run a particular demo you need to define one of these.
 * Only one demo can be configured at a time
 *
 *          M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
 *          M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON
 *
 *  These defines are used in iot_demo_runner.h for demo selection */

#define M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON

uint8_t myStickCID[6];

#endif /* ifndef _M5STICKC_DEMO_CONFIG_H_ */
