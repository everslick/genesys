-include Makefile.version

TARGETS = src

DIR := $(shell basename `pwd`)

MAKECMDGOALS ?= debug
MAKEFLAGS    += --silent

debug release clean ota usb otalog usblog check stack upload esp07:
	for TRG in $(TARGETS) ; do $(MAKE) --silent -C $$TRG $(MAKECMDGOALS) ; done

new: clean all

tar:
	cd .. && tar cvzf espade-$(VERSION).tar.gz $(DIR) --exclude-vcs

pull:
	echo "make pull is no longer supported!"
	echo "use 'make env-setup' (once) and 'make env-sync' (to update) instead"

env-setup:
	echo Setting up build environment ...
	git clone git@github.com:everslick/Arduino.git
	cd Arduino/tools && python get.py
	cd Arduino/libraries && git clone git@github.com:Links2004/arduinoWebSockets.git
	cd Arduino && git remote add upstream https://github.com/esp8266/Arduino.git
	cd .. && rm -rf Arduino
	mv Arduino ..
	echo DONE installing

env-pull:
	echo Pulling build environment from upstream master ...
	cd ../Arduino && git fetch upstream
	cd ../Arduino && git checkout master
	cd ../Arduino/libraries/arduinoWebSockets && git pull
	echo DONE pulling

env-sync: env-pull
	echo Syncing build environment with upstream master ...
	cd ../Arduino && git rebase upstream/master
	cd ../Arduino && git push -f origin master
	echo DONE syncing
