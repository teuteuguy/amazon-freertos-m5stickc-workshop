/**
 * @file m5stickc_lab_connection.h
 * @brief Connection code for the library to be used commonly accross the different labs.
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

#include "m5stickc.h"

/*-----------------------------------------------------------*/

static const char *TAG = "m5stickc_lab_connection";

/*-----------------------------------------------------------*/

#define IOT_MQTT_TOPIC_PREFIX "m5stickc"

/**
 * @brief The timeout for MQTT operations.
 */
#define MQTT_TIMEOUT_MS (5000)

/**
 * @brief The first characters in the client identifier. A timestamp is appended
 * to this prefix to create a unique client identifer.
 *
 * This prefix is also used to generate topic names and topic filters used.
 */
#define CLIENT_IDENTIFIER_PREFIX "m5stickc"

/**
 * @brief The longest client identifier that an MQTT server must accept (as defined
 * by the MQTT 3.1.1 spec) is 23 characters. Add 1 to include the length of the NULL
 * terminator.
 */
#define CLIENT_IDENTIFIER_MAX_LENGTH (24)

/**
 * @brief The keep-alive interval.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#define KEEP_ALIVE_SECONDS (60)

/**
 * @brief The Last Will and Testament topic name.
 *
 * The MQTT server will publish a message to this topic name if this client is
 * unexpectedly disconnected.
 */
#define LWT_TOPIC_NAME_FORMAT IOT_MQTT_TOPIC_PREFIX "/%s/lwt"

/**
 * @brief The length of #LWT_TOPIC_NAME.
 */
#define LWT_TOPIC_NAME_MAX_LENGTH ((uint16_t)(sizeof(LWT_TOPIC_NAME_FORMAT) - 1 + 12))

/**
 * @brief The message to publish to #LWT_TOPIC_NAME.
 */
#define LWT_MESSAGE "{\"message\": \"disconnected\"}"

/**
 * @brief The length of #LWT_MESSAGE.
 */
#define LWT_MESSAGE_LENGTH ((size_t)(sizeof(LWT_MESSAGE) - 1))

/*-----------------------------------------------------------*/

/* Semaphore for connection readiness */
static IotSemaphore_t connectionReadySem;

/* Semaphore for connection library / SDK clean up */
static IotSemaphore_t cleanUpReadySem;

/* Semaphore for shadow delta management */ 
static IotSemaphore_t shadowDeltaSem;

/* Handle of the MQTT connection used in this demo. */
static IotMqttConnection_t _mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

static m5stickc_iot_connection_params_t *_pConnectionParams = NULL;

/* boolean flag for connection established */
static bool connectionEstablished = false;

/*-----------------------------------------------------------*/

/**
 * @brief Initialize the the MQTT library and the Shadow library.
 *
 * @return `EXIT_SUCCESS` if all libraries were successfully initialized;
 * `EXIT_FAILURE` otherwise.
 */
static int _initialize(void)
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
static void _cleanup(void)
{
    AwsIotShadow_Cleanup();
    IotMqtt_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Set the Shadow callback functions used in this demo.
 *
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 *
 * @return `EXIT_SUCCESS` if all Shadow callbacks were set; `EXIT_FAILURE`
 * otherwise.
 */
static int _setShadowCallbacks(const char *pThingName)
{
    int status = EXIT_SUCCESS;
    AwsIotShadowError_t callbackStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowCallbackInfo_t deltaCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER;
    AwsIotShadowCallbackInfo_t updatedCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER;

    /* Set the functions for callbacks. */
    deltaCallback.pCallbackContext = &shadowDeltaSem;
    deltaCallback.function = _pConnectionParams->shadowDeltaCallback;
    updatedCallback.function = _pConnectionParams->shadowUpdatedCallback;

    if (_pConnectionParams->shadowDeltaCallback != NULL)
    {
        /* Set the delta callback, which notifies of different desired and reported
         * Shadow states. */
        callbackStatus = AwsIotShadow_SetDeltaCallback(_mqttConnection,
                                                    pThingName,
                                                    strlen(pThingName),
                                                    0,
                                                    &deltaCallback);

        if (callbackStatus != AWS_IOT_SHADOW_SUCCESS)
        {
            IotLogError("Failed to set shadow callback, error %s.",
                        AwsIotShadow_strerror(callbackStatus));
            status = EXIT_FAILURE;
        }
    }

    if (_pConnectionParams->shadowUpdatedCallback != NULL && callbackStatus == AWS_IOT_SHADOW_SUCCESS)
    {
        /* Set the updated callback, which notifies when a Shadow document is
         * changed. */
        callbackStatus = AwsIotShadow_SetUpdatedCallback(_mqttConnection,
                                                         pThingName,
                                                         strlen(pThingName),
                                                         0,
                                                         &updatedCallback);
        if (callbackStatus != AWS_IOT_SHADOW_SUCCESS)
        {
            IotLogError("Failed to set shadow callback, error %s.",
                        AwsIotShadow_strerror(callbackStatus));
            status = EXIT_FAILURE;
        }
    }

    return status;
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
 *
 * @return `EXIT_SUCCESS` if the connection is successfully established; `EXIT_FAILURE`
 * otherwise.
 */
static int _establishMqttConnection(const char *pIdentifier,
                                    void *pNetworkServerInfo,
                                    void *pNetworkCredentialInfo,
                                    const IotNetworkInterface_t *pNetworkInterface)
{
    int status = EXIT_SUCCESS;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    IotMqttPublishInfo_t lwtInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    char pClientIdentifierBuffer[CLIENT_IDENTIFIER_MAX_LENGTH] = {0};
    char pLwtBuffer[LWT_TOPIC_NAME_MAX_LENGTH] = {0};

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
        connectInfo.pWillInfo = &lwtInfo;

        status = snprintf(pLwtBuffer,
                          LWT_TOPIC_NAME_MAX_LENGTH,
                          LWT_TOPIC_NAME_FORMAT,
                          _pConnectionParams->strID);

    }

    /* Check for errors from snprintf. */
    if (status < 0)
    {
        ESP_LOGE(TAG, "Failed to generate the LWT topic name.");
        status = EXIT_FAILURE;
    }
    else
    {
        status = EXIT_SUCCESS;

        /* Set the members of the Last Will and Testament (LWT) message info. The
         * MQTT server will publish the LWT message if this client disconnects
         * unexpectedly. */
        lwtInfo.pTopicName = pLwtBuffer;
        lwtInfo.topicNameLength = strlen(pLwtBuffer);
        lwtInfo.pPayload = LWT_MESSAGE;
        lwtInfo.payloadLength = LWT_MESSAGE_LENGTH;
    }

    if (_pConnectionParams->useShadow == true && pIdentifier == NULL)
    {
        ESP_LOGE(TAG, "Shadow Thing Name must be provided.");

        status = EXIT_FAILURE;
    }

    /* Use the parameter client identifier if provided. Otherwise, generate a
     * unique client identifier. */
    if (pIdentifier != NULL)
    {
        connectInfo.pClientIdentifier = pIdentifier;
        connectInfo.clientIdentifierLength = (uint16_t)strlen(pIdentifier);
    }
    else
    {
        /* Every active MQTT connection must have a unique client identifier. The demos
            * generate this unique client identifier by appending a timestamp to a common
            * prefix. */
        status = snprintf(pClientIdentifierBuffer,
                            CLIENT_IDENTIFIER_MAX_LENGTH,
                            CLIENT_IDENTIFIER_PREFIX "%lu",
                            (long unsigned int)IotClock_GetTimeMs());

        /* Check for errors from snprintf. */
        if (status < 0)
        {
            IotLogError("Failed to generate unique client identifier for demo.");
            status = EXIT_FAILURE;
        }
        else
        {
            /* Set the client identifier buffer and length. */
            connectInfo.pClientIdentifier = pClientIdentifierBuffer;
            connectInfo.clientIdentifierLength = (uint16_t)status;

            status = EXIT_SUCCESS;
        }
    }

    /* Establish the MQTT connection. */
    if ( status == EXIT_SUCCESS )
    {
        IotLogInfo("MQTT client identifier is %.*s (length %hu).",
                   connectInfo.clientIdentifierLength,
                   connectInfo.pClientIdentifier,
                   connectInfo.clientIdentifierLength);

        connectStatus = IotMqtt_Connect(&networkInfo,
                                        &connectInfo,
                                        MQTT_TIMEOUT_MS,
                                        &_mqttConnection);

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
/**
 * @brief The function that runs the MQTT demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */

int m5stickc_lab_run(bool awsIotMqttMode,
                     const char *pIdentifier,
                     void *pNetworkServerInfo,
                     void *pNetworkCredentialInfo,
                     const IotNetworkInterface_t *pNetworkInterface)
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    /* Flags for tracking which cleanup functions must be called. */
    bool librariesInitialized = false;

    connectionEstablished = false;

    /* The first parameter of this demo function is not used. Shadows are specific
     * to AWS IoT, so this value is hardcoded to true whenever needed. */
    (void)awsIotMqttMode;

    /* Determine the length of the Thing Name. */
    if (pIdentifier != NULL)
    {
        int thingNameLength = strlen(pIdentifier);

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

    if (status == EXIT_SUCCESS)
    {
        /* Initialize the libraries required for this demo. */
        status = _initialize();
    }

    if (status == EXIT_SUCCESS)
    {
        /* Mark the libraries as initialized. */
        librariesInitialized = true;

        /* Establish a new MQTT connection. */
        status = _establishMqttConnection(pIdentifier,
                                          pNetworkServerInfo,
                                          pNetworkCredentialInfo,
                                          pNetworkInterface);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize: %i", status);
    }

    if (status == EXIT_SUCCESS)
    {
        /* Mark the MQTT connection as established. */
        connectionEstablished = true;

        /* Set the Shadow callbacks for this demo. */
        status = _setShadowCallbacks(pIdentifier);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize the MQTT Connection: %i", status);
    }

    // Unhook connection readiness semaphore
    IotSemaphore_Post(&connectionReadySem);
    // Unhook shadow delta semaphore
    IotSemaphore_Post(&shadowDeltaSem);

    IotLogInfo("Waiting for connection clean up signal...");
    // Wait for clean up semaphore
    IotSemaphore_Wait(&cleanUpReadySem);
    IotSemaphore_Destroy(&cleanUpReadySem);

    IotLogInfo("Received connection clean up signal.");

    // Cleanup shadow delta semaphore
    IotSemaphore_Destroy(&shadowDeltaSem);

    /* Disconnect the MQTT connection if it was established. */
    if (connectionEstablished == true)
    {
        IotMqtt_Disconnect(_mqttConnection, 0);
        connectionEstablished = false;
    }

    /* Clean up libraries if they were initialized. */
    if (librariesInitialized == true)
    {
        _cleanup();
    }

    return status;
}

/*-----------------------------------------------------------*/

esp_err_t m5stickc_lab_connection_init(m5stickc_iot_connection_params_t * pConnectionParams)
{
    esp_err_t res = EXIT_SUCCESS;

    _pConnectionParams = pConnectionParams;

    static demoContext_t mqttDemoContext =
    {
        .networkTypes = democonfigNETWORK_TYPES,
        .demoFunction = m5stickc_lab_run,
        .networkConnectedCallback = NULL,
        .networkDisconnectedCallback = NULL
    };

    mqttDemoContext.networkConnectedCallback = _pConnectionParams->networkConnectedCallback;
    mqttDemoContext.networkDisconnectedCallback = _pConnectionParams->networkDisconnectedCallback;

    // Create semaphore for connection readiness
    if (res == EXIT_SUCCESS && !IotSemaphore_Create(&connectionReadySem, 0, 1))
    {
        ESP_LOGE(TAG, "Failed to create connection semaphore!");
        res = EXIT_FAILURE;
    }
    
    // Create semaphore for connection readiness
    if ( res == EXIT_SUCCESS && !IotSemaphore_Create(&cleanUpReadySem, 0, 1))
    {
        ESP_LOGE(TAG, "Failed to create clean up semaphore!");
        res = EXIT_FAILURE;
    }

    // Create semaphore for shadow delta
    if ( res == EXIT_SUCCESS && !IotSemaphore_Create(&shadowDeltaSem, 0, 1))
    {
        ESP_LOGE(TAG, "Failed to create shadow delta semaphore!");
        res = EXIT_FAILURE;
    }

    if ( res == EXIT_SUCCESS )
    {
        ESP_LOGI(TAG, "Creating IoT Thread");
        res = Iot_CreateDetachedThread(runDemoTask, &mqttDemoContext, democonfigDEMO_PRIORITY, democonfigDEMO_STACKSIZE);        
    }

    return res;
}

/*-----------------------------------------------------------*/

esp_err_t m5stickc_lab_connection_update_shadow(AwsIotShadowDocumentInfo_t *updateDocument)
{
    int status = EXIT_SUCCESS;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;

    IotSemaphore_Wait(&shadowDeltaSem);

    updateStatus = AwsIotShadow_TimedUpdate(_mqttConnection,
                                            updateDocument,
                                            AWS_IOT_SHADOW_FLAG_KEEP_SUBSCRIPTIONS,
                                            MQTT_TIMEOUT_MS);

    /* Check the status of the Shadow update. */
    if (updateStatus != AWS_IOT_SHADOW_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to send Shadow update, error %s.", AwsIotShadow_strerror(updateStatus));
        status = EXIT_FAILURE;
    }
    else
    {
        ESP_LOGD(TAG, "Successfully sent Shadow update.");
    }

    IotSemaphore_Post(&shadowDeltaSem);

    return status;
}

/*-----------------------------------------------------------*/

esp_err_t m5stickc_lab_connection_publish(IotMqttPublishInfo_t * publishInfo, IotMqttCallbackInfo_t * publishComplete)
{
    int status = EXIT_SUCCESS;
    IotMqttError_t publishStatus = IOT_MQTT_STATUS_PENDING;

    if (NULL != _mqttConnection)
    {
        /* PUBLISH a message. This is an asynchronous function that notifies of
         * completion through a callback. */
        ESP_LOGI(TAG, "MQTT Publish: %s: %s", (char *)publishInfo->pTopicName, (char *)publishInfo->pPayload);

        publishStatus = IotMqtt_Publish(_mqttConnection, publishInfo, 0, publishComplete, NULL);

        if (publishStatus != IOT_MQTT_STATUS_PENDING)
        {
            ESP_LOGE(TAG, "MQTT Publish returned error %s.", IotMqtt_strerror(publishStatus));
            status = EXIT_FAILURE;
        }
    }
    else
    {
        ESP_LOGE(TAG, "MQTT Publish: MQTT Connection is (NULL) not available.");
        status = EXIT_FAILURE;
    }
        
    return status;
}
/*-----------------------------------------------------------*/

void m5stickc_lab_connection_ready_wait(void)
{
    IotSemaphore_Wait( &connectionReadySem );
    IotSemaphore_Post( &connectionReadySem );
}

void m5stickc_lab_connection_cleanup(void)
{
    IotSemaphore_Post(&cleanUpReadySem);
}

/**
 * @brief Try to delete any Shadow document in the cloud.
 *
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Shadow Thing Name to delete.
 * @param[in] thingNameLength The length of `pThingName`.
 */
// static void _clearShadowDocument(IotMqttConnection_t mqttConnection,
//                                  const char *const pThingName,
//                                  size_t thingNameLength)
// {
//     AwsIotShadowError_t deleteStatus = AWS_IOT_SHADOW_STATUS_PENDING;

//     /* Delete any existing Shadow document so that this demo starts with an empty
//      * Shadow. */
//     deleteStatus = AwsIotShadow_TimedDelete(mqttConnection,
//                                             pThingName,
//                                             thingNameLength,
//                                             0,
//                                             TIMEOUT_MS);

//     /* Check for return values of "SUCCESS" and "NOT FOUND". Both of these values
//      * mean that the Shadow document is now empty. */
//     if ((deleteStatus == AWS_IOT_SHADOW_SUCCESS) || (deleteStatus == AWS_IOT_SHADOW_NOT_FOUND))
//     {
//         IotLogInfo("Successfully cleared Shadow of %.*s.",
//                    thingNameLength,
//                    pThingName);
//     }
//     else
//     {
//         IotLogWarn("Shadow of %.*s not cleared.",
//                    thingNameLength,
//                    pThingName);
//     }
// }