/**
 * @file m5stickc_demo.h
 */

#ifndef _M5STICKC_DEMO__H_
#define _M5STICKC_DEMO__H_

#include "esp_err.h"

/* Amazon FreeRTOS demo re-define to run specific M5StickC demos */
#define DEMO_RUNNER_RunDemos m5stickc_demo_run

esp_err_t m5stickc_demo_run(void);

#endif /* ifndef _M5STICKC_DEMO__H_ */
