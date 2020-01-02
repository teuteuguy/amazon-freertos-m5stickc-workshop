/**
 * @file m5stickc_lab2_shadow.h
 * @brief Lab2: Demonstrates using the IoT Core Shadow.
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"
#include "platform/iot_threads.h"

/* MQTT include. */
#include "iot_mqtt.h"

/* Shadow include. */
#include "aws_iot_shadow.h"

/* JSON utilities include. */
#include "iot_json_utils.h"

#include "aws_demo.h"
#include "types/iot_network_types.h"
#include "esp_log.h"

#include "m5stickc_lab_config.h"
#include "m5stickc_lab_connection.h"
#include "m5stickc_lab2_shadow.h"

#include "m5stickc.h"

static const char *TAG = "m5stickc_lab2_shadow";

/**
 * @brief The timeout for MQTT operations.
 */
#define SHADOW_UPDATE_TIMEOUT_MS (5000)

/**
 * @brief Format string representing a Shadow document with a "desired" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_DESIRED_JSON     \
    "{"                         \
    "\"state\":{"               \
    "\"desired\":{"             \
    "\"powerOn\":%01d,"         \
    "\"temperature\":%02d"      \
    "}"                         \
    "},"                        \
    "\"clientToken\":\"%06lu\"" \
    "}"

/**
 * @brief The expected size of #SHADOW_DESIRED_JSON.
 *
 * Because all the format specifiers in #SHADOW_DESIRED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_DESIRED_JSON_SIZE (sizeof(SHADOW_DESIRED_JSON) - 3)

/**
 * @brief Format string representing a Shadow document with a "reported" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_REPORTED_JSON    \
    "{"                         \
    "\"state\":{"               \
    "\"reported\":{"            \
    "\"powerOn\":%01d,"         \
    "\"temperature\":%2d"       \
    "}"                         \
    "},"                        \
    "\"clientToken\":\"%06lu\"" \
    "}"

/**
 * @brief The expected size of #SHADOW_REPORTED_JSON.
 *
 * Because all the format specifiers in #SHADOW_REPORTED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_REPORTED_JSON_SIZE (sizeof(SHADOW_REPORTED_JSON) - 3)

typedef struct {
    uint8_t powerOn;
    uint8_t temperature;
} shadowState_t;

shadowState_t shadowStateReported = {
    .powerOn = 0,
    .temperature = 35};
shadowState_t shadowStateDesired = {
    .powerOn = 0,
    .temperature = 0};

/**
 * @brief The expected size of #SHADOW_REPORTED_JSON.
 *
 * Because all the format specifiers in #SHADOW_REPORTED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_REPORTED_JSON_SIZE (sizeof(SHADOW_REPORTED_JSON) - 3)

/*-----------------------------------------------------------*/

static const TickType_t xAirConRefreshTimerFrequency_ms = 10000UL;
static TimerHandle_t xAirCon;

static void prvAirConTimerCallback(TimerHandle_t pxTimer);

/*-----------------------------------------------------------*/

void vLab2NetworkConnectedCallback(bool awsIotMqttMode,
                               const char *pIdentifier,
                               void *pNetworkServerInfo,
                               void *pNetworkCredentialInfo,
                               const IotNetworkInterface_t *pNetworkInterface)
{
    ESP_LOGD(TAG, "vNetworkConnectedCallback for %s", pIdentifier);

    /* Start the AirCon */
    xAirCon = xTimerCreate("AirConRefresh", pdMS_TO_TICKS(xAirConRefreshTimerFrequency_ms), pdTRUE, (void *)pIdentifier, prvAirConTimerCallback);
    xTimerStart(xAirCon, 0);
}

void vLab2NetworkDisconnectedCallback(const IotNetworkInterface_t *pNetworkInterface)
{
    ESP_LOGI(TAG, "vNetworkDisconnectedCallback");
    m5stickc_lab_connection_cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Parses a key in the "state" section of a Shadow delta document.
 *
 * @param[in] pDeltaDocument The Shadow delta document to parse.
 * @param[in] deltaDocumentLength The length of `pDeltaDocument`.
 * @param[in] pDeltaKey The key in the delta document to find. Must be NULL-terminated.
 * @param[out] pDelta Set to the first character in the delta key.
 * @param[out] pDeltaLength The length of the delta key.
 *
 * @return `true` if the given delta key is found; `false` otherwise.
 */
static bool _getDelta(const char *pDeltaDocument,
                      size_t deltaDocumentLength,
                      const char *pDeltaKey,
                      const char **pDelta,
                      size_t *pDeltaLength)
{
    bool stateFound = false, deltaFound = false;
    const size_t deltaKeyLength = strlen(pDeltaKey);
    const char *pState = NULL;
    size_t stateLength = 0;

    /* Find the "state" key in the delta document. */
    stateFound = IotJsonUtils_FindJsonValue(pDeltaDocument,
                                            deltaDocumentLength,
                                            "state",
                                            5,
                                            &pState,
                                            &stateLength);

    if (stateFound == true)
    {
        /* Find the delta key within the "state" section. */
        deltaFound = IotJsonUtils_FindJsonValue(pState,
                                                stateLength,
                                                pDeltaKey,
                                                deltaKeyLength,
                                                pDelta,
                                                pDeltaLength);
    }
    else
    {
        IotLogWarn("Failed to find \"state\" in Shadow delta document.");
    }

    return deltaFound;
}

/*-----------------------------------------------------------*/

/**
 * @brief Parses the "state" key from the "previous" or "current" sections of a
 * Shadow updated document.
 *
 * @param[in] pUpdatedDocument The Shadow updated document to parse.
 * @param[in] updatedDocumentLength The length of `pUpdatedDocument`.
 * @param[in] pSectionKey Either "previous" or "current". Must be NULL-terminated.
 * @param[out] pState Set to the first character in "state".
 * @param[out] pStateLength Length of the "state" section.
 *
 * @return `true` if the "state" was found; `false` otherwise.
 */
static bool _getUpdatedState(const char *pUpdatedDocument,
                             size_t updatedDocumentLength,
                             const char *pSectionKey,
                             const char **pState,
                             size_t *pStateLength)
{
    bool sectionFound = false, stateFound = false;
    const size_t sectionKeyLength = strlen(pSectionKey);
    const char *pSection = NULL;
    size_t sectionLength = 0;

    /* Find the given section in the updated document. */
    sectionFound = IotJsonUtils_FindJsonValue(pUpdatedDocument,
                                              updatedDocumentLength,
                                              pSectionKey,
                                              sectionKeyLength,
                                              &pSection,
                                              &sectionLength);

    if (sectionFound == true)
    {
        /* Find the "state" key within the "previous" or "current" section. */
        stateFound = IotJsonUtils_FindJsonValue(pSection,
                                                sectionLength,
                                                "state",
                                                5,
                                                pState,
                                                pStateLength);
    }
    else
    {
        IotLogWarn("Failed to find section %s in Shadow updated document.",
                   pSectionKey);
    }

    return stateFound;
}
/*-----------------------------------------------------------*/

/**
 * @brief Send the Shadow update that will trigger the Shadow callbacks.
 *
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 *
 * @return `EXIT_SUCCESS` if all Shadow updates were sent; `EXIT_FAILURE`
 * otherwise.
 */
static int _reportShadow(const char *const pThingName)
{
    int status = EXIT_SUCCESS;

    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

    static char pUpdateDocument[EXPECTED_REPORTED_JSON_SIZE + 1] = {0};

    /* Set the common members of the Shadow update document info. */
    updateDocument.pThingName = pThingName;
    updateDocument.thingNameLength = strlen(pThingName);
    updateDocument.u.update.pUpdateDocument = pUpdateDocument;
    updateDocument.u.update.updateDocumentLength = EXPECTED_REPORTED_JSON_SIZE;

    /* Generate a Shadow reported state document, using a timestamp for the client
     * token. To keep the client token within 6 characters, it is modded by 1000000.
     * */
    status = snprintf(pUpdateDocument,
                      EXPECTED_REPORTED_JSON_SIZE + 1,
                      SHADOW_REPORTED_JSON,
                      (int)shadowStateReported.powerOn,
                      (uint8_t)shadowStateReported.temperature,
                      (long unsigned)(IotClock_GetTimeMs() % 1000000));

    /* Check for errors from snprintf. The expected value is the length of
     * the desired JSON document less the format specifier for the state.
     * */
    if (status <= 0)
    {
        ESP_LOGE(TAG, "Failed to generate reported state document for Shadow update: %u vs. %u\n", status, EXPECTED_REPORTED_JSON_SIZE);
        ESP_LOGE(TAG, "%s\n", pUpdateDocument);
        status = EXIT_FAILURE;
        return status;
    }
    else
    {
        status = EXIT_SUCCESS;
    }

    if (status == EXIT_SUCCESS)
    {
        status = m5stickc_lab_connection_update_shadow(&updateDocument);
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Shadow delta callback, invoked when the desired and updates Shadow
 * states differ.
 *
 * This function simulates a device updating its state in response to a Shadow.
 *
 * @param[in] pCallbackContext Not used.
 * @param[in] pCallbackParam The received Shadow delta document.
 */
static void _shadowDeltaCallback(void *pCallbackContext,
                                 AwsIotShadowCallbackParam_t *pCallbackParam)
{
    bool powerOnDeltaFound = false;
    bool temperatureDeltaFound = false;
    const char *pPowerOnDelta = NULL;
    const char *pTemperatureDelta = NULL;
    size_t deltaLength = 0;
    int status = 0;

    /* Check if there is a different "powerOn" state in the Shadow. */
    powerOnDeltaFound = _getDelta(pCallbackParam->u.callback.pDocument,
                           pCallbackParam->u.callback.documentLength,
                           "powerOn",
                           &pPowerOnDelta,
                           &deltaLength);
    
    if (powerOnDeltaFound == true)
    {
        uint8_t newPowerOn = (uint8_t)atoi(pPowerOnDelta);
        IotLogInfo("Shadow delta: powerOn: %u vs. %u", newPowerOn, shadowStateDesired.powerOn);
        if (newPowerOn != shadowStateReported.powerOn)
        {
            IotLogInfo("%.*s changing powerOn state from %u to %u.",
                       pCallbackParam->thingNameLength,
                       pCallbackParam->pThingName,
                       shadowStateReported.powerOn, newPowerOn);

            shadowStateDesired.powerOn = newPowerOn;
            shadowStateReported.powerOn = newPowerOn;
        }
    }

    /* Check if there is a different "temperature" state in the Shadow. */
    temperatureDeltaFound = _getDelta(pCallbackParam->u.callback.pDocument,
                                      pCallbackParam->u.callback.documentLength,
                                      "temperature",
                                      &pTemperatureDelta,
                                      &deltaLength);

    if (temperatureDeltaFound == true)
    {
        uint8_t newTemperature = (uint8_t)atoi(pTemperatureDelta);
        IotLogDebug("Shadow delta: temperature: %u vs. %u", newTemperature, shadowStateDesired.temperature);
        /* Change the current state based on the value in the delta document. */
        if (newTemperature != shadowStateDesired.temperature)
        {            
            IotLogInfo("%.*s changing temperature state from %u to %u.",
                       pCallbackParam->thingNameLength,
                       pCallbackParam->pThingName,
                       shadowStateDesired.temperature, newTemperature);

            shadowStateDesired.temperature = newTemperature;
        }
    }

    if (powerOnDeltaFound == true)
    {
        IotLogInfo("Shadow delta: change of Power State requires updating shadow");

        status = _reportShadow(pCallbackParam->pThingName);

        if (status != EXIT_SUCCESS)
        {
            IotLogError("Shadow delta: report new shadow failed\n");
        }
    }

}

/*-----------------------------------------------------------*/

/**
 * @brief Shadow updated callback, invoked when the Shadow document changes.
 *
 * This function reports when a Shadow has been updated.
 *
 * @param[in] pCallbackContext Not used.
 * @param[in] pCallbackParam The received Shadow updated document.
 */
static void _shadowUpdatedCallback(void *pCallbackContext,
                                   AwsIotShadowCallbackParam_t *pCallbackParam)
{
    bool previousFound = false, currentFound = false;
    const char *pPrevious = NULL, *pCurrent = NULL;
    size_t previousLength = 0, currentLength = 0;

    /* Silence warnings about unused parameters. */
    (void)pCallbackContext;

    /* Find the previous Shadow document. */
    previousFound = _getUpdatedState(pCallbackParam->u.callback.pDocument,
                                     pCallbackParam->u.callback.documentLength,
                                     "previous",
                                     &pPrevious,
                                     &previousLength);

    /* Find the current Shadow document. */
    currentFound = _getUpdatedState(pCallbackParam->u.callback.pDocument,
                                    pCallbackParam->u.callback.documentLength,
                                    "current",
                                    &pCurrent,
                                    &currentLength);

    /* Log the previous and current states. */
    if ((previousFound == true) && (currentFound == true))
    {
        IotLogInfo("Shadow was updated!\r\n"
                   "Previous: {\"state\":%.*s}\r\n"
                   "Current:  {\"state\":%.*s}",
                   previousLength,
                   pPrevious,
                   currentLength,
                   pCurrent);
    }
    else
    {
        if (previousFound == false)
        {
            IotLogWarn("Previous state not found in Shadow updated document.");
        }

        if (currentFound == false)
        {
            IotLogWarn("Current state not found in Shadow updated document.");
        }
    }
}

/*-----------------------------------------------------------*/

static void prvAirConTimerCallback(TimerHandle_t pxTimer)
{
    int status = EXIT_SUCCESS;
    configASSERT(pxTimer);

    char *pThingName = (char *)pvTimerGetTimerID(pxTimer);

    // Used for the screen.
    char pAirConStr[11] = {0};

    if (shadowStateReported.powerOn == 1)
    {
        shadowStateReported.temperature--;
        if (shadowStateReported.temperature < shadowStateDesired.temperature)
        {
            shadowStateReported.temperature = shadowStateDesired.temperature;
        }

        ESP_LOGI(TAG, "Timer: AirCon is ON => Temp (%u) needs to decrease to target (%u)",
                shadowStateReported.temperature,
                shadowStateDesired.temperature);

        status = snprintf(pAirConStr, 11, " ON %02u", shadowStateReported.temperature);
    }
    else
    {
        shadowStateReported.temperature++;
        if (shadowStateReported.temperature > 40)
        {
            shadowStateReported.temperature = 40;
        }

        ESP_LOGI(TAG, "Timer: AirCon is OFF => Temp (%u) increases", shadowStateReported.temperature);

        status = snprintf(pAirConStr, 11, "OFF %02u", shadowStateReported.temperature);
    }

    if (status >= 0)
    {
        TFT_print(pAirConStr, M5DISPLAY_WIDTH - 6 * 9, M5DISPLAY_HEIGHT - 13);
    }
    
    /* Report Shadow. */
    status = _reportShadow(pThingName);

    if (status != EXIT_SUCCESS)
    {
        IotLogError("Timer: Failed to report shadow.");
    }

}


/*-----------------------------------------------------------*/

void m5stickc_lab2_init(const char *const strID)
{
    static m5stickc_iot_connection_params_t connectionParams;

    connectionParams.strID = (char *)strID;
    connectionParams.useShadow = true;
    connectionParams.networkConnectedCallback = vLab2NetworkConnectedCallback;
    connectionParams.networkDisconnectedCallback = vLab2NetworkDisconnectedCallback;
    connectionParams.shadowDeltaCallback = _shadowDeltaCallback;
    connectionParams.shadowUpdatedCallback = _shadowUpdatedCallback;

    m5stickc_lab_connection_init(&connectionParams);
}

/*-----------------------------------------------------------*/
