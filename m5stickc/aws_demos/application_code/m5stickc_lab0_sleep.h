/**
 * @file m5stickc_lab0_sleep.h
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5STICKC_LAB0_SLEEP_H_
#define _M5STICKC_LAB0_SLEEP_H_

/* Platform layer includes. */
#include "platform/iot_clock.h"

void m5stickc_lab0_init( TimerCallbackFunction_t callback );
void m5stickc_lab0_event( void );

#endif /* ifndef _M5STICKC_LAB0_SLEEP_H_ */
