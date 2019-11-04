/**
 * @file m5stickc_demo.c
 * @brief Demo code to run different labs on the M5StickC.
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"
#include "platform/iot_threads.h"

#include "aws_demo.h"
#include "types/iot_network_types.h"
#include "semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "m5stickc.h"

#include "m5stickc_lab_config.h"
#include "m5stickc_demo.h"

static const char *TAG = "m5stickc_demo";

/*-----------------------------------------------------------*/

/* Declaration of demo functions. */
#if defined(M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP) || defined(M5CONFIG_LAB1_AWS_IOT_BUTTON)
#include "m5stickc_lab0_sleep.h"
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON
#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
#include "m5stickc_lab1_aws_iot_button.h"
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON
#ifdef M5CONFIG_LAB2_SHADOW
#include "m5stickc_lab2_shadow.h"
#endif // M5CONFIG_LAB2_SHADOW

/*-----------------------------------------------------------*/

uint8_t myStickCID[6] = { 0 };

/*-----------------------------------------------------------*/

esp_err_t draw_battery_level(void);
void battery_refresh_timer_init(void);

esp_err_t m5stickc_demo_init(void);

/*-----------------------------------------------------------*/

esp_err_t m5stickc_demo_run(void)
{
    esp_err_t res = m5stickc_demo_init();

    esp_efuse_mac_get_default(myStickCID);
    configPRINTF(("myStickCID: %02x%02x%02x%02x%02x%02x\n", myStickCID[0], myStickCID[1], myStickCID[2], myStickCID[3], myStickCID[4], myStickCID[5]));

    return res;
}

/*-----------------------------------------------------------*/

void m5button_event_handler(void * handler_arg, esp_event_base_t base, int32_t id, void * event_data)
{
    if (base == M5BUTTON_A_EVENT_BASE && id == M5BUTTON_BUTTON_CLICK_EVENT) {
        ESP_LOGI(TAG, "Button A Pressed");
        
#ifdef M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
        m5stickc_lab0_start();
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    }

    if (base == M5BUTTON_B_EVENT_BASE && id == M5BUTTON_BUTTON_HOLD_EVENT) {
        ESP_LOGI(TAG, "Button B Held");

        ESP_LOGI(TAG, "Restarting");
        esp_restart();
    }
}

esp_err_t m5stickc_demo_init(void)
{
    esp_err_t res = ESP_FAIL;

    ESP_LOGI(TAG, "======================================================");
    ESP_LOGI(TAG, "m5stickc_demo_init: ...");
    ESP_LOGI(TAG, "m5stickc_demo_init: democonfigDEMO_STACKSIZE: %u", democonfigDEMO_STACKSIZE);
    ESP_LOGI(TAG, "m5stickc_demo_init: democonfigDEMO_PRIORITY:  %u", democonfigDEMO_PRIORITY);

    m5stickc_config_t m5config;
    m5config.power.enable_lcd_backlight = false;
    m5config.power.lcd_backlight_level = 1;

    res = m5_init(&m5config);
    ESP_LOGI(TAG, "m5stickc_demo_init: m5_init ...             %s", res == ESP_OK ? "OK" : "NOK");
    if (res != ESP_OK) return res;

    res = esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, ESP_EVENT_ANY_ID, m5button_event_handler, NULL);
    ESP_LOGI(TAG, "                    Button A registered ... %s", res == ESP_OK ? "OK" : "NOK");
    if (res != ESP_OK) return res;

    res = esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, ESP_EVENT_ANY_ID, m5button_event_handler, NULL);
    ESP_LOGI(TAG, "                    Button B registered ... %s", res == ESP_OK ? "OK" : "NOK");
    if (res != ESP_OK) return res;

    TFT_FONT_ROTATE = 0;
    TFT_TEXT_WRAP = 0;
    TFT_FONT_TRANSPARENT = 0;
    TFT_FONT_FORCEFIXED = 0;
    TFT_GRAY_SCALE = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
    TFT_setRotation(LANDSCAPE_FLIP);
    TFT_setFont(DEFAULT_FONT, NULL);
    TFT_resetclipwin();
    TFT_fillScreen(TFT_BLACK);
    TFT_FONT_BACKGROUND = TFT_BLACK;
    TFT_FONT_FOREGROUND = TFT_ORANGE;
    res = m5display_on();
    ESP_LOGI(TAG, "                    LCD Backlight ON ...    %s", res == ESP_OK ? "OK" : "NOK");
    if (res != ESP_OK) return res;

    #define SCREEN_OFFSET 2
    #define SCREEN_LINE_HEIGHT 14
    #define SCREEN_LINE_1  SCREEN_OFFSET + 0 * SCREEN_LINE_HEIGHT
    #define SCREEN_LINE_2  SCREEN_OFFSET + 1 * SCREEN_LINE_HEIGHT
    #define SCREEN_LINE_3  SCREEN_OFFSET + 2 * SCREEN_LINE_HEIGHT
    #define SCREEN_LINE_4  SCREEN_OFFSET + 3 * SCREEN_LINE_HEIGHT

    TFT_print((char *)"Amazon FreeRTOS", CENTER, SCREEN_LINE_1);
    TFT_print((char *)"workshop", CENTER, SCREEN_LINE_2);
#ifdef M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    TFT_print((char *)"LAB0 - SETUP & SLEEP", CENTER, SCREEN_LINE_4);
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
    TFT_print((char *)"LAB1 - AWS IOT BUTTON", CENTER, SCREEN_LINE_4);
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON
#ifdef M5CONFIG_LAB2_SHADOW
    TFT_print((char *)"LAB2 - THING SHADOW", CENTER, SCREEN_LINE_4);
#endif // M5CONFIG_LAB2_SHADOW

    TFT_drawLine(0, M5DISPLAY_HEIGHT - 13 - 3, M5DISPLAY_WIDTH, M5DISPLAY_HEIGHT - 13 - 3, TFT_ORANGE);
    
    res = draw_battery_level();
    battery_refresh_timer_init();

    ESP_LOGI(TAG, "m5stickc_demo_init: ... done");
    ESP_LOGI(TAG, "======================================================");

#ifdef M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    m5stickc_lab0_init();
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
    {
        m5stickc_lab1_start( m5stickc_lab0_init );
    }
    else
    {
        m5stickc_lab0_init();
    }
#endif

#ifdef M5CONFIG_LAB2_SHADOW
    m5stickc_lab2_start();
#endif // M5CONFIG_LAB2_SHADOW

    return res;
}

/*-----------------------------------------------------------*/

static const TickType_t xBatteryRefreshTimerFrequency_ms = 10000UL;
static TimerHandle_t xBatteryRefresh;

esp_err_t draw_battery_level(void)
{
    esp_err_t res = ESP_FAIL;
    int status = EXIT_SUCCESS;
    uint16_t vbat = 0, vaps = 0, b, c, battery;
    char pVbatStr[11] = {0};

    res = m5power_get_vbat(&vbat);
    res |= m5power_get_vaps(&vaps);

    if (res == ESP_OK)
    {
        ESP_LOGD(TAG, "draw_battery_level: VBat:         %u", vbat);
        ESP_LOGD(TAG, "draw_battery_level: VAps:         %u", vaps);
        b = (vbat * 1.1);
        ESP_LOGD(TAG, "draw_battery_level: b:            %u", b);
        c = (vaps * 1.4);
        ESP_LOGD(TAG, "draw_battery_level: c:            %u", c);
        battery = ((b - 3000)) / 12;
        ESP_LOGD(TAG, "draw_battery_level: battery:      %u", battery);

        if (battery >= 100)
        {
            battery = 99; // No need to support 100% :)
        }

        if (c >= 4500) //4.5)
        {
            status = snprintf(pVbatStr, 11, "CHG: %02u%%", battery);
        }
        else
        {
            status = snprintf(pVbatStr, 11, "BAT: %02u%%", battery);
        }

        if (status < 0) {
            ESP_LOGE(TAG, "draw_battery_level: error with creating battery string");
        }
        else
        {
            ESP_LOGD(TAG, "draw_battery_level: Charging str(%i): %s", status, pVbatStr);
            TFT_print(pVbatStr, 1, M5DISPLAY_HEIGHT - 13);
        }
    }

    return res;
}

static void prvBatteryRefreshTimerCallback(TimerHandle_t pxTimer)
{
    draw_battery_level();    
}

void battery_refresh_timer_init(void)
{
    xBatteryRefresh = xTimerCreate("BatteryRefresh", pdMS_TO_TICKS(xBatteryRefreshTimerFrequency_ms), pdTRUE, NULL, prvBatteryRefreshTimerCallback);
    xTimerStart(xBatteryRefresh, 0);
}


