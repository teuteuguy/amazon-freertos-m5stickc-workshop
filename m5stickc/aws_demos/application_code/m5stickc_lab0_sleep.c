/**
 * @file m5stickc_lab0_sleep.h
 * @brief Lab0: Demonstrates getting started, and simply implmenting dumb deep sleep.
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

#include "m5stickc_lab_config.h"
#include "m5stickc_lab0_sleep.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"

#include "esp_log.h"

#include "m5stickc.h"

static const char *TAG = "m5stickc_lab0_sleep";

/*-----------------------------------------------------------*/

static const TickType_t xSleepTimerFrequency_ms = 10000UL;
static TimerHandle_t xSleepTimer;

static void prvSleepTimerCallback(TimerHandle_t pxTimer)
{
    esp_err_t res = ESP_FAIL;
    res = m5power_set_sleep();
    if (res == ESP_OK)
    {
        esp_deep_sleep_start();
    }
}

/*-----------------------------------------------------------*/

void m5stickc_lab0_init(void)
{
    esp_sleep_enable_ext0_wakeup(M5BUTTON_BUTTON_A_GPIO, 0);
    xSleepTimer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(xSleepTimerFrequency_ms), pdFALSE, NULL, prvSleepTimerCallback);
    xTimerStart(xSleepTimer, 0);
}

void m5stickc_lab0_start(void)
{
    ESP_LOGI(TAG, "Reseting sleep timer");
    xTimerReset(xSleepTimer, pdMS_TO_TICKS(xSleepTimerFrequency_ms));
}

/*-----------------------------------------------------------*/