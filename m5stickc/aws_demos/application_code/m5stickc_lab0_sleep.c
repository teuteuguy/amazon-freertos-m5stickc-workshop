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

static const TickType_t xSleepTimerFrequency_ms = 15000UL;
static TimerHandle_t xSleepTimer;

/*-----------------------------------------------------------*/

void m5stickc_lab0_init( TimerCallbackFunction_t callback )
{
    xSleepTimer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(xSleepTimerFrequency_ms), pdFALSE, NULL, callback);
    xTimerStart( xSleepTimer, 0 );
}

void m5stickc_lab0_event( void )
{
    ESP_LOGI(TAG, "Reseting sleep timer");
    xTimerReset(xSleepTimer, pdMS_TO_TICKS(xSleepTimerFrequency_ms));
}

/*-----------------------------------------------------------*/