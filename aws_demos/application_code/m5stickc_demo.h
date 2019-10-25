/**
 * @file m5stickc_demo.h
 */

#ifndef _M5STICKC_DEMO__H_
#define _M5STICKC_DEMO__H_

/* To run a particular demo you need to define one of these.
 * Only one demo can be configured at a time
 *
 *          M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
 *          M5CONFIG_LAB1_AWS_IOT_BUTTON
 *
 *  These defines are used in iot_demo_runner.h for demo selection */

#define M5CONFIG_LAB1_AWS_IOT_BUTTON


/* Amazon FreeRTOS demo re-define to run specific M5StickC demos */
#define DEMO_RUNNER_RunDemos m5stickc_demo_run

esp_err_t m5stickc_demo_run(void);

#endif /* ifndef _M5STICKC_DEMO__H_ */
