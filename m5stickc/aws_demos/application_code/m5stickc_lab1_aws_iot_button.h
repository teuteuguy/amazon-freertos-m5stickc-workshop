/**
 * @file m5stickc_lab1_aws_iot_button.h
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5STICKC_LAB1_AWS_IOT_BUTTON_H_
#define _M5STICKC_LAB1_AWS_IOT_BUTTON_H_

void m5stickc_lab1_init(const char *strID);
void m5stickc_lab1_action(const char *strID, int32_t buttonID);
// void m5stickc_lab1_cleanup( void );

// void m5stickc_lab1_init( void );
// void m5stickc_lab1_start( const char * strID, int32_t buttonID );
// void m5stickc_lab1_cleanup( void );

#endif /* ifndef _M5STICKC_LAB1_AWS_IOT_BUTTON_H_ */
