/**
 * @file m5stickc_demo.h
 */

#ifndef _M5STICKC_DEMO__H_
#define _M5STICKC_DEMO__H_

/* To run a particular demo you need to define one of these.
 * Only one demo can be configured at a time
 *
 *          M5CONFIG_IOT_BUTTON_DEMO_ENABLED
 *
 *  These defines are used in iot_demo_runner.h for demo selection */

#define M5CONFIG_IOT_BUTTON_DEMO_ENABLED

#define DEMO_RUNNER_RunDemos vM5StickC_Run_Demo

esp_err_t vM5StickC_Run_Demo(void);

#endif /* ifndef _M5STICKC_DEMO__H_ */
