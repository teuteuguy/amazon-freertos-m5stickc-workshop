/**
 * @file m5stickc_lab_config.h
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5STICKC_LAB_CONFIG_H_
#define _M5STICKC_LAB_CONFIG_H_

#include "stdint.h"

/* To run a particular lab you need to define one of these.
 * Only one lab can be configured at a time
 *
 *          M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
 *          M5CONFIG_LAB1_AWS_IOT_BUTTON
 *          M5CONFIG_LAB2_SHADOW
 *
 *  These defines are used in iot_demo_runner.h for demo selection */

#define M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

uint8_t myStickCID[6];

#endif /* ifndef _M5STICKC_LAB_CONFIG_H_ */
