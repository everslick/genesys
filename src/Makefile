#==============================================================================
# makeESPArduino
#
# A makefile for ESP8286 Arduino projects.
# Edit the contents of this file to suit your project
# or just include it and override the applicable macros.
#
# License: GPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2016 Peter Lerup.
#               2016 Clemens Kirchgatterer
#                    All rights reserved.
#
#==============================================================================

-include ../Makefile.defaults
-include ../Makefile.config
-include ../Makefile.version

SKETCH = genesys.ino

LIBS = $(ESP_LIBS)/ESP8266WiFi       \
       $(ESP_LIBS)/ESP8266mDNS       \
       $(ESP_LIBS)/ESP8266HTTPClient \
       $(ESP_LIBS)/ESP8266httpUpdate \
       $(ESP_LIBS)/ESP8266WebServer  \
       $(ESP_LIBS)/DNSServer         \
       $(ESP_LIBS)/arduinoWebSockets \
       $(ESP_LIBS)/EEPROM            \
       $(ESP_LIBS)/Hash              \
       .

BUILD_SILENTLY             ?= 1
BUILD_LWIP_SRC             ?= 1
BUILD_SECURE_TELEMETRY     ?= 0

I18N_COUNTRY_CODE          ?= US

DEFAULT_DEVICE_NAME        ?= genesys

DEFAULT_WIFI_ENABLED       ?= 1
DEFAULT_WIFI_SSID          ?=
DEFAULT_WIFI_PASS          ?=

DEFAULT_IP_STATIC          ?= 0
DEFAULT_IP_ADDR            ?= 10.0.0.222
DEFAULT_IP_NETMASK         ?= 255.255.255.0
DEFAULT_IP_GATEWAY         ?= 10.0.0.138
DEFAULT_IP_DNS1            ?= 8.8.4.4
DEFAULT_IP_DNS2            ?= 8.8.8.8

DEFAULT_AP_ENABLED         ?= 1
DEFAULT_AP_ADDR            ?= 10.1.1.1

DEFAULT_NTP_ENABLED        ?= 1
DEFAULT_NTP_SERVER         ?= pool.ntp.org
DEFAULT_NTP_INTERVAL       ?= 180

DEFAULT_TELEMETRY_ENABLED  ?= 1
DEFAULT_TELEMETRY_URL      ?= 10.0.0.1
DEFAULT_TELEMETRY_USER     ?= YOUR_MQTT_USER
DEFAULT_TELEMETRY_PASS     ?= YOUR_MQTT_PASS
DEFAULT_TELEMETRY_INTERVAL ?= 5

DEFAULT_UPDATE_ENABLED     ?= 0
DEFAULT_UPDATE_URL         ?= http://10.0.0.1/genesys/update.php
DEFAULT_UPDATE_INTERVAL    ?= 5

DEFAULT_STORAGE_ENABLED    ?= 0
DEFAULT_STORAGE_MASK       ?= 0
DEFAULT_STORAGE_INTERVAL   ?= 900

DEFAULT_LOGGER_ENABLED     ?= 1
DEFAULT_LOGGER_CHANNELS    ?= 3
DEFAULT_LOGGER_HOST        ?= 10.0.0.1
DEFAULT_LOGGER_PORT        ?= 49152

DEFAULT_MDNS_ENABLED       ?= 1
DEFAULT_WEBSERVER_ENABLED  ?= 1
DEFAULT_WEBSOCKET_ENABLED  ?= 1
DEFAULT_TELNET_ENABLED     ?= 1
DEFAULT_GPIO_ENABLED       ?= 1
DEFAULT_RTC_ENABLED        ?= 1

DEFAULT_CPU_TURBO          ?= 0

FLASH_SIZE   ?= 1M # flash size in bytes
FLASH_MODE   ?= qio
FLASH_SPEED  ?= 40
FLASH_LAYOUT ?= ../flash.ld

UPLOAD_URL   ?= $(DEFAULT_DEVICE_NAME).local/update
UPLOAD_SIZE  ?= 8M # flash size in bits
UPLOAD_SPEED ?= 115200
UPLOAD_PORT  ?= /dev/ttyUSB0
UPLOAD_VERB  ?= -v

ESP_ROOT     ?= $(HOME)/Devel/ESP8266/Arduino
BUILD_ROOT   ?= /tmp/$(MAIN_NAME)

OPTIMIZE      = -Os
WARNINGS      = -w

DEFINES += -DFIRMWARE=\"$(VERSION)\"

DEFINES += -DI18N_COUNTRY_CODE=$(I18N_COUNTRY_CODE)

DEFINES += -DDEFAULT_USER_NAME=\"$(DEFAULT_USER_NAME)\"
DEFINES += -DDEFAULT_USER_PASS=\"$(DEFAULT_USER_PASS)\"

DEFINES += -DDEFAULT_DEVICE_NAME=\"$(DEFAULT_DEVICE_NAME)\"

DEFINES += -DDEFAULT_WIFI_ENABLED=$(DEFAULT_WIFI_ENABLED)
DEFINES += -DDEFAULT_WIFI_SSID=\"$(DEFAULT_WIFI_SSID)\"
DEFINES += -DDEFAULT_WIFI_PASS=\"$(DEFAULT_WIFI_PASS)\"

DEFINES += -DDEFAULT_IP_STATIC=$(DEFAULT_IP_STATIC)
DEFINES += -DDEFAULT_IP_ADDR=\"$(DEFAULT_IP_ADDR)\"
DEFINES += -DDEFAULT_IP_NETMASK=\"$(DEFAULT_IP_NETMASK)\"
DEFINES += -DDEFAULT_IP_GATEWAY=\"$(DEFAULT_IP_GATEWAY)\"
DEFINES += -DDEFAULT_IP_DNS1=\"$(DEFAULT_IP_DNS1)\"
DEFINES += -DDEFAULT_IP_DNS2=\"$(DEFAULT_IP_DNS2)\"

DEFINES += -DDEFAULT_AP_ENABLED=$(DEFAULT_AP_ENABLED)
DEFINES += -DDEFAULT_AP_ADDR=\"$(DEFAULT_AP_ADDR)\"

DEFINES += -DDEFAULT_NTP_ENABLED=$(DEFAULT_NTP_ENABLED)
DEFINES += -DDEFAULT_NTP_SERVER=\"$(DEFAULT_NTP_SERVER)\"
DEFINES += -DDEFAULT_NTP_INTERVAL=$(DEFAULT_NTP_INTERVAL)

DEFINES += -DDEFAULT_TELEMETRY_ENABLED=$(DEFAULT_TELEMETRY_ENABLED)
DEFINES += -DDEFAULT_TELEMETRY_URL=\"$(DEFAULT_TELEMETRY_URL)\"
DEFINES += -DDEFAULT_TELEMETRY_USER=\"$(DEFAULT_TELEMETRY_USER)\"
DEFINES += -DDEFAULT_TELEMETRY_PASS=\"$(DEFAULT_TELEMETRY_PASS)\"
DEFINES += -DDEFAULT_TELEMETRY_INTERVAL=$(DEFAULT_TELEMETRY_INTERVAL)

DEFINES += -DDEFAULT_UPDATE_ENABLED=$(DEFAULT_UPDATE_ENABLED)
DEFINES += -DDEFAULT_UPDATE_URL=\"$(DEFAULT_UPDATE_URL)\"
DEFINES += -DDEFAULT_UPDATE_INTERVAL=$(DEFAULT_UPDATE_INTERVAL)

DEFINES += -DDEFAULT_STORAGE_ENABLED=$(DEFAULT_STORAGE_ENABLED)
DEFINES += -DDEFAULT_STORAGE_MASK=$(DEFAULT_STORAGE_MASK)
DEFINES += -DDEFAULT_STORAGE_INTERVAL=$(DEFAULT_STORAGE_INTERVAL)

DEFINES += -DDEFAULT_LOGGER_ENABLED=$(DEFAULT_LOGGER_ENABLED)
DEFINES += -DDEFAULT_LOGGER_CHANNELS=$(DEFAULT_LOGGER_CHANNELS)
DEFINES += -DDEFAULT_LOGGER_HOST=\"$(DEFAULT_LOGGER_HOST)\"
DEFINES += -DDEFAULT_LOGGER_PORT=$(DEFAULT_LOGGER_PORT)

DEFINES += -DDEFAULT_MDNS_ENABLED=$(DEFAULT_MDNS_ENABLED)
DEFINES += -DDEFAULT_WEBSERVER_ENABLED=$(DEFAULT_WEBSERVER_ENABLED)
DEFINES += -DDEFAULT_WEBSOCKET_ENABLED=$(DEFAULT_WEBSOCKET_ENABLED)
DEFINES += -DDEFAULT_TELNET_ENABLED=$(DEFAULT_TELNET_ENABLED)
DEFINES += -DDEFAULT_GPIO_ENABLED=$(DEFAULT_GPIO_ENABLED)
DEFINES += -DDEFAULT_RTC_ENABLED=$(DEFAULT_RTC_ENABLED)

DEFINES += -DDEFAULT_CPU_TURBO=$(DEFAULT_CPU_TURBO)

C_DEFINES += $(DEFINES)

LD_STD_LIBS += -lhal -lphy -lpp -lnet80211 -lwpa -lcrypto -lmain -lwps -laxtls -lespnow -lsmartconfig -lmesh -lwpa2 -lstdc++ -lm -lc -lgcc

START_TIME     := $(shell perl -e "print time();")
# Main output definitions
MAIN_NAME       = $(basename $(notdir $(SKETCH)))
MAIN_EXE        = $(BUILD_ROOT)/$(MAIN_NAME).bin
MAIN_ELF        = $(OBJ_DIR)/$(MAIN_NAME).elf
SRC_GIT_VERSION = $(call git_description,$(dir $(SKETCH)))

# esp8266 arduino directories
ESP_GIT_VERSION = $(call git_description,$(ESP_ROOT))
ESP_LIBS        = $(ESP_ROOT)/libraries
TOOLS_ROOT      = $(ESP_ROOT)/tools
TOOLS_BIN       = $(TOOLS_ROOT)/xtensa-lx106-elf/bin
SDK_ROOT        = $(ESP_ROOT)/tools/sdk

# Directory for intermedite build files
OBJ_DIR         = $(BUILD_ROOT)/obj
OBJ_EXT         = .o
DEP_EXT         = .d

# Compiler definitions
CC              = $(TOOLS_BIN)/xtensa-lx106-elf-gcc
CPP             = $(TOOLS_BIN)/xtensa-lx106-elf-g++
LD              = $(CC)
AR              = $(TOOLS_BIN)/xtensa-lx106-elf-ar
GDB             = $(TOOLS_BIN)/xtensa-lx106-elf-gdb
ESP_TOOL        = $(TOOLS_ROOT)/esptool/esptool

MAKECMDGOALS ?= debug

LD_WRAP_FN    = -Wl,-wrap,malloc -Wl,-wrap,calloc -Wl,-wrap,realloc -Wl,-wrap,free -Wl,-wrap,tcp_recved -Wl,-wrap,tcp_write

INCLUDE_DIRS += $(SDK_ROOT)/include $(SDK_ROOT)/lwip/include $(SDK_ROOT)/libc/xtensa-lx106-elf/include $(CORE_DIR) $(ESP_ROOT)/variants/generic $(OBJ_DIR)

ifeq ($(MAKECMDGOALS),release)
  C_DEFINES    += -DRELEASE
  C_DEFINES    += -DQUIET
  OPTIMIZE      = -Os
else
  ifeq ($(MAKECMDGOALS),beta)
    C_DEFINES  += -DBETA
    OPTIMIZE    = -Os
  else
    C_DEFINES  += -DALPHA
  endif
endif

ifeq ($(BUILD_LWIP_SRC),1)
  LD_STD_LIBS  += -llwip_src
  LWIP_DEFINES  = $(C_DEFINES) -DLWIP_OPEN_SRC -DMEMP_NUM_TCP_PCB_TIME_WAIT=2
  LWIP_LIB      = $(SDK_ROOT)/lib/liblwip_src.a
else
  LD_STD_LIBS  += -llwip
endif

ifeq ($(BUILD_SILENTLY),1)
  MAKEFLAGS    += --silent
endif

ifeq ($(BUILD_SECURE_TELEMETRY),1)
  #LD_STD_LIBS  += -lssl
  C_DEFINES    += -DTELEMETRY_TLS_SUPPORT
endif

C_DEFINES    += -D__ets__ -DICACHE_FLASH -U__STRICT_ANSI__ -DF_CPU=80000000L -DARDUINO=10605 -DESP8266 -DNO_GLOBAL_INSTANCES

C_INCLUDES   += $(foreach dir,$(INCLUDE_DIRS) $(USER_INC),-I$(dir))

C_FLAGS      += -c $(OPTIMIZE) $(WARNINGS) -Wall -Wextra -Wpointer-arith -Wno-implicit-function-declaration -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals -falign-functions=4 -MMD -std=gnu99 -ffunction-sections -fdata-sections

CPP_FLAGS    += -c $(OPTIMIZE) $(WARNINGS) -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -MMD -ffunction-sections -fdata-sections

S_FLAGS      += -c $(OPTIMIZE) -x assembler-with-cpp -MMD -mlongcalls

LD_FLAGS     += $(OPTIMIZE) $(WARNINGS) -nostdlib -Wl,--no-check-sections -u call_user_start -u _printf_float -u _scanf_float -Wl,-static -L$(SDK_ROOT)/lib -L$(SDK_ROOT)/libc/xtensa-lx106-elf/lib -L$(SDK_ROOT)/ld -T$(FLASH_LAYOUT) -Wl,--gc-sections -Wl,-wrap,system_restart_local -Wl,-wrap,register_chipv6_phy $(LD_WRAP_FN)

# Core source files
CORE_DIR  = $(ESP_ROOT)/cores/esp8266
CORE_SRC  = $(shell find $(CORE_DIR) -name "*.S" -o -name "*.c" -o -name "*.cpp")
CORE_OBJ  = $(patsubst %,$(OBJ_DIR)/%$(OBJ_EXT),$(notdir $(CORE_SRC)))
CORE_LIB  = $(OBJ_DIR)/core.ar

# User defined compilation units
USER_SRC  = $(shell find $(LIBS) -name "*.S" -o -name "*.c" -o -name "*.cpp")
USER_H    = $(shell find $(LIBS) -name "*.h")
USER_DIRS = $(sort $(dir $(USER_SRC)))
USER_INC  = $(sort $(dir $(USER_H)))
USER_SRC += $(SKETCH)
# Object file suffix seems to be significant for the linker...
USER_OBJ  = $(subst .ino,.cpp,$(patsubst %,$(OBJ_DIR)/%$(OBJ_EXT),$(notdir $(USER_SRC))))

VPATH += $(shell find $(CORE_DIR) -type d) $(USER_DIRS)

# Automatically generated build information data
# Makes the build date and git descriptions at the actual build
# event available as string constants in the program
BUILD_INFO_H   = $(OBJ_DIR)/buildinfo.h
BUILD_INFO_CPP = $(OBJ_DIR)/buildinfo.cpp
BUILD_INFO_OBJ = $(BUILD_INFO_CPP)$(OBJ_EXT)
BUILD_DATE     = $(call time_string,"%Y-%m-%d")
BUILD_TIME     = $(call time_string,"%H:%M:%S")

$(BUILD_INFO_H): | $(OBJ_DIR)
	echo "typedef struct { const char *date, *time, *src_version, *env_version;} _tBuildInfo; extern _tBuildInfo _BuildInfo;" >$@

# Utility functions
git_description = $(shell git -C  $(1) describe --tags --always --dirty 2>/dev/null)
time_string = $(shell perl -e 'use POSIX qw(strftime); print strftime($(1), localtime());')

MEM_USAGE = \
  'while (<>) { \
    $$r += $$1 if /^\.(?:data|rodata|bss)\s+(\d+)/;\
    $$f += $$1 if /^\.(?:irom0\.text|text|data|rodata)\s+(\d+)/;\
  }\
  print "\nMemory usage\n";\
  print sprintf("  %-6s %6d bytes\n" x 2 ."\n", "Ram:", $$r, "Flash:", $$f);'

# Build rules
$(OBJ_DIR)/%.cpp$(OBJ_EXT): %.cpp $(BUILD_INFO_H)
	echo  $(<F)
	$(CPP) $(C_DEFINES) $(C_INCLUDES) $(CPP_FLAGS) $< -o $@

$(OBJ_DIR)/%.cpp$(OBJ_EXT): %.ino $(BUILD_INFO_H)
	echo  $(<F)
	$(CPP) -x c++ -include $(CORE_DIR)/Arduino.h $(C_DEFINES) $(C_INCLUDES) $(CPP_FLAGS) $< -o $@

$(OBJ_DIR)/%.c$(OBJ_EXT): %.c
	echo  $(<F)
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) $< -o $@

$(OBJ_DIR)/%.S$(OBJ_EXT): %.S
	echo  $(<F)
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(S_FLAGS) $< -o $@

$(CORE_LIB): $(CORE_OBJ)
	echo  Creating core archive
	rm -f $@
	$(AR) cru $@  $^

$(LWIP_LIB):
	echo
	echo -n 'Building lwip from source ... '
	cd $(SDK_ROOT)/lwip/src ; make BUILD_DEFINES="$(LWIP_DEFINES)" install
	echo done!
	echo

$(MAIN_EXE): $(CORE_LIB) $(USER_OBJ) $(LWIP_LIB)
	echo Linking $(MAIN_EXE)
	echo "  Versions: $(SRC_GIT_VERSION), $(ESP_GIT_VERSION)"
	echo 	'#include <buildinfo.h>' >$(BUILD_INFO_CPP)
	echo '_tBuildInfo _BuildInfo = {"$(BUILD_DATE)","$(BUILD_TIME)","$(SRC_GIT_VERSION)","$(ESP_GIT_VERSION)"};' >>$(BUILD_INFO_CPP)
	$(CPP) $(C_DEFINES) $(C_INCLUDES) $(CPP_FLAGS) $(BUILD_INFO_CPP) -o $(BUILD_INFO_OBJ)
	$(LD) $(LD_FLAGS) -Wl,--start-group $^ $(BUILD_INFO_OBJ) $(LD_STD_LIBS) -Wl,--end-group -L$(OBJ_DIR) -o $(MAIN_ELF)
	$(ESP_TOOL) -eo $(ESP_ROOT)/bootloaders/eboot/eboot.elf -bo $@ -bm $(FLASH_MODE) -bf $(FLASH_SPEED) -bz $(FLASH_SIZE) -bs .text -bp 4096 -ec -eo $(MAIN_ELF) -bs .irom0.text -bs .text -bs .data -bs .rodata -bc -ec
	$(TOOLS_BIN)/xtensa-lx106-elf-size -A $(MAIN_ELF) | perl -e $(MEM_USAGE)
	perl -e 'print "Build complete. Elapsed time: ", time()-$(START_TIME),  " seconds\n\n"'

upload: all
	$(ESP_TOOL) $(UPLOAD_VERB) -cd ck -cb $(UPLOAD_SPEED) -cp $(UPLOAD_PORT) -ca 0x00000 -cf $(MAIN_EXE)

clean:
	echo Removing all intermediate build files...
	cd $(SDK_ROOT)/lwip/src ; make clean
	rm -f $(LWIP_LIB)
	rm -f $(OBJ_DIR)/* 

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

ota: all
	curl $(UPLOAD_URL)
	sleep 1
	curl --progress-bar -F "image=@$(MAIN_EXE)" $(UPLOAD_URL) >/dev/null

usb: all
	killall picocom || true
	killall minicom || true
	$(MAKE) upload

log:
	picocom -b $(UPLOAD_SPEED) $(UPLOAD_PORT)

otalog:
	netcat -l -u -k -p 49152

usblog:
	minicom -c on -l -D $(UPLOAD_PORT) -b $(UPLOAD_SPEED) || true
	reset

check:
	cppcheck --platform=unix32 --enable=all --inconclusive --quiet -i load.c .

stack:
	rm -f stack.txt && vi +star stack.txt && awk '/>>>stack>>>/{flag=1;next}/<<<stack<<</{flag=0}flag' stack.txt | awk -e '{ OFS="\n"; $$1=""; print }' | $(TOOLS_BIN)/xtensa-lx106-elf-addr2line -aipfC -e $(MAIN_ELF) | grep -v "?? ??:0" ; rm -f stack.txt

.PHONY: all alpha beta release
all alpha beta release: $(OBJ_DIR) $(BUILD_INFO_H) $(MAIN_EXE)

# Include all available dependencies
-include $(wildcard $(OBJ_DIR)/*$(DEP_EXT))

.DEFAULT_GOAL = all
