---
layout: default
---

# Amazon Freertos workshop using the M5 StickC

Amazon FreeRTOS workshop using [M5StickC](https://docs.m5stack.com/#/en/core/m5stickc)


## Introduction

This workshop is designed to run on the M5StickC device. It includes multiple labs to get up and running.

- Lab0: Init device. Sleep. Wake On Button press.
- Lab1: Create your own AWS IoT Button.
- Lab2: Interract with the AWS IoT Thing Shadows.
- Lab3: WIP ... (Alexa?)

## Setup

There are multiple ways to run this workshop.

- [Set up your Cloud9 Environment](./markdown/cloud9.html)
- [Build on your laptop](./markdown/laptopbuild.html)


## Lab 0 - Deep Sleep

If you've followed the Setup guide, you should have nothing to do. The code first compiles to Lab 0

Your device you boot up, and directly go to sleep.

You can wake up the device by pressing the front button. This will trigger a full restart of the device, and device will go back to deep-sleep after a couple seconds.

## Lab 1 - Create your own AWS IoT Button

### Introduction
A couple years ago, AWS announced the AWS IoT Button you could order from Amazon.com. Why not create our own ?

For this lab, our M5StickC will act as an AWS IoT Button, allowing you to trigger downstream AWS Services.

### [Goto Lab 1](./markdown/lab1.html)

## Lab 2 - Interract with the Thing Shadow

### Introduction
It's the summer, and its hot outside. Lets play around with remove controlled Air Conditioning units.

For this lab, our M5StickC will act as our Air Conditioning unit.

You can turn the AirCon ON or OFF remotely as well as set a target temperature.

* If the AirCon is OFF, the temperature will rise to a stable 40 deg celcius.
* If the AirCon is ON, the temperature will decrease to reach your chosen temperature

### [Goto Lab 2](./markdown/lab2.html)


# Disclaimer
The following workshop material including documentation and code, is provided as is. You may incur AWS service costs for using the different resources outlined in the labs. Material is provided AS IS and is to be used at your own discretion. The author will not be responsible for any issues you may run into by using this material. 

If you have any feedback, suggestions, please use the issues section.