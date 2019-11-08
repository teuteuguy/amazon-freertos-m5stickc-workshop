/**
 * @file m5stickc_lab_connection.h
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5STICKC_LAB_CONNECTION_H_
#define _M5STICKC_LAB_CONNECTION_H_

typedef struct {
    char * strID;
    bool useShadow;
    networkConnectedCallback_t networkConnectedCallback;
    networkDisconnectedCallback_t networkDisconnectedCallback;
    void (*shadowDeltaCallback)(void *, AwsIotShadowCallbackParam_t *);
    void (*shadowUpdatedCallback)(void *, AwsIotShadowCallbackParam_t *);
} m5stickc_iot_connection_params_t;

esp_err_t m5stickc_lab_connection_init(m5stickc_iot_connection_params_t * params);
void m5stickc_lab_connection_wait(void);
void m5stickc_lab_connection_cleanup(void);

esp_err_t m5stickc_lab_connection_update_shadow(AwsIotShadowDocumentInfo_t *updateDocument);

#endif /* ifndef _M5STICKC_LAB_CONNECTION_H_ */
