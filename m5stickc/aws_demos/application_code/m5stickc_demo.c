/**
 * @file mystickc.c
 * @brief Demonstrates usage of the MQTT library.
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

/* MQTT include. */
#include "iot_mqtt.h"

#include "aws_demo.h"
#include "types/iot_network_types.h"
#include "semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "m5stickc.h"

#include "m5stickc_demo_config.h"
#include "m5stickc_demo.h"

static const char *TAG = "m5stickc_demo";

/*-----------------------------------------------------------*/

/* Declaration of demo functions. */
#ifdef M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON
#include "m5stickc_demo_lab1_aws_iot_button.h"
#endif // M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON

/*-----------------------------------------------------------*/

uint8_t myStickCID[6] = { 0 };

/*-----------------------------------------------------------*/

void init_sleep_timer(void);
void reset_sleep_timer(void);

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

        #ifdef M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
        ESP_LOGI(TAG, "Reseting sleep timer");
        reset_sleep_timer();
        #endif // M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

        #ifdef M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON
        m5stickc_demo_lab1_start();        
        #endif // M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON
    }
    if (base == M5BUTTON_B_EVENT_BASE && id == M5BUTTON_BUTTON_HOLD_EVENT) {
        ESP_LOGI(TAG, "Need to restart");
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
    m5config.power.lcd_backlight_level = 7;

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
    #ifdef M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    TFT_print((char *)"LAB0 - SETUP & SLEEP", CENTER, SCREEN_LINE_4);
    #endif // M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    #ifdef M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON
    TFT_print((char *)"LAB1 - AWS IOT BUTTON", CENTER, SCREEN_LINE_4);
    #endif // M5CONFIG_DEMO_LAB1_AWS_IOT_BUTTON

    TFT_drawLine(0, M5DISPLAY_HEIGHT - 13 - 3, M5DISPLAY_WIDTH, M5DISPLAY_HEIGHT - 13 - 3, TFT_ORANGE);
    
    uint16_t vbat = 0, vaps = 0;
    res = m5power_get_vbat(&vbat);
    res |= m5power_get_vaps(&vaps);
    if (res == ESP_OK)
    {
        float b = vbat * 1.1 / 1000;
        float c = vaps * 1.4 / 1000;
        uint8_t battery = ((b - 3.0) / 1.2) * 100;
        char pVbatStr[9] = {0};

        ESP_LOGI(TAG, "                    VBat:                   %u", vbat);
        ESP_LOGI(TAG, "                    VAps:                   %u", vaps);
        ESP_LOGI(TAG, "                    battery:                %u", battery);
        ESP_LOGI(TAG, "                    c:                      %f", c);

        if (c >= 4.5) {
            snprintf( pVbatStr, 9, "CHG: %u%%", battery > 100 ? 100 : battery );
        } else {
            snprintf( pVbatStr, 9, "BAT: %u%%", battery > 100 ? 100 : battery );
        }

        ESP_LOGI(TAG, "                    Charging str:           %s", pVbatStr);
        TFT_print(pVbatStr, 1, M5DISPLAY_HEIGHT - 13);
    } else {
        ESP_LOGI(TAG, "                    VBat/VAps ...           NOK");
    }

    ESP_LOGI(TAG, "m5stickc_demo_init: ... done");
    ESP_LOGI(TAG, "======================================================");

    #ifdef M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    esp_sleep_enable_ext0_wakeup(M5BUTTON_BUTTON_A_GPIO, 0);
    init_sleep_timer();
    #endif // M5CONFIG_DEMO_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

    return res;
}

/*-----------------------------------------------------------*/

static const TickType_t xSleepTimerFrequency_ms = 10000UL;
static TimerHandle_t xSleepTimer;

static void prvSleepTimerCallback( TimerHandle_t pxTimer )
{    
    // m5led_toggle();
    esp_err_t res = ESP_FAIL;
    res = m5power_set_sleep();
    if (res == ESP_OK)
    {
        esp_deep_sleep_start();
    }
}

void init_sleep_timer(void)
{
    xSleepTimer = xTimerCreate( "SleepTimer", pdMS_TO_TICKS( xSleepTimerFrequency_ms ), pdFALSE, NULL, prvSleepTimerCallback );
	xTimerStart( xSleepTimer, 0 );
}

void reset_sleep_timer(void)
{
    xTimerReset( xSleepTimer, pdMS_TO_TICKS( xSleepTimerFrequency_ms ) );
}


