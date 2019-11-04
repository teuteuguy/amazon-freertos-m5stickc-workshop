/**
 * @file m5stickc_lab1_aws_iot_button.h
 *
 * (C) 2019 - Timothee Cruse <timothee.cruse@gmail.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5STICKC_LAB1_AWS_IOT_BUTTON_H_
#define _M5STICKC_LAB1_AWS_IOT_BUTTON_H_

typedef void (* labFinishCallback_t)( void );

void m5stickc_lab1_start( labFinishCallback_t finishCallback );

#endif /* ifndef _M5STICKC_LAB1_AWS_IOT_BUTTON_H_ */
