LOCAL_DIR := $(GET_LOCAL_DIR)

# Standalone port: project-level target headers from the merged custom output.
INCLUDES += -Iout/lk/include

PLATFORM := mediatek

MODULES += \
        dev/keys \
    lib/ptable

include out/lk/rules_platform.mk

