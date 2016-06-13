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

-include Makefile.config

VERSION = 0.0.1

SKETCH = genesys.ino

LIBS = $(ESP_LIBS)/ESP8266WiFi       \
       $(ESP_LIBS)/ESP8266mDNS       \
       $(ESP_LIBS)/ESPAsyncTCP       \
       $(ESP_LIBS)/ESPAsyncWebServer \
       $(ESP_LIBS)/DNSServer         \
       $(ESP_LIBS)/EEPROM            \
       $(ESP_LIBS)/Hash              \
       $(ESP_LIBS)/ArduinoOTA        \
       .

BUILD_SILENTLY        ?= 0
BUILD_RELEASE         ?= 0
BUILD_LWIP_SRC        ?= 0
BUILD_GDB_STUB        ?= 0
BUILD_SSL_MQTT        ?= 0

DEFAULT_LOG_CHANNELS  ?= 3
DEFAULT_LOG_SERVER    ?= 10.0.0.1
DEFAULT_LOG_PORT      ?= 49152

DEFAULT_WIFI_SSID     ?= YOUR_WIFI_SSID
DEFAULT_WIFI_PASS     ?= YOUR_WIFI_PASS

DEFAULT_IP_STATIC     ?= 0
DEFAULT_IP_ADDR       ?= 10.0.0.222
DEFAULT_IP_NETMASK    ?= 255.255.255.0
DEFAULT_IP_GATEWAY    ?= 10.0.0.138
DEFAULT_IP_DNS1       ?= 8.8.4.4
DEFAULT_IP_DNS2       ?= 8.8.8.8

DEFAULT_AP_ENABLED    ?= 1
DEFAULT_AP_ADDR       ?= 10.0.1.1

DEFAULT_MDNS_ENABLED  ?= 1
DEFAULT_MDNS_NAME     ?= genesys

DEFAULT_NTP_ENABLED   ?= 1
DEFAULT_NTP_SERVER    ?= pool.ntp.org
DEFAULT_NTP_INTERVAL  ?= 180

DEFAULT_MQTT_ENABLED  ?= 1
DEFAULT_MQTT_SERVER   ?= 10.0.0.1
DEFAULT_MQTT_USER     ?= YOUR_MQTT_USER
DEFAULT_MQTT_PASS     ?= YOUR_MQTT_PASS
DEFAULT_MQTT_INTERVAL ?= 5

UPDATE_URL   ?= $(DEFAULT_MDNS_NAME).local/update

FLASH_SIZE   ?= 2M
FLASH_MODE   ?= qio
FLASH_SPEED  ?= 40
FLASH_LAYOUT ?= eagle.flash.2m.ld

UPLOAD_SPEED ?= 115200
UPLOAD_PORT  ?= /dev/ttyUSB0
UPLOAD_VERB  ?= -v

ESP_ROOT     ?= $(HOME)/Devel/ESP8266/Arduino
BUILD_ROOT   ?= /tmp/$(MAIN_NAME)

OPTIMIZE      = -Os

DEFINES += -DFIRMWARE=\"$(VERSION)\"

DEFINES += -DDEFAULT_USER_NAME=\"$(DEFAULT_USER_NAME)\"
DEFINES += -DDEFAULT_USER_PASS=\"$(DEFAULT_USER_PASS)\"
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
DEFINES += -DDEFAULT_MDNS_ENABLED=$(DEFAULT_MDNS_ENABLED)
DEFINES += -DDEFAULT_MDNS_NAME=\"$(DEFAULT_MDNS_NAME)\"
DEFINES += -DDEFAULT_NTP_ENABLED=$(DEFAULT_NTP_ENABLED)
DEFINES += -DDEFAULT_NTP_SERVER=\"$(DEFAULT_NTP_SERVER)\"
DEFINES += -DDEFAULT_NTP_INTERVAL=$(DEFAULT_NTP_INTERVAL)
DEFINES += -DDEFAULT_MQTT_ENABLED=$(DEFAULT_MQTT_ENABLED)
DEFINES += -DDEFAULT_MQTT_SERVER=\"$(DEFAULT_MQTT_SERVER)\"
DEFINES += -DDEFAULT_MQTT_USER=\"$(DEFAULT_MQTT_USER)\"
DEFINES += -DDEFAULT_MQTT_PASS=\"$(DEFAULT_MQTT_PASS)\"
DEFINES += -DDEFAULT_MQTT_INTERVAL=$(DEFAULT_MQTT_INTERVAL)

DEFINES += -DDEFAULT_LOG_CHANNELS=$(DEFAULT_LOG_CHANNELS)
DEFINES += -DDEFAULT_LOG_SERVER=\"$(DEFAULT_LOG_SERVER)\"
DEFINES += -DDEFAULT_LOG_PORT=$(DEFAULT_LOG_PORT)

C_DEFINES += $(DEFINES)

LD_STD_LIBS += -lm -lgcc -lhal -lphy -lpp -lnet80211 -lcrypto -lmain -lwpa -lwpa2
LD_STD_LIBS += -laxtls -lsmartconfig -lmesh -lwps -lstdc++

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
AR              = $(TOOLS_BIN)/xtensa-lx106-elf-ar
GDB             = $(TOOLS_BIN)/xtensa-lx106-elf-gdb
LD              = $(CC)

ESP_TOOL        = $(TOOLS_ROOT)/esptool/esptool

ifeq ($(BUILD_RELEASE),1)
C_DEFINES   += -DRELEASE
endif

ifeq ($(BUILD_GDB_STUB),1)
LIBS        += $(ESP_LIBS)/GDBStub
C_DEFINES   += -DDEBUG
C_DEFINES   += -DGDBSTUB_CTRLC_BREAK=1
C_DEFINES   += -DGDBSTUB_USE_OWN_STACK=1
C_DEFINES   += -DGDBSTUB_REDIRECT_CONSOLE_OUTPUT=1
C_DEFINES   += -DGDBSTUB_BREAK_ON_INIT=0
C_DEFINES   += -DATTR_GDBFN=ICACHE_FLASH_ATTR
S_FLAGS     += -mlongcalls
OPTIMIZE     = -Og -ggdb
endif

ifeq ($(BUILD_LWIP_SRC),1)
LD_STD_LIBS += -llwip_src
else
LD_STD_LIBS += -llwip
endif

ifeq ($(BUILD_SILENTLY),1)
MAKEFLAGS   += --silent
endif

ifeq ($(BUILD_SSL_MQTT),1)
LD_STD_LIBS += -lssl
LIBS        += $(ESP_LIBS)/esp-mqtt-arduino-ssl
else
LIBS        += $(ESP_LIBS)/esp-mqtt-arduino
endif

ifeq ($(BUILD_LWIP_SRC),1)
LWIP_LIB     = $(SDK_ROOT)/lib/liblwip_src.a
LWIP_DEFINES = $(C_DEFINES) -DLWIP_OPEN_SRC -DLWIP_STATS=1
endif

INCLUDE_DIRS += $(SDK_ROOT)/include $(SDK_ROOT)/lwip/include $(CORE_DIR) $(ESP_ROOT)/variants/generic $(OBJ_DIR)

C_DEFINES    += -DLWIP_STATS=$(BUILD_LWIP_SRC) -D__ets__ -DICACHE_FLASH -U__STRICT_ANSI__ -DF_CPU=80000000L -DARDUINO=10605 -DARDUINO_ESP8266_ESP01 -DARDUINO_ARCH_ESP8266 -DESP8266

C_INCLUDES   += $(foreach dir,$(INCLUDE_DIRS) $(USER_INC),-I$(dir))

C_FLAGS      += -c $(OPTIMIZE) -Wpointer-arith -Wno-implicit-function-declaration -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals -falign-functions=4 -MMD -std=gnu99 -ffunction-sections -fdata-sections

CPP_FLAGS    += -c $(OPTIMIZE) -nostdlib -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -MMD -ffunction-sections -fdata-sections

S_FLAGS      += $(OPTIMIZE) -c -x assembler-with-cpp -MMD -mlongcalls

LD_FLAGS     += -w $(OPTIMIZE) -Wl,--no-check-sections -u call_user_start -Wl,-static -L$(SDK_ROOT)/lib -L$(SDK_ROOT)/ld -T$(FLASH_LAYOUT) -Wl,--gc-sections -Wl,-wrap,system_restart_local -Wl,-wrap,register_chipv6_phy -Wl,-wrap,malloc -Wl,-wrap,calloc -Wl,-wrap,realloc -Wl,-wrap,free

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
	curl --progress-bar -F "image=@$(MAIN_EXE)" $(UPDATE_URL) >/dev/null

usb: all
	killall miniterm.py || true
	$(MAKE) upload

otalog:
	netcat -l -u -k 49152

usblog:
	miniterm.py --raw $(UPLOAD_PORT) $(UPLOAD_SPEED) || true
	reset

gdb:
	rm -rf gdbcmds
	echo 'set remote hardware-breakpoint-limit 1' >> gdbcmds
	echo 'set remote hardware-watchpoint-limit 1' >> gdbcmds
	echo 'set debug xtensa 4' >> gdbcmds
	echo 'set serial baud $(UPLOAD_SPEED)' >> gdbcmds
	echo 'target remote $(UPLOAD_PORT)' >> gdbcmds
	$(GDB) -x gdbcmds

stack:
	rm -f stack.txt && vi stack.txt && awk '/>>>stack>>>/{flag=1;next}/<<<stack<<</{flag=0}flag' stack.txt | awk -e '{ OFS="\n"; $$1=""; print }' | $(TOOLS_BIN)/xtensa-lx106-elf-addr2line -aipfC -e $(MAIN_ELF) | grep -v "?? ??:0"

.PHONY: all
all: $(OBJ_DIR) $(BUILD_INFO_H) $(MAIN_EXE)

# Include all available dependencies
-include $(wildcard $(OBJ_DIR)/*$(DEP_EXT))

.DEFAULT_GOAL = all
