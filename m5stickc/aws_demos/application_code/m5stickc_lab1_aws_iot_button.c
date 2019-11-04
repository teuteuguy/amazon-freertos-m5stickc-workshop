/**
 * @file m5stickc_lab1_aws_iot_button.c
 * @brief Lab1: Demonstrates the use of the MQTT library to publish on AWS IoT Core.
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"

/* MQTT include. */
#include "iot_mqtt.h"

#include "aws_demo.h"
#include "types/iot_network_types.h"
#include "esp_log.h"

#include "m5stickc_lab_config.h"
#include "m5stickc_lab1_aws_iot_button.h"

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
#define TOPIC_FORMAT                             IOT_DEMO_MQTT_TOPIC_PREFIX "/%02x%02x%02x%02x%02x%02x"
#define TOPIC_BUFFER_LENGTH                      ( ( uint16_t ) ( sizeof( TOPIC_FORMAT ) + 24 ) )

/**
 * @brief Format string of the PUBLISH messages in this demo.
 */
#define PUBLISH_PAYLOAD_FORMAT_SINGLE                   "{\"serialNumber\": \"%02x%02x%02x%02x%02x%02x\",\"clickType\": \"SINGLE\"}"
#define PUBLISH_PAYLOAD_FORMAT_HOLD                     "{\"serialNumber\": \"%02x%02x%02x%02x%02x%02x\",\"clickType\": \"HOLD\"}"

/**
 * @brief Size of the buffer that holds the PUBLISH messages in this demo.
 */
#define PUBLISH_PAYLOAD_BUFFER_LENGTH            ( sizeof( PUBLISH_PAYLOAD_FORMAT_SINGLE ) + 24 )

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

int m5stickc_lab1_aws_iot_button(
    bool awsIotMqttMode, 
    const char *pIdentifier,
    void *pNetworkServerInfo, 
    void *pNetworkCredentialInfo, 
    const IotNetworkInterface_t *pNetworkInterface, const char * pPayload);

void vNetworkConnectedCallback( bool awsIotMqttMode,
                                const char * pIdentifier,
                                void * pNetworkServerInfo,
                                void * pNetworkCredentialInfo,
                                const IotNetworkInterface_t * pNetworkInterface )
{
    ESP_LOGD(TAG, "vNetworkConnectedCallback");
}

void vNetworkDisconnectedCallback( const IotNetworkInterface_t * pNetworkInterface )
{
    ESP_LOGD(TAG, "vNetworkDisconnectedCallback");
}

labFinishCallback_t pFinishCallback = NULL;

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
 * @brief Initialize the MQTT library.
 *
 * @return `EXIT_SUCCESS` if all libraries were successfully initialized;
 * `EXIT_FAILURE` otherwise.
 */
static int _initializeDemo( void )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t mqttInitStatus = IOT_MQTT_SUCCESS;

    mqttInitStatus = IotMqtt_Init();

    if( mqttInitStatus != IOT_MQTT_SUCCESS )
    {
        /* Failed to initialize MQTT library. */
        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Clean up the MQTT library.
 */
static void _cleanupDemo( void )
{
    IotMqtt_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Establish a new connection to the MQTT server.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier.
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
static int _establishMqttConnection( bool awsIotMqttMode,
                                     const char * pIdentifier,
                                     void * pNetworkServerInfo,
                                     void * pNetworkCredentialInfo,
                                     const IotNetworkInterface_t * pNetworkInterface,
                                     IotMqttConnection_t * pMqttConnection )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    IotMqttPublishInfo_t willInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    char pClientIdentifierBuffer[ CLIENT_IDENTIFIER_MAX_LENGTH ] = { 0 };

    /* Set the members of the network info not set by the initializer. This
     * struct provided information on the transport layer to the MQTT connection. */
    networkInfo.createNetworkConnection = true;
    networkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
    networkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
    networkInfo.pNetworkInterface = pNetworkInterface;

    #if ( IOT_MQTT_ENABLE_SERIALIZER_OVERRIDES == 1 ) && defined( IOT_DEMO_MQTT_SERIALIZER )
        networkInfo.pMqttSerializer = IOT_DEMO_MQTT_SERIALIZER;
    #endif

    /* Set the members of the connection info not set by the initializer. */
    connectInfo.awsIotMqttMode = awsIotMqttMode;
    connectInfo.cleanSession = true;
    connectInfo.keepAliveSeconds = KEEP_ALIVE_SECONDS;
    connectInfo.pWillInfo = &willInfo;

    /* Set the members of the Last Will and Testament (LWT) message info. The
     * MQTT server will publish the LWT message if this client disconnects
     * unexpectedly. */
    willInfo.pTopicName = WILL_TOPIC_NAME;
    willInfo.topicNameLength = WILL_TOPIC_NAME_LENGTH;
    willInfo.pPayload = WILL_MESSAGE;
    willInfo.payloadLength = WILL_MESSAGE_LENGTH;

    /* Use the parameter client identifier if provided. Otherwise, generate a
     * unique client identifier. */
    if( pIdentifier != NULL )
    {
        connectInfo.pClientIdentifier = pIdentifier;
        connectInfo.clientIdentifierLength = ( uint16_t ) strlen( pIdentifier );
    }
    else
    {
        /* Every active MQTT connection must have a unique client identifier. The demos
         * generate this unique client identifier by appending a timestamp to a common
         * prefix. */
        status = snprintf( pClientIdentifierBuffer,
                           CLIENT_IDENTIFIER_MAX_LENGTH,
                           CLIENT_IDENTIFIER_PREFIX "%lu",
                           ( long unsigned int ) IotClock_GetTimeMs() );

        /* Check for errors from snprintf. */
        if( status < 0 )
        {
            IotLogError( "Failed to generate unique client identifier for demo." );
            status = EXIT_FAILURE;
        }
        else
        {
            /* Set the client identifier buffer and length. */
            connectInfo.pClientIdentifier = pClientIdentifierBuffer;
            connectInfo.clientIdentifierLength = ( uint16_t ) status;

            status = EXIT_SUCCESS;
        }
    }

    /* Establish the MQTT connection. */
    if( status == EXIT_SUCCESS )
    {
        IotLogInfo( "MQTT demo client identifier is %.*s (length %hu).",
                    connectInfo.clientIdentifierLength,
                    connectInfo.pClientIdentifier,
                    connectInfo.clientIdentifierLength );

        connectStatus = IotMqtt_Connect( &networkInfo,
                                         &connectInfo,
                                         MQTT_TIMEOUT_MS,
                                         pMqttConnection );

        if( connectStatus != IOT_MQTT_SUCCESS )
        {
            IotLogError( "MQTT CONNECT returned error %s.",
                         IotMqtt_strerror( connectStatus ) );

            status = EXIT_FAILURE;
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Transmit message.
 *
 * @param[in] mqttConnection The MQTT connection to use for publishing.
 * @param[in] pTopicName The topic name for publishing.
 * @param[in] pPayload The payload for publishing.
 *
 * @return `EXIT_SUCCESS` if all messages are published; `EXIT_FAILURE` otherwise.
 */
static int _publishMessage( IotMqttConnection_t mqttConnection,
                            const char * pTopicName,
                            uint16_t topicNameLength,
                            const char * pPayload,
                            uint16_t payloadLength )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t publishStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttPublishInfo_t publishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    IotMqttCallbackInfo_t publishComplete = IOT_MQTT_CALLBACK_INFO_INITIALIZER;

    /* The MQTT library should invoke this callback when a PUBLISH message
     * is successfully transmitted. */
    publishComplete.function = _operationCompleteCallback;

    /* Set the common members of the publish info. */
    publishInfo.qos = IOT_MQTT_QOS_1;
    publishInfo.topicNameLength = topicNameLength;
    publishInfo.pPayload = pPayload;
    publishInfo.payloadLength = payloadLength;
    publishInfo.pTopicName = pTopicName;
    publishInfo.retryMs = PUBLISH_RETRY_MS;
    publishInfo.retryLimit = PUBLISH_RETRY_LIMIT;

    /* PUBLISH a message. This is an asynchronous function that notifies of
    * completion through a callback. */
    ESP_LOGD(TAG, "Publish: %s (%u) on %s", pPayload, payloadLength, pTopicName);
    publishStatus = IotMqtt_Publish( mqttConnection, &publishInfo, 0, &publishComplete, NULL );

    if( publishStatus != IOT_MQTT_STATUS_PENDING )
    {
        IotLogError( "MQTT PUBLISH returned error %s.", IotMqtt_strerror( publishStatus ) );
        status = EXIT_FAILURE;
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
int m5stickc_lab1_aws_iot_button_single( bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface)
{
    return m5stickc_lab1_aws_iot_button(awsIotMqttMode, pIdentifier, pNetworkServerInfo, pNetworkCredentialInfo, pNetworkInterface,
        PUBLISH_PAYLOAD_FORMAT_SINGLE );
}
int m5stickc_lab1_aws_iot_button_hold( bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface)
{
    return m5stickc_lab1_aws_iot_button(awsIotMqttMode, pIdentifier, pNetworkServerInfo, pNetworkCredentialInfo, pNetworkInterface,
        PUBLISH_PAYLOAD_FORMAT_HOLD );
}

int m5stickc_lab1_aws_iot_button(bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface,
                 const char * payloadFormat )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    /* Handle of the MQTT connection used in this demo. */
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Topic and Payload buffers */
    char pTopic[ TOPIC_BUFFER_LENGTH ] = { 0 };
    char pPublishPayload[ PUBLISH_PAYLOAD_BUFFER_LENGTH ] = { 0 };
    uint16_t publishPayloadLength;

    /* Generate the payload for the PUBLISH. */
    status = snprintf( pPublishPayload, PUBLISH_PAYLOAD_BUFFER_LENGTH, payloadFormat,
                        myStickCID[0], myStickCID[1], myStickCID[2], myStickCID[3], myStickCID[4], myStickCID[5] );

    /* Check for errors from snprintf. */
    if( status < 0 )
    {
        IotLogError( "Failed to generate MQTT PUBLISH payload: %d.", (int) status );
        status = EXIT_FAILURE;
        return status;
    }
    else
    {
        publishPayloadLength = status;
        status = EXIT_SUCCESS;

        /* Generate the topic. */
        status = snprintf( pTopic, TOPIC_BUFFER_LENGTH, TOPIC_FORMAT, 
                    myStickCID[0], myStickCID[1], myStickCID[2], myStickCID[3], myStickCID[4], myStickCID[5] );

    }

    /* Check for errors from snprintf. */
    if( status < 0 )
    {
        IotLogError( "Failed to generate MQTT topic name for PUBLISH %d.", (int) status );
        status = EXIT_FAILURE;
    }
    else
    {
        status = EXIT_SUCCESS;
    }

    /* Flags for tracking which cleanup functions must be called. */
    bool librariesInitialized = false, connectionEstablished = false;

    if( status == EXIT_SUCCESS )
    {
        /* Initialize the libraries required for this demo. */
        status = _initializeDemo();
    }

    if( status == EXIT_SUCCESS )
    {
        /* Mark the libraries as initialized. */
        librariesInitialized = true;

        /* Establish a new MQTT connection. */
        status = _establishMqttConnection( awsIotMqttMode,
                                           pIdentifier,
                                           pNetworkServerInfo,
                                           pNetworkCredentialInfo,
                                           pNetworkInterface,
                                           &mqttConnection );
    }

    if( status == EXIT_SUCCESS )
    {
        /* Mark the MQTT connection as established. */
        connectionEstablished = true;

        /* PUBLISH message */
        status = _publishMessage( mqttConnection, pTopic, TOPIC_BUFFER_LENGTH, pPublishPayload, publishPayloadLength );

    }

    /* Disconnect the MQTT connection if it was established. */
    if( connectionEstablished == true )
    {
        IotMqtt_Disconnect( mqttConnection, 0 );
    }

    /* Clean up libraries if they were initialized. */
    if( librariesInitialized == true )
    {
        _cleanupDemo();
    }

    if ( pFinishCallback != NULL )
    {
        pFinishCallback();
    }

    return status;    
}

/*-----------------------------------------------------------*/

void m5stickc_lab1_start( labFinishCallback_t finishCallback ) 
{
    pFinishCallback = finishCallback;

    static demoContext_t mqttDemoContext =
        {
            .networkTypes = democonfigNETWORK_TYPES,
            .demoFunction = m5stickc_lab1_aws_iot_button_single,
            .networkConnectedCallback = vNetworkConnectedCallback,
            .networkDisconnectedCallback = vNetworkDisconnectedCallback
        };

    Iot_CreateDetachedThread(runDemoTask, &mqttDemoContext, democonfigDEMO_PRIORITY, democonfigDEMO_STACKSIZE);
}

/*-----------------------------------------------------------*/
