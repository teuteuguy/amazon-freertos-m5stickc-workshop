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

/* Declaration of demo functions. */
#if defined(M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP) || defined(M5CONFIG_LAB1_AWS_IOT_BUTTON)
#include "m5stickc_lab0_sleep.h"
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP || M5CONFIG_LAB1_AWS_IOT_BUTTON
#if defined(M5CONFIG_LAB1_AWS_IOT_BUTTON) || defined(M5CONFIG_LAB2_SHADOW)
#include "m5stickc_lab1_aws_iot_button.h"
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON || M5CONFIG_LAB2_SHADOW
#ifdef M5CONFIG_LAB2_SHADOW
#include "m5stickc_lab2_shadow.h"
#endif // M5CONFIG_LAB2_SHADOW

#include "m5stickc_lab_connection.h"

/*-----------------------------------------------------------*/

static const char *TAG = "m5stickc_demo";

/*-----------------------------------------------------------*/

uint8_t uM5StickCID[6] = { 0 };
#define M5STICKC_ID_STR_LENGTH ( sizeof(uM5StickCID) * 2 + 1 )
char strM5StickCID[M5STICKC_ID_STR_LENGTH] = "";

/*-----------------------------------------------------------*/

esp_err_t draw_battery_level(void);
void battery_refresh_timer_init(void);

esp_err_t m5stickc_demo_init(void);

static esp_err_t prvDeepSleep( gpio_num_t wakeup_pin );
static void prvSleepTimerCallback( TimerHandle_t pxTimer );

#if defined(M5CONFIG_LAB1_AWS_IOT_BUTTON) || defined(M5CONFIG_LAB2_SHADOW)

IotSemaphore_t m5stickc_lab1_semaphore;

#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON || M5CONFIG_LAB2_SHADOW


/*-----------------------------------------------------------*/

esp_err_t m5stickc_demo_run(void)
{
    esp_err_t res = esp_efuse_mac_get_default(uM5StickCID);

    if (res == ESP_OK)
    {
        int status = snprintf( strM5StickCID, M5STICKC_ID_STR_LENGTH, "%02x%02x%02x%02x%02x%02x",
                    uM5StickCID[0], uM5StickCID[1], uM5StickCID[2], uM5StickCID[3], uM5StickCID[4], uM5StickCID[5] );
        if( status < 0 )
        {
            ESP_LOGE(TAG, "Error generating the ID: %d", (int) status);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "myStickCID: %s", strM5StickCID);

    if (res == ESP_OK)
    {
        res = m5stickc_demo_init();
    }

    return res;
}

/*-----------------------------------------------------------*/

void m5button_event_handler(void * handler_arg, esp_event_base_t base, int32_t id, void * event_data)
{
    if (base == M5BUTTON_A_EVENT_BASE )
    {

        if ( id == M5BUTTON_BUTTON_CLICK_EVENT )
        {
            ESP_LOGI(TAG, "Button A Pressed");            
        }
        if ( id == M5BUTTON_BUTTON_HOLD_EVENT )
        {
            ESP_LOGI(TAG, "Button A Held");            
        }
        
#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
        m5stickc_lab0_event();
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

#if defined(M5CONFIG_LAB1_AWS_IOT_BUTTON) || defined(M5CONFIG_LAB2_SHADOW)
        IotSemaphore_Wait(&m5stickc_lab1_semaphore);
        m5stickc_lab1_action(strM5StickCID, id);
        IotSemaphore_Post(&m5stickc_lab1_semaphore);
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON || M5CONFIG_LAB2_SHADOW

#if defined(M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP) || defined(M5CONFIG_LAB1_AWS_IOT_BUTTON)
        m5stickc_lab0_event();
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP || M5CONFIG_LAB1_AWS_IOT_BUTTON

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

    m5stickc_config_t m5config;
    m5config.power.enable_lcd_backlight = false;
    m5config.power.lcd_backlight_level = 1;

    res = m5_init(&m5config);
    ESP_LOGI(TAG, "m5stickc_demo_init: m5_init ...             %s", res == ESP_OK ? "OK" : "NOK");
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
    m5stickc_lab0_init( prvSleepTimerCallback );
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
    TFT_print((char *)"LAB1 - AWS IOT BUTTON", CENTER, SCREEN_LINE_4);
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

#ifdef M5CONFIG_LAB2_SHADOW
    TFT_print((char *)"LAB2 - THING SHADOW", CENTER, SCREEN_LINE_4);
    m5stickc_lab2_init(strM5StickCID);
#endif // M5CONFIG_LAB2_SHADOW

    TFT_drawLine(0, M5DISPLAY_HEIGHT - 13 - 3, M5DISPLAY_WIDTH, M5DISPLAY_HEIGHT - 13 - 3, TFT_ORANGE);
    
    res = draw_battery_level();
    battery_refresh_timer_init();

    res = esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, ESP_EVENT_ANY_ID, m5button_event_handler, NULL);
    ESP_LOGI(TAG, "                    Button A registered ... %s", res == ESP_OK ? "OK" : "NOK");
    if (res != ESP_OK) return res;

    res = esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, ESP_EVENT_ANY_ID, m5button_event_handler, NULL);
    ESP_LOGI(TAG, "                    Button B registered ... %s", res == ESP_OK ? "OK" : "NOK");
    if (res != ESP_OK) return res;

    ESP_LOGI(TAG, "m5stickc_demo_init: ... done");
    ESP_LOGI(TAG, "======================================================");

#if defined(M5CONFIG_LAB1_AWS_IOT_BUTTON) || defined(M5CONFIG_LAB2_SHADOW)

    if (!IotSemaphore_Create(&m5stickc_lab1_semaphore, 0, 1))
    {
        ESP_LOGE(TAG, "Failed to create Lab 1 semaphore!");
        res = ESP_FAIL;
    }

    if (res == ESP_OK)
    {
        /* Init the Semaphore to release it */
        IotSemaphore_Post(&m5stickc_lab1_semaphore);
    }

#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON || M5CONFIG_LAB2_SHADOW

#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON

    m5stickc_lab1_init(strM5StickCID);

    // Create semaphore for lab1
    if ( res == ESP_OK )
    {
        /* Take the Semaphore */
        IotSemaphore_Wait( &m5stickc_lab1_semaphore );
        
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
        {
            // Woken up by our button
            ESP_LOGI( TAG, "                    Woken up by the button" );
            m5stickc_lab1_action(strM5StickCID, M5BUTTON_BUTTON_CLICK_EVENT);
        }
        else
        {
            // Woken up by other
            ESP_LOGI( TAG, "                    Woken up by other!" );
        }

        IotSemaphore_Post( &m5stickc_lab1_semaphore );
    }

#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

#if defined(M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP) || defined(M5CONFIG_LAB1_AWS_IOT_BUTTON)
    m5stickc_lab0_init( prvSleepTimerCallback );
#endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP || M5CONFIG_LAB1_AWS_IOT_BUTTON

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

/*-----------------------------------------------------------*/

static esp_err_t prvDeepSleep( gpio_num_t wakeup_pin )
{
    esp_err_t res = ESP_FAIL;

#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
    m5stickc_lab_connection_cleanup();
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

    ESP_LOGI(TAG, "Going to DEEP SLEEP!");

    res = esp_sleep_enable_ext0_wakeup( wakeup_pin, 0 );

    if (res != ESP_OK)
    {
        return res;
    }

    res = m5power_set_sleep();
    if (res == ESP_OK)
    {
        esp_deep_sleep_start();
    }

    return res;
}

static void prvSleepTimerCallback(TimerHandle_t pxTimer)
{
    esp_err_t res = ESP_FAIL;

    ESP_LOGI(TAG, "Sleep timer callback!");

#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
    
    IotSemaphore_Wait( &m5stickc_lab1_semaphore );
    
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

    res = prvDeepSleep( M5BUTTON_BUTTON_A_GPIO );

    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Error setting deep sleep!");
    }
}