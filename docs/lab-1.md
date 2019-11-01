---
layout: default
title:  "Lab 1 - Create your own AWS IoT Button"
categories: [lab]
tags: [iot-core]
excerpt_separator: <!--more-->
---

A couple years ago, AWS announced the AWS IoT Button you could order from Amazon.com. Why not create our own ?

For this lab, our M5StickC will act as an AWS IoT Button, allowing you to trigger downstream AWS Services.

<!--more-->

## Configuration
In order to configure the code and your M5StickC for Lab1, you must change the following lines of code:

```bash
In file: ./amazon-freertos/vendors/espressif/boards/m5stickc/aws_demos/application_code/m5stickc_lab_config.h

Change the LAB define to: M5CONFIG_LAB1_AWS_IOT_BUTTON

Replace:
#define M5CONFIG_LAB0_DEEP_SLEEP_BUTTON_WAKEUP

With:
#define M5CONFIG_LAB1_AWS_IOT_BUTTON
```

## Rebuild Code
```bash
cd ./amazon-freertos/vendors/espressif/boards/m5stickc/aws_demos
make all -j4
```

## Flash Code
Please follow the procedure form the following [Setup](./flashing.html) guide.

## Result
Log into your AWS IoT Management console, and subscribe to the following topic:

```bash
m5stickc/+
```

* Click the button: You should see the following JSON document flow on the broker

```json
{
	"serialNumber": "[YOUR DEVICE SERIAL NUMBER]",
	"clickType": "SINGLE"
}
```

* Hold the button: You should see the following JSON document flow on the broker

```json
{
	"serialNumber": "[YOUR DEVICE SERIAL NUMBER]",
	"clickType": "HOLD"
}
```
