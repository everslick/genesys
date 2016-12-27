# GENESYS

Genesys is a complete framework for writing a ESP8266 firmware. It provides
a solid starting point for your own functions and makes it easy to develop,
debug and deploy firmware binaries. Genesys is built on top of the ESP8266
Arduino core and supports additional libraries like websockets and mqtt.

Genesys is targeted at serious developers and thus is best built with
make instead of Arduino IDE. Building with Arduino might still work, though.

Main Features
-------------
* timezone and daylight saving support
* XXTEA encryption for all passwords
* support for AT24C32 eeprom chip (on RTC module)
* login on serial port and telnet server
* cooperative multitasking for modules and tasks
* built in UNIX like shell with top, ls, mv, ps, kill, ...
* initial setup via soft accesspoint
* mobile friendly webinterface
* debug logs via UDP socket, serial port and file
* disable logs when compiling RELEASE version
* visualization of CPU load, memory usage and network traffic
* over the air updates (push and pull)
* support for websockets, MQTT, mDNS, SPIFFS, ...
* millisecond precision NTP and RTC implementation
* support static IP configuration as well as DHCP
* support factory reset via hardware button
* status leds

License
-------
Genesys is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Genesys is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Genesys.  If not, see <http://www.gnu.org/licenses/>.

Installation
============

Besides the ESP-Arduino-Core <https://github.com/esp8266/Arduino> Genesys
needs an additional 3rd party library that is:

* arduinoWebSockets <https://github.com/Links2004/arduinoWebSockets>

The following script can be used to set up the build environment and dependencies (change as needed):

```
mkdir -p ~/Devel/ESP8266
cd ~/Devel/ESP8266
git clone git@github.com:everslick/genesys
cd genesys
make env-setup
vi Makefile.config
make
```

Now you can create a file called Makefile.defaults and populate it with the
following content:

```
I18N_COUNTRY_CODE          = DE

DEFAULT_LOG_CHANNELS       = 7
DEFAULT_LOG_SERVER         = 10.0.0.150
DEFAULT_LOG_PORT           = 49152

DEFAULT_USER_NAME          = genesys
DEFAULT_USER_PASS          = genesys

DEFAULT_DEVICE_NAME        = genesys

DEFAULT_WIFI_ENABLED       = 1
DEFAULT_WIFI_SSID          = YOUR_WIFI_SSID
DEFAULT_WIFI_PASS          = YOUR_WIFI_PASS

DEFAULT_IP_STATIC          = 1
DEFAULT_IP_ADDR            = 10.0.0.222
DEFAULT_IP_NETMASK         = 255.255.255.0
DEFAULT_IP_GATEWAY         = 10.0.0.138
DEFAULT_IP_DNS1            = 10.0.0.138
DEFAULT_IP_DNS2            = 8.8.8.8

DEFAULT_AP_ENABLED         = 1
DEFAULT_AP_ADDR            = 10.1.1.1

DEFAULT_NTP_ENABLED        = 1
DEFAULT_NTP_SERVER         = pool.ntp.org
DEFAULT_NTP_INTERVAL       = 180

DEFAULT_TRANSPORT_ENABLED  = 1
DEFAULT_TRANSPORT_URL      = 10.0.0.150
DEFAULT_TRANSPORT_USER     = YOUR_MQTT_USER
DEFAULT_TRANSPORT_PASS     = YOUR_MQTT_PASS
DEFAULT_TRANSPORT_INTERVAL = 5

DEFAULT_UPDATE_ENABLED     = 1
DEFAULT_UPDATE_URL         = http://10.0.0.150/genesys/update.php
DEFAULT_UPDATE_INTERVAL    = 5

DEFAULT_STORAGE_ENABLED    = 1
DEFAULT_STORAGE_INTERVAL   = 10

DEFAULT_MDNS_ENABLED       = 1
DEFAULT_WEBSERVER_ENABLED  = 1
DEFAULT_WEBSOCKET_ENABLED  = 1
DEFAULT_TELNET_ENABLED     = 1
DEFAULT_GPIO_ENABLED       = 1
DEFAULT_RTC_ENABLED        = 1

DEFAULT_CPU_TURBO          = 1
```

Those are the default values that get compiled into the firmware binary so
you don't have to do a configuration cycle after each installation. These
values are also used for a factory reset (long press of PROG button on GPIO 0).

Build
-----
GNU/Make is used for the build process. The Makefile has some user configurable
variables in the top that you have to change according to your setup and
hardware such as the size and layout of your flash chip. For compilation and
deployment the following make targets can be used:

- make usb (build then upload via USB using esptool)
- make ota (build then update over the air using wget)
- make usblog (open serial terminal for debug logs using miniterm.py)
- make otalog (listen on a UDP port for incomming debug logs using netcat)
- make clean (remove all object and dependency files for a fresh build)
- make stack (paste a stack dump into vim to get a stacktrace)

SETUP
-----
After the ESP8266 has booted you can connect to the webinterface. Either via
the soft accesspoint of the ESP or, if you have set up the default configuration
properly, by its IP address or mDNS name. The soft accesspoint starts with the
device name as SSID or, if non is set yet with a hardcoded SSID of ESP8266-XXXXXX,
XXXXXX being the last 3 bytes of the MAC address.

If no username and password have been set yet, the webserver will redirect to
the initial configuration page. Here you can set the username/password for the
webserver, the WiFi to connect to as well as the WPA2 password, and its network
configuration (DHCP or static IP).

![setup](https://cloud.githubusercontent.com/assets/1909551/21504709/917e9814-cc61-11e6-85fd-81badd552d13.png)

Pressing [Save] stores the configuration in the EEPROM (flash) and reboots the
device. Connecting again will redirect you to the login page.

LOGIN
-----
After successful login you see the example hompage showing the firmware version,
uptime, current time (UTC) and the actual value from the ADC. The page gets
refreshed every two seconds.

![login](https://cloud.githubusercontent.com/assets/1909551/21504710/9184c004-cc61-11e6-8096-2f77ef331566.png)

HOME
----
The default home page shows the value of the ADC and the temperature of the RTC.
These values are updated over constantly over websockets without the need for
the page to be reloaded.

![home](https://cloud.githubusercontent.com/assets/1909551/21504713/9196f85a-cc61-11e6-9ee5-856d65bba08e.png)

CONF
----
The configuration page allows setting up of more parameters then was possible in
the initial setup. Here you can also define the mDNS name, MQTT publishing
intervals, or NTP servers.

![conf](https://cloud.githubusercontent.com/assets/1909551/21504715/91a3fa28-cc61-11e6-87f9-2b53b0fe9d44.png)

SYS
---
The [SYS] page gives insights to the resources currently consumed (heap, cpu, net)
and iallows individual modules to be loaded and unloaded at runtime.

![sys](https://cloud.githubusercontent.com/assets/1909551/21504704/91656132-cc61-11e6-90bf-2968828e5a03.png)

LOG
---

If the logger module is active the last loglines can be requested through the /log
page. Additionally the logger module can log to a file or send each line to a
remote host via UDP port 49152.

![log](https://cloud.githubusercontent.com/assets/1909551/21504711/918d24ec-cc61-11e6-9e8e-c7ecf37c1bcd.png)

INFO
----
When the firmware was not built as RELEASE version (by setting BUILD_RELEASE = 1
in the Makefile or Makefile.config), there are additional pages accessable that
provide information about the hardware and resource usage. For one there ist the
[Info] page, that shows static information about the firmware and hardware.

![info](https://cloud.githubusercontent.com/assets/1909551/21504712/919394ee-cc61-11e6-8b06-f52e1345bc93.png)

FILES
-----

The built in file browser allows access to the SPIFFS filesystem. Uploads are
also supported.

![files](https://cloud.githubusercontent.com/assets/1909551/21504714/919d3d46-cc61-11e6-9d96-61f0d61dfa06.png)

CLOCK
-----

The system clock can be either synchronized over NTP, an I2C real time clock or
with the browser time.

![clock](https://cloud.githubusercontent.com/assets/1909551/21504716/91a8cf9e-cc61-11e6-8939-37a43af7ad7a.png)

SHELL
-----
The command line interface is available over the serial console or via telnet.
Some unix like commands are implemented already.

![shell1](https://cloud.githubusercontent.com/assets/1909551/21504708/917a2c5c-cc61-11e6-852f-982608c73c75.png)

![shell2](https://cloud.githubusercontent.com/assets/1909551/21504707/91725e28-cc61-11e6-9508-adf62a19103f.png)

![shell3](https://cloud.githubusercontent.com/assets/1909551/21504706/916c7daa-cc61-11e6-948f-726b04c59ae1.png)

![shell4](https://cloud.githubusercontent.com/assets/1909551/21504705/91680194-cc61-11e6-9a85-6d818c599b54.png)

CLI
---
available commands are:
	init <m>     ... initialize module <m>
	fini <m>     ... finalize module <m>
	state [m]    ... query state of module [m]
	turbo [0|1]  ... switch cpu turbo mode on or off
	conf <k|k=v> ... get or set config key <k>
	save         ... save config to EEPROM
	format       ... create / filesystem
	ls           ... list filesystem content
	cat <f>      ... print content of file <f>
	rm <f>       ... remove file <f> from filesystem
	mv <f> <t>   ... rename file <f> to file <t>
	df           ... report file system disk space usage
	ntp          ... set system time from ntp server
	rtc          ... set system time from RTC
	date [d]     ... get/set time [YYYY/MM/DD HH:MM:SS]
	systohc      ... set RTC from system time
	uptime       ... get system uptime
	localtime    ... get local time
	adc          ... read ADC value
	toggle <p>   ... toggle GPIO pin <p>
	high <p>     ... set GPIO pin <p> high
	low <p>      ... set GPIO pin <p> low
	flash <l>    ... flash led <l> once
	pulse <l>    ... let led <l> blink
	on <l>       ... switch led <l> on
	off <l>      ... switch led <l> off
	ping <h>     ... send 3 ICMP ping requests to host <h>
	scan         ... scan WiFi for available accesspoints
	ps           ... list all currently running tasks
	kill <p>     ... terminate the task with PID <p>
	top          ... show runtime system usage statistics
	c64          ... 'hello, world!' demo
	clear        ... clear screen
	reboot       ... reboot device
	reset        ... perform factory reset
	help         ... print this info

The command line interface supports rudimentary TAB completion (based on the first letter typed) and command line interface hints. Hints are especially useful to give instant riminders on the syntax of a specific command.

TODO
----
* improved documentation
* move to async MQTT <https://github.com/marvinroger/async-mqtt-client>
* TLS support for MQTT
* WiFi mesh networking
* support configuration changes via MQTT subscription
* relais output via MQTT subscription

BUGS
----
* build with Arduino IDE fails in load.c complaining about size_t
