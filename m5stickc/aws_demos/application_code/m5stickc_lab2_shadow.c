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
#include "m5stickc_lab2_shadow.h"

#include "m5stickc.h"

/**
 * @brief The keep-alive interval used for this demo.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#define KEEP_ALIVE_SECONDS (60)

/**
 * @brief The timeout for Shadow and MQTT operations in this demo.
 */
#define TIMEOUT_MS (5000)

static const char *TAG = "m5stickc_lab2_shadow";

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

void vNetworkConnectedCallback(bool awsIotMqttMode,
                               const char *pIdentifier,
                               void *pNetworkServerInfo,
                               void *pNetworkCredentialInfo,
                               const IotNetworkInterface_t *pNetworkInterface)
{
    ESP_LOGI(TAG, "Network connected.");
}

static bool lostConnection = false;
void vNetworkDisconnectedCallback(const IotNetworkInterface_t *pNetworkInterface)
{
    ESP_LOGI(TAG, "Network disconnected.");

    lostConnection = true;
}

/*-----------------------------------------------------------*/

/**
 * @brief Initialize the the MQTT library and the Shadow library.
 *
 * @return `EXIT_SUCCESS` if all libraries were successfully initialized;
 * `EXIT_FAILURE` otherwise.
 */
static int _initializeDemo(void)
{
    int status = EXIT_SUCCESS;
    IotMqttError_t mqttInitStatus = IOT_MQTT_SUCCESS;
    AwsIotShadowError_t shadowInitStatus = AWS_IOT_SHADOW_SUCCESS;

    /* Flags to track cleanup on error. */
    bool mqttInitialized = false;

    /* Initialize the MQTT library. */
    mqttInitStatus = IotMqtt_Init();

    if (mqttInitStatus == IOT_MQTT_SUCCESS)
    {
        mqttInitialized = true;
    }
    else
    {
        status = EXIT_FAILURE;
    }

    /* Initialize the Shadow library. */
    if (status == EXIT_SUCCESS)
    {
        /* Use the default MQTT timeout. */
        shadowInitStatus = AwsIotShadow_Init(0);

        if (shadowInitStatus != AWS_IOT_SHADOW_SUCCESS)
        {
            status = EXIT_FAILURE;
        }
    }

    /* Clean up on error. */
    if (status == EXIT_FAILURE)
    {
        if (mqttInitialized == true)
        {
            IotMqtt_Cleanup();
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Clean up the the MQTT library and the Shadow library.
 */
static void _cleanupDemo(void)
{
    AwsIotShadow_Cleanup();
    IotMqtt_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Establish a new connection to the MQTT server for the Shadow demo.
 *
 * @param[in] pIdentifier NULL-terminated MQTT client identifier. The Shadow
 * demo will use the Thing Name as the client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 * @param[out] pMqttConnection Set to the handle to the new MQTT connection.
 *
 * @return `EXIT_SUCCESS` if the connection is successfully established; `EXIT_FAILURE`
 * otherwise.
 */
static int _establishMqttConnection(const char *pIdentifier,
                                    void *pNetworkServerInfo,
                                    void *pNetworkCredentialInfo,
                                    const IotNetworkInterface_t *pNetworkInterface,
                                    IotMqttConnection_t *pMqttConnection)
{
    int status = EXIT_SUCCESS;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;

    if (pIdentifier == NULL)
    {
        IotLogError("Shadow Thing Name must be provided.");

        status = EXIT_FAILURE;
    }

    if (status == EXIT_SUCCESS)
    {
        /* Set the members of the network info not set by the initializer. This
         * struct provided information on the transport layer to the MQTT connection. */
        networkInfo.createNetworkConnection = true;
        networkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
        networkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
        networkInfo.pNetworkInterface = pNetworkInterface;

#if (IOT_MQTT_ENABLE_SERIALIZER_OVERRIDES == 1) && defined(IOT_DEMO_MQTT_SERIALIZER)
        networkInfo.pMqttSerializer = IOT_DEMO_MQTT_SERIALIZER;
#endif

        /* Set the members of the connection info not set by the initializer. */
        connectInfo.awsIotMqttMode = true;
        connectInfo.cleanSession = true;
        connectInfo.keepAliveSeconds = KEEP_ALIVE_SECONDS;

        /* AWS IoT recommends the use of the Thing Name as the MQTT client ID. */
        connectInfo.pClientIdentifier = pIdentifier;
        connectInfo.clientIdentifierLength = (uint16_t)strlen(pIdentifier);

        IotLogInfo("Shadow Thing Name is %.*s (length %hu).",
                   connectInfo.clientIdentifierLength,
                   connectInfo.pClientIdentifier,
                   connectInfo.clientIdentifierLength);

        /* Establish the MQTT connection. */
        connectStatus = IotMqtt_Connect(&networkInfo,
                                        &connectInfo,
                                        TIMEOUT_MS,
                                        pMqttConnection);

        if (connectStatus != IOT_MQTT_SUCCESS)
        {
            IotLogError("MQTT CONNECT returned error %s.",
                        IotMqtt_strerror(connectStatus));

            status = EXIT_FAILURE;
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

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
 * @param[in] pDeltaSemaphore Used to synchronize Shadow updates with the delta
 * callback.
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 * @param[in] thingNameLength The length of `pThingName`.
 *
 * @return `EXIT_SUCCESS` if all Shadow updates were sent; `EXIT_FAILURE`
 * otherwise.
 */
static int _reportShadow(IotSemaphore_t *pDeltaSemaphore, 
                         IotMqttConnection_t mqttConnection,
                         const char *const pThingName,
                         size_t thingNameLength)
{
    int status = EXIT_SUCCESS;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

    /* A buffer containing the update document. It has static duration to prevent
     * it from being placed on the call stack. */
    static char pUpdateDocument[EXPECTED_DESIRED_JSON_SIZE + 1] = {0};

    /* Set the common members of the Shadow update document info. */
    updateDocument.pThingName = pThingName;
    updateDocument.thingNameLength = thingNameLength;
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
        IotLogError("Failed to generate reported state document for Shadow update: %u vs. %u\n", status, EXPECTED_REPORTED_JSON_SIZE);
        IotLogError("%s\n", pUpdateDocument);
        status = EXIT_FAILURE;
        return status;
    }
    else
    {
        status = EXIT_SUCCESS;
    }

    if (IotSemaphore_TimedWait(pDeltaSemaphore, TIMEOUT_MS) == false)
    {
        IotLogError("Timed out trying to get the semaphore to report shadow.");

        status = EXIT_FAILURE;
        return status;
    }

    IotLogDebug("Sending Shadow update: %s", pUpdateDocument);

    /* Send the Shadow update. Because the Shadow is constantly updated in
     * this demo, the "Keep Subscriptions" flag is passed to this function.
     * Note that this flag only needs to be passed on the first call, but
     * passing it for subsequent calls is fine.
     */
    updateStatus = AwsIotShadow_TimedUpdate(mqttConnection,
                                            &updateDocument,
                                            AWS_IOT_SHADOW_FLAG_KEEP_SUBSCRIPTIONS,
                                            TIMEOUT_MS);

    IotSemaphore_Post(pDeltaSemaphore);

    /* Check the status of the Shadow update. */
    if (updateStatus != AWS_IOT_SHADOW_SUCCESS)
    {
        IotLogError("Failed to send Shadow update, error %s.", AwsIotShadow_strerror(updateStatus));
        status = EXIT_FAILURE;

        if (updateStatus == AWS_IOT_SHADOW_MQTT_ERROR)
        {
            /* connection could have died. Lets kill it. */
            lostConnection = true;
        }

        return status;
    }
    else
    {
        IotLogDebug("Successfully sent Shadow update.");
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
    int updateDocumentLength = 0;
    int status = 0;
    IotSemaphore_t *pDeltaSemaphore = pCallbackContext;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

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

        status = _reportShadow(pDeltaSemaphore,
                               pCallbackParam->mqttConnection,
                               pCallbackParam->pThingName,
                               pCallbackParam->thingNameLength);

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

/**
 * @brief Set the Shadow callback functions used in this demo.
 *
 * @param[in] pDeltaSemaphore Used to synchronize Shadow updates with the delta
 * callback.
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 * @param[in] thingNameLength The length of `pThingName`.
 *
 * @return `EXIT_SUCCESS` if all Shadow callbacks were set; `EXIT_FAILURE`
 * otherwise.
 */
static int _setShadowCallbacks(IotSemaphore_t *pDeltaSemaphore, 
                               IotMqttConnection_t mqttConnection,
                               const char *pThingName,
                               size_t thingNameLength)
{
    int status = EXIT_SUCCESS;
    AwsIotShadowError_t callbackStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowCallbackInfo_t deltaCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER,
                               updatedCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER;

    /* Set the functions for callbacks. */
    deltaCallback.pCallbackContext = pDeltaSemaphore;
    deltaCallback.function = _shadowDeltaCallback;
    updatedCallback.function = _shadowUpdatedCallback;

    /* Set the delta callback, which notifies of different desired and reported
     * Shadow states. */
    callbackStatus = AwsIotShadow_SetDeltaCallback(mqttConnection,
                                                   pThingName,
                                                   thingNameLength,
                                                   0,
                                                   &deltaCallback);

    if (callbackStatus != AWS_IOT_SHADOW_SUCCESS)
    {
        status = EXIT_FAILURE;
    }

    // if (callbackStatus == AWS_IOT_SHADOW_SUCCESS)
    // {
    //     /* Set the updated callback, which notifies when a Shadow document is
    //      * changed. */
    //     callbackStatus = AwsIotShadow_SetUpdatedCallback(mqttConnection,
    //                                                      pThingName,
    //                                                      thingNameLength,
    //                                                      0,
    //                                                      &updatedCallback);
    // }

    if (callbackStatus != AWS_IOT_SHADOW_SUCCESS)
    {
        IotLogError("Failed to set demo shadow callback, error %s.",
                    AwsIotShadow_strerror(callbackStatus));

        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Try to delete any Shadow document in the cloud.
 *
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Shadow Thing Name to delete.
 * @param[in] thingNameLength The length of `pThingName`.
 */
static void _clearShadowDocument(IotMqttConnection_t mqttConnection,
                                 const char *const pThingName,
                                 size_t thingNameLength)
{
    AwsIotShadowError_t deleteStatus = AWS_IOT_SHADOW_STATUS_PENDING;

    /* Delete any existing Shadow document so that this demo starts with an empty
     * Shadow. */
    deleteStatus = AwsIotShadow_TimedDelete(mqttConnection,
                                            pThingName,
                                            thingNameLength,
                                            0,
                                            TIMEOUT_MS);

    /* Check for return values of "SUCCESS" and "NOT FOUND". Both of these values
     * mean that the Shadow document is now empty. */
    if ((deleteStatus == AWS_IOT_SHADOW_SUCCESS) || (deleteStatus == AWS_IOT_SHADOW_NOT_FOUND))
    {
        IotLogInfo("Successfully cleared Shadow of %.*s.",
                   thingNameLength,
                   pThingName);
    }
    else
    {
        IotLogWarn("Shadow of %.*s not cleared.",
                   thingNameLength,
                   pThingName);
    }
}

/*-----------------------------------------------------------*/

static const TickType_t xAirConRefreshTimerFrequency_ms = 10000UL;
static TimerHandle_t xAirCon;

typedef struct {
    IotSemaphore_t * pSemaphore;
    IotMqttConnection_t mqttConnection;
    char * pThingName;
    size_t thingNameLength;
} timerId_t;

static void prvAirConTimerCallback(TimerHandle_t pxTimer)
{
    if (lostConnection == false)
    {
        int status = EXIT_SUCCESS;
        configASSERT(pxTimer);

        timerId_t * myTimerId = (timerId_t *)pvTimerGetTimerID(pxTimer);

        char pAirConStr[11] = {0};
        bool change = false;

        if (shadowStateReported.powerOn == 1 && 
            shadowStateReported.temperature != shadowStateDesired.temperature)
        {
            if (shadowStateReported.temperature > shadowStateDesired.temperature)
            {
                shadowStateReported.temperature--;
                change = true;
            }

            IotLogInfo("Timer: AirCon is ON => Temp (%u) needs to decrease to target (%u)",
                    shadowStateReported.temperature,
                    shadowStateDesired.temperature);

            status = snprintf(pAirConStr, 11, " ON %02u", shadowStateReported.temperature);
        }
        
        if (shadowStateReported.powerOn == 0 &&
            shadowStateReported.temperature != 40)
        {
            shadowStateReported.temperature++;
            if (shadowStateReported.temperature > 40)
            {
                shadowStateReported.temperature = 40;
            }

            change = true;

            IotLogInfo("Timer: AirCon is OFF => Temp (%u) increases", shadowStateReported.temperature);

            status = snprintf(pAirConStr, 11, "OFF %02u", shadowStateReported.temperature);
        }

        if (status >= 0 && change == true)
        {
            TFT_print(pAirConStr, M5DISPLAY_WIDTH - 6 * 9, M5DISPLAY_HEIGHT - 13);

            /* Report Shadow. */
            status = _reportShadow(myTimerId->pSemaphore,
                                myTimerId->mqttConnection,
                                myTimerId->pThingName,
                                myTimerId->thingNameLength);

            if (status != EXIT_SUCCESS)
            {
                IotLogError("Timer: Failed to report shadow.");
            }
        }
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief The function that runs the Shadow demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Ignored for the Shadow demo.
 * @param[in] pIdentifier NULL-terminated Shadow Thing Name.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection for Shadows.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection for Shadows.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int m5stickc_lab_shadow_run(bool awsIotMqttMode,
                            const char *pIdentifier,
                            void *pNetworkServerInfo,
                            void *pNetworkCredentialInfo,
                            const IotNetworkInterface_t *pNetworkInterface)
{
    /* Return value of this function and the exit status of this program. */
    int status = 0;

    /* Handle of the MQTT connection used in this demo. */
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Length of Shadow Thing Name. */
    size_t thingNameLength = 0;

    /* Allows the Shadow update function to wait for the delta callback to complete
     * a state change before continuing. */
    IotSemaphore_t deltaSemaphore;

    for(;;)
    {

        /* Flags for tracking which cleanup functions must be called. */
        bool librariesInitialized = false, connectionEstablished = false, deltaSemaphoreCreated = false;

        /* The first parameter of this demo function is not used. Shadows are specific
         * to AWS IoT, so this value is hardcoded to true whenever needed. */
        (void)awsIotMqttMode;

        /* Determine the length of the Thing Name. */
        if (pIdentifier != NULL)
        {
            thingNameLength = strlen(pIdentifier);

            if (thingNameLength == 0)
            {
                IotLogError("The length of the Thing Name (identifier) must be nonzero.");

                status = EXIT_FAILURE;
            }
        }
        else
        {
            IotLogError("A Thing Name (identifier) must be provided for the Shadow demo.");

            status = EXIT_FAILURE;
        }

        /* Initialize the libraries required for this demo. */
        if (status == EXIT_SUCCESS)
        {
            status = _initializeDemo();
        }

        if (status == EXIT_SUCCESS)
        {
            /* Mark the libraries as initialized. */
            librariesInitialized = true;

            /* Establish a new MQTT connection. */
            status = _establishMqttConnection(pIdentifier,
                                            pNetworkServerInfo,
                                            pNetworkCredentialInfo,
                                            pNetworkInterface,
                                            &mqttConnection);
        }

        if (status == EXIT_SUCCESS)
        {
            /* Mark the MQTT connection as established. */
            connectionEstablished = true;

            /* Mark the connection as NOT lost. */
            lostConnection = false;

            /* Create the semaphore that synchronizes with the delta callback. */
            deltaSemaphoreCreated = IotSemaphore_Create(&deltaSemaphore, 0, 1);

            if (deltaSemaphoreCreated == false)
            {
                status = EXIT_FAILURE;
            }
        }

        if (status == EXIT_SUCCESS)
        {
            /* Init the Semaphore to release it */
            IotSemaphore_Post(&deltaSemaphore);

            timerId_t myTimerId = {
                .pSemaphore = &deltaSemaphore,
                .mqttConnection = mqttConnection,
                .pThingName = pIdentifier,
                .thingNameLength = thingNameLength};

            /* Start the AirCon */
            xAirCon = xTimerCreate("AirConRefresh", pdMS_TO_TICKS(xAirConRefreshTimerFrequency_ms), pdTRUE, (void *)&myTimerId, prvAirConTimerCallback);
            xTimerStart(xAirCon, 0);
        }

        if (status == EXIT_SUCCESS)
        {
            IotLogInfo("Init, MQTT and Semaphore done. Setting up callbacks.");

            /* Set the Shadow callbacks for this demo. */
            status = _setShadowCallbacks(&deltaSemaphore,
                                        mqttConnection,
                                        pIdentifier,
                                        thingNameLength);
        }

        if (status == EXIT_SUCCESS)
        {
            /* Report Shadow. */
            status = _reportShadow(&deltaSemaphore, 
                                mqttConnection,
                                pIdentifier,
                                thingNameLength);
        }

        while (lostConnection == false)
        {
            vTaskDelay( pdMS_TO_TICKS(5000) );

            // Have we lost connection ?
            if (lostConnection == true)
            {
                break;
            }
        }

        ESP_LOGE(TAG, "Lost Connection.");

        /* Disconnect the MQTT connection if it was established. */
        if (connectionEstablished == true)
        {
            IotMqtt_Disconnect(mqttConnection, 0);
        }

        /* Clean up libraries if they were initialized. */
        if (librariesInitialized == true)
        {
            _cleanupDemo();
        }

        /* Destroy the delta semaphore if it was created. */
        if (deltaSemaphoreCreated == true)
        {
            IotSemaphore_Destroy(&deltaSemaphore);
        }

        // vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return status;
}

/*-----------------------------------------------------------*/

void m5stickc_lab2_start(void)
{
    static demoContext_t mqttDemoContext =
        {
            .networkTypes = democonfigNETWORK_TYPES,
            .demoFunction = m5stickc_lab_shadow_run,
            .networkConnectedCallback = vNetworkConnectedCallback,
            .networkDisconnectedCallback = vNetworkDisconnectedCallback};

    Iot_CreateDetachedThread(runDemoTask, &mqttDemoContext, democonfigDEMO_PRIORITY, democonfigDEMO_STACKSIZE);
}

/*-----------------------------------------------------------*/
