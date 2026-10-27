#
# LK project: ASUS MT6589 device (Fonepad 7 generation).
#
LOCAL_DIR := $(GET_LOCAL_DIR)

TARGET := mediatek

MODULES += app/mt_boot \
           out/lk

DEBUG := 0

DEFINES += WITH_DEBUG_UART=1
