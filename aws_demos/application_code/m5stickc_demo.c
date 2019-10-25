/**
 * @file mystickc.c
 * @brief Demonstrates usage of the MQTT library.
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

#include "aws_demo.h"
#include "types/iot_network_types.h"
#include "semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "m5stickc.h"

#include "m5stickc_demo.h"

static const char *TAG = "m5stickc_demo";

#ifndef IOT_DEMO_MQTT_TOPIC_PREFIX
    #define IOT_DEMO_MQTT_TOPIC_PREFIX "m5stickc_demo"
#endif
/** @endcond */

/**
 * @brief The first characters in the client identifier. A timestamp is appended
 * to this prefix to create a unique client identifer.
 *
 * This prefix is also used to generate topic names and topic filters used in this
 * demo.
 */
#define CLIENT_IDENTIFIER_PREFIX                 "m5stickc_demo"

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
 * @brief The timeout for MQTT operations in this demo.
 */
#define MQTT_TIMEOUT_MS                          ( 5000 )

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

/* Declaration of demo functions. */
#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
int m5stickc_demo_aws_iot_button_single(bool awsIotMqttMode, const char *pIdentifier, void *pNetworkServerInfo, void *pNetworkCredentialInfo, const IotNetworkInterface_t *pNetworkInterface);
int m5stickc_demo_aws_iot_button_hold(bool awsIotMqttMode, const char *pIdentifier, void *pNetworkServerInfo, void *pNetworkCredentialInfo, const IotNetworkInterface_t *pNetworkInterface);
int m5stickc_demo_aws_iot_button(bool awsIotMqttMode, const char *pIdentifier, void *pNetworkServerInfo, void *pNetworkCredentialInfo, const IotNetworkInterface_t *pNetworkInterface, const char * pPayload);
#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

void vNetworkConnectedCallback( bool awsIotMqttMode,
                                const char * pIdentifier,
                                void * pNetworkServerInfo,
                                void * pNetworkCredentialInfo,
                                const IotNetworkInterface_t * pNetworkInterface );
void vNetworkDisconnectedCallback( const IotNetworkInterface_t * pNetworkInterface );


void init_sleep_timer(void);
void reset_sleep_timer(void);

esp_err_t m5stickc_demo_init(void);

/*-----------------------------------------------------------*/

uint8_t myStickCID[6] = { 0 };

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
        ESP_LOGI(TAG, "Reseting sleep timer");
        reset_sleep_timer();
        #endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

        #ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
        static demoContext_t mqttDemoContext =
            {
                .networkTypes = democonfigNETWORK_TYPES,
                .demoFunction = m5stickc_demo_aws_iot_button_single,
                .networkConnectedCallback = vNetworkConnectedCallback,
                .networkDisconnectedCallback = vNetworkDisconnectedCallback
            };

        Iot_CreateDetachedThread(runDemoTask, &mqttDemoContext, democonfigDEMO_PRIORITY, democonfigDEMO_STACKSIZE);
        #endif // M5CONFIG_LAB1_AWS_IOT_BUTTON
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
    #ifdef M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    TFT_print((char *)"LAB0 - SETUP & SLEEP", CENTER, SCREEN_LINE_4);
    #endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    #ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
    TFT_print((char *)"LAB1 - AWS IOT BUTTON", CENTER, SCREEN_LINE_4);
    #endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

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

    #ifdef M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP
    esp_sleep_enable_ext0_wakeup(M5BUTTON_BUTTON_A_GPIO, 0);
    init_sleep_timer();
    #endif // M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

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

/*-----------------------------------------------------------*/

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
    // gpio_set_level(LED, LED_OFF);
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
                            const char * pPayload,
                            size_t payloadLength )
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
    publishInfo.topicNameLength = TOPIC_BUFFER_LENGTH;
    publishInfo.pPayload = pPayload;
    publishInfo.payloadLength = payloadLength;
    publishInfo.pTopicName = pTopicName;
    publishInfo.retryMs = PUBLISH_RETRY_MS;
    publishInfo.retryLimit = PUBLISH_RETRY_LIMIT;

    /* PUBLISH a message. This is an asynchronous function that notifies of
    * completion through a callback. */
    publishStatus = IotMqtt_Publish( mqttConnection, &publishInfo, 0, &publishComplete, NULL );

    if( publishStatus != IOT_MQTT_STATUS_PENDING )
    {
        IotLogError( "MQTT PUBLISH returned error %s.", IotMqtt_strerror( publishStatus ) );
        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

#ifdef M5CONFIG_LAB1_AWS_IOT_BUTTON
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
int m5stickc_demo_aws_iot_button_single( bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface)
{
    return m5stickc_demo_aws_iot_button(awsIotMqttMode, pIdentifier, pNetworkServerInfo, pNetworkCredentialInfo, pNetworkInterface,
        PUBLISH_PAYLOAD_FORMAT_SINGLE );
}
int m5stickc_demo_aws_iot_button_hold( bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface)
{
    return m5stickc_demo_aws_iot_button(awsIotMqttMode, pIdentifier, pNetworkServerInfo, pNetworkCredentialInfo, pNetworkInterface,
        PUBLISH_PAYLOAD_FORMAT_HOLD );
}

int m5stickc_demo_aws_iot_button(bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface,
                 const char * pFormat )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    /* Handle of the MQTT connection used in this demo. */
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Topic and Payload buffers */
    char pTopic[ TOPIC_BUFFER_LENGTH ] = { 0 };
    char pPublishPayload[ PUBLISH_PAYLOAD_BUFFER_LENGTH ] = { 0 };
    size_t publishPayloadLength = 0;

    /* Generate the payload for the PUBLISH. */
    status = snprintf( pPublishPayload, PUBLISH_PAYLOAD_BUFFER_LENGTH, pFormat,
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
        status = EXIT_SUCCESS;
        publishPayloadLength = (size_t) status;

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
        status = _publishMessage( mqttConnection, pTopic, pPublishPayload, publishPayloadLength );

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

    return status;    
}

#endif // M5CONFIG_LAB1_AWS_IOT_BUTTON

/*-----------------------------------------------------------*/
