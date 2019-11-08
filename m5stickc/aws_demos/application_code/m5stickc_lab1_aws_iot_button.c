/**
 * @file m5stickc_lab1_aws_iot_button.c
 * @brief Lab1: Demonstrates the use of the MQTT library to publish on AWS IoT Core.
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"

/* MQTT include. */
#include "iot_mqtt.h"

/* Shadow include. */
#include "aws_iot_shadow.h"

#include "aws_demo.h"
#include "types/iot_network_types.h"
#include "esp_log.h"

#include "m5stickc_lab_config.h"
#include "m5stickc_lab_connection.h"
#include "m5stickc_lab1_aws_iot_button.h"

#include "m5stickc.h"

static const char *TAG = "m5stickc_lab1_aws_iot_button";

#ifndef IOT_DEMO_MQTT_TOPIC_PREFIX
    #define IOT_DEMO_MQTT_TOPIC_PREFIX "m5stickc"
#endif
/** @endcond */

/**
 * @brief The timeout for MQTT operations in this demo.
 */
#define MQTT_TIMEOUT_MS                          ( 5000 )

/**
 * @brief The first characters in the client identifier. A timestamp is appended
 * to this prefix to create a unique client identifer.
 *
 * This prefix is also used to generate topic names and topic filters used in this
 * demo.
 */
#define CLIENT_IDENTIFIER_PREFIX                 "m5stickc"

/**
 * @brief The longest client identifier that an MQTT server must accept (as defined
 * by the MQTT 3.1.1 spec) is 23 characters. Add 1 to include the length of the NULL
 * terminator.
 */
#define CLIENT_IDENTIFIER_MAX_LENGTH             ( 24 )

/**
 * @brief The keep-alive interval used for this demo.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#define KEEP_ALIVE_SECONDS                       ( 60 )

/**
 * @brief The Last Will and Testament topic name in this demo.
 *
 * The MQTT server will publish a message to this topic name if this client is
 * unexpectedly disconnected.
 */
#define WILL_TOPIC_NAME                          IOT_DEMO_MQTT_TOPIC_PREFIX "/will"

/**
 * @brief The length of #WILL_TOPIC_NAME.
 */
#define WILL_TOPIC_NAME_LENGTH                   ( ( uint16_t ) ( sizeof( WILL_TOPIC_NAME ) - 1 ) )

/**
 * @brief The message to publish to #WILL_TOPIC_NAME.
 */
#define WILL_MESSAGE                             "MQTT demo unexpectedly disconnected."

/**
 * @brief The length of #WILL_MESSAGE.
 */
#define WILL_MESSAGE_LENGTH                      ( ( size_t ) ( sizeof( WILL_MESSAGE ) - 1 ) )

/**
 * @brief The length of topic filter.
 *
 * For convenience, all topic filters are the same length.
 */
#define TOPIC_FORMAT                             IOT_DEMO_MQTT_TOPIC_PREFIX "/%s"
#define TOPIC_BUFFER_LENGTH                      ( ( uint16_t ) ( sizeof( TOPIC_FORMAT ) + 12 ) )

/**
 * @brief Format string of the PUBLISH messages in this demo.
 */
#define PUBLISH_PAYLOAD_FORMAT_SINGLE                   "{\"serialNumber\": \"%s\",\"clickType\": \"SINGLE\"}"
#define PUBLISH_PAYLOAD_FORMAT_HOLD                     "{\"serialNumber\": \"%s\",\"clickType\": \"HOLD\"}"

/**
 * @brief Size of the buffer that holds the PUBLISH messages in this demo.
 */
#define PUBLISH_PAYLOAD_BUFFER_LENGTH            ( sizeof( PUBLISH_PAYLOAD_FORMAT_SINGLE ) + 12 )

/**
 * @brief The maximum number of times each PUBLISH in this demo will be retried.
 */
#define PUBLISH_RETRY_LIMIT                      ( 10 )

/**
 * @brief A PUBLISH message is retried if no response is received within this
 * time.
 */
#define PUBLISH_RETRY_MS                         ( 1000 )

/**
 * @brief The topic name on which acknowledgement messages for incoming publishes
 * should be published.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME               IOT_DEMO_MQTT_TOPIC_PREFIX "/acknowledgements"

/**
 * @brief The length of #ACKNOWLEDGEMENT_TOPIC_NAME.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME_LENGTH        ( ( uint16_t ) ( sizeof( ACKNOWLEDGEMENT_TOPIC_NAME ) - 1 ) )

/**
 * @brief Format string of PUBLISH acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_FORMAT           "Client has received PUBLISH %.*s from server."

/**
 * @brief Size of the buffers that hold acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_BUFFER_LENGTH    ( sizeof( ACKNOWLEDGEMENT_MESSAGE_FORMAT ) + 2 )

/*-----------------------------------------------------------*/

void vLab1NetworkConnectedCallback( bool awsIotMqttMode,
                                const char * pIdentifier,
                                void * pNetworkServerInfo,
                                void * pNetworkCredentialInfo,
                                const IotNetworkInterface_t * pNetworkInterface )
{
    ESP_LOGD(TAG, "vNetworkConnectedCallback");
}

void vLab1NetworkDisconnectedCallback( const IotNetworkInterface_t * pNetworkInterface )
{
    ESP_LOGD(TAG, "vNetworkDisconnectedCallback");
}

/*-----------------------------------------------------------*/

/**
 * @brief Called by the MQTT library when an operation completes.
 *
 * The demo uses this callback to determine the result of PUBLISH operations.
 * @param[in] param1 The number of the PUBLISH that completed, passed as an intptr_t.
 * @param[in] pOperation Information about the completed operation passed by the
 * MQTT library.
 */
static void _operationCompleteCallback( void * param1,
                                        IotMqttCallbackParam_t * const pOperation )
{
    intptr_t publishCount = ( intptr_t ) param1;

    /* Silence warnings about unused variables. publishCount will not be used if
     * logging is disabled. */
    ( void ) publishCount;

    /* Print the status of the completed operation. A PUBLISH operation is
     * successful when transmitted over the network. */
    if( pOperation->u.operation.result == IOT_MQTT_SUCCESS )
    {
        IotLogInfo( "MQTT %s %d successfully sent.",
                    IotMqtt_OperationType( pOperation->u.operation.type ),
                    ( int ) publishCount );
    }
    else
    {
        IotLogError( "MQTT %s %d could not be sent. Error %s.",
                     IotMqtt_OperationType( pOperation->u.operation.type ),
                     ( int ) publishCount,
                     IotMqtt_strerror( pOperation->u.operation.result ) );
    }
}



/*-----------------------------------------------------------*/

/**
 * @brief Transmit message.
 *
 * @param[in] pTopicName The topic name for publishing.
 * @param[in] pPayload The payload for publishing.
 *
 * @return `EXIT_SUCCESS` if all messages are published; `EXIT_FAILURE` otherwise.
 */
static int _publishMessage( const char * pTopicName,
                            const char * pPayload)
{
    int status = EXIT_SUCCESS;

    IotMqttPublishInfo_t publishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    IotMqttCallbackInfo_t publishComplete = IOT_MQTT_CALLBACK_INFO_INITIALIZER;

    /* The MQTT library should invoke this callback when a PUBLISH message
     * is successfully transmitted. */
    publishComplete.function = _operationCompleteCallback;

    /* Set the common members of the publish info. */
    publishInfo.qos = IOT_MQTT_QOS_1;
    publishInfo.topicNameLength = strlen(pTopicName);
    publishInfo.pPayload = pPayload;
    publishInfo.payloadLength = strlen(pPayload);
    publishInfo.pTopicName = pTopicName;
    publishInfo.retryMs = PUBLISH_RETRY_MS;
    publishInfo.retryLimit = PUBLISH_RETRY_LIMIT;

    status = m5stickc_lab_connection_publish(&publishInfo, &publishComplete);

    return status;
}

/*-----------------------------------------------------------*/

void m5stickc_lab1_init(const char *const strID)
{
    static m5stickc_iot_connection_params_t connectionParams;

    connectionParams.strID = (char *)strID;
    connectionParams.useShadow = false;
    connectionParams.networkConnectedCallback = vLab1NetworkConnectedCallback;
    connectionParams.networkDisconnectedCallback = vLab1NetworkDisconnectedCallback;

    m5stickc_lab_connection_init(&connectionParams);
}

// void m5stickc_lab1_init1(void) 
// {
//     static demoContext_t mqttDemoContext =
//         {
//             .networkTypes = democonfigNETWORK_TYPES,
//             .demoFunction = m5stickc_lab1_aws_iot_button,
//             .networkConnectedCallback = vNetworkConnectedCallback,
//             .networkDisconnectedCallback = vNetworkDisconnectedCallback
//         };

//     // Create semaphore for connection readiness
//     if ( !IotSemaphore_Create( &connectionReadySem, 0, 1 ) )
//     {
//         IotLogError("Failed to create connection semaphore!");
//     }
    
//     // Create semaphore for connection readiness
//     if ( !IotSemaphore_Create( &cleanUpReadySem, 0, 1 ) )
//     {
//         IotLogError("Failed to create clean up semaphore!");
//     }

//     Iot_CreateDetachedThread(runDemoTask, &mqttDemoContext, democonfigDEMO_PRIORITY, democonfigDEMO_STACKSIZE);    
// }

/*-----------------------------------------------------------*/

void m5stickc_lab1_action( const char * strID, int32_t buttonID ) 
{
    ESP_LOGI(TAG, "m5stickc_lab1_action: %d", buttonID);

    m5stickc_lab_connection_ready_wait();
    
    /* Topic and Payload buffers */
    char pTopic[ TOPIC_BUFFER_LENGTH ] = { 0 };
    char pPublishPayload[ PUBLISH_PAYLOAD_BUFFER_LENGTH ] = { 0 };

    /* Generate the payload for the PUBLISH. */
    int status = -1;
    
    if ( buttonID == M5BUTTON_BUTTON_CLICK_EVENT ) 
    {
        status = snprintf( pPublishPayload, PUBLISH_PAYLOAD_BUFFER_LENGTH, PUBLISH_PAYLOAD_FORMAT_SINGLE, strID );
    }
    if ( buttonID == M5BUTTON_BUTTON_HOLD_EVENT ) 
    {
        status = snprintf( pPublishPayload, PUBLISH_PAYLOAD_BUFFER_LENGTH, PUBLISH_PAYLOAD_FORMAT_HOLD, strID );
    }

    /* Check for errors from snprintf. */
    if( status < 0 )
    {
        IotLogError( "Failed to generate MQTT PUBLISH payload: %d.", (int) status );
    }
    else
    {
        /* Generate the topic. */
        status = snprintf( pTopic, TOPIC_BUFFER_LENGTH, TOPIC_FORMAT, strID );

    }
    
    if ( status < 0 ) 
    {
        IotLogError( "Failed to generate MQTT payload topic: %d.", (int) status );
    }
    
    _publishMessage( pTopic, pPublishPayload );
}

/*-----------------------------------------------------------*/
