#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := aws_demo

ifndef AMAZON_FREERTOS_PATH
export AMAZON_FREERTOS_PATH := $(CURDIR)/amazon-freertos
endif

ifndef IDF_PATH
export IDF_PATH := $(AMAZON_FREERTOS_PATH)/vendors/espressif/esp-idf
EXTRA_COMPONENT_DIRS := $(CURDIR)/src/components $(CURDIR)/src/espressif_code

include $(IDF_PATH)/make/project.mk

else
$(error ERROR: IDF_PATH is defined in your environement, it will not point to vendors/espressif/esp-idf. To remove the variable run the command "unset IDF_PATH")
endif


