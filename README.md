# GENESYS

Genesys is a complete framework for writing a ESP8266 firmware. It provides
a solid starting point for your own functions and makes it easy to develop,
debug and deploy firmware binaries. Genesys is built on top of the ESP8266
Arduino core and supports additional libraries like websockets and mqtt.

Genesys is targeted at serious developers and thus is best built with
make instead of Arduino IDE. Building with Arduino might still work, though.

Main Features
-------------
* initial setup via soft accesspoint
* username/password authentication for webserver
* debug logs via UDP socket and/or serial port
* disable logs when compiling RELEASE version
* allows commands to be sent via debug log back channel
* rudimentary visualization of CPU load, memory usage and network traffic
* over the air updates
* support for websockets, MQTT, mDNS
* millisecond precision NTP implementation
* support static IP configuration as well as DHCP
* support factory reset via hardware button
* status led
* eye candy

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
------------
Besides the ESP-Arduino-Core <https://github.com/esp8266/Arduino> Genesys
needs some 3rd party libraries such as arduinoWebSockets
<https://github.com/Links2004/arduinoWebSockets> and esp-mqtt-arduino
<https://github.com/i-n-g-o/esp-mqtt-arduino>. The following script can be
used to set up the build environment and dependencies (change as needed):

```
mkdir -p ~/Devel/ESP8266
cd ~/Devel/ESP8266
git clone git@github.com:esp8266/Arduino.git
cd Arduino/tools
python get.py
cd ../libraries
git clone git@github.com:Links2004/arduinoWebSockets.git
git clone git@github.com:i-n-g-o/esp-mqtt-arduino.git
cd ~/Devel/ESP8266
git clone git@github.com:everslick/genesys.git
cd genesys
```

Now you can create a file called Makefile.config and populate it with the
following content:

```
UPDATE_URL = $(DEFAULT_IP_ADDR)/update

BUILD_SILENTLY        = 1
BUILD_RELEASE         = 0
BUILD_GDB_STUB        = 0
BUILD_LWIP_SRC        = 0
BUILD_SSL_MQTT        = 0

DEFAULT_LOG_CHANNELS  = 3
DEFAULT_LOG_SERVER    = 10.0.0.150
DEFAULT_LOG_PORT      = 49152

DEFAULT_USER_NAME     = genesys
DEFAULT_USER_PASS     = genesys

DEFAULT_WIFI_SSID     = YOUR_WIFI_SSID
DEFAULT_WIFI_PASS     = YOUR_WIFI_PASS

DEFAULT_IP_STATIC     = 1
DEFAULT_IP_ADDR       = 10.0.0.222
DEFAULT_IP_NETMASK    = 255.255.255.0
DEFAULT_IP_GATEWAY    = 10.0.0.138
DEFAULT_IP_DNS1       = 10.0.0.138
DEFAULT_IP_DNS2       = 8.8.8.8

DEFAULT_AP_ENABLED    = 1
DEFAULT_AP_ADDR       = 10.1.1.1

DEFAULT_MDNS_ENABLED  = 0
DEFAULT_MDNS_NAME     = esp

DEFAULT_NTP_ENABLED   = 1
DEFAULT_NTP_SERVER    = pool.ntp.org
DEFAULT_NTP_INTERVAL  = 180

DEFAULT_MQTT_ENABLED  = 1
DEFAULT_MQTT_SERVER   = 10.0.0.150
DEFAULT_MQTT_USER     = GENESYS
DEFAULT_MQTT_PASS     = GENESYS
DEFAULT_MQTT_INTERVAL = 5
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

Setup
-----
After the ESP8266 has booted you can connect to the webinterface. Either via
the soft accesspoint of the ESP or, if you have set up the default configuration
properly, by its IP address or mDNS name. The soft accesspoint starts with a
hardcoded SSID of esp-XXXXXX, XXXXXX being the last 3 bytes of the MAC address.

If no username and password have been set yet, the webserver will redirect to
the initial configuration page. Here you can set the username/password for the
webserver, the WiFi to connect to as well as the WPA2 password, and its network
configuration (DHCP or static IP).

![Initial Setup Page](https://cloud.githubusercontent.com/assets/1909551/15268288/e9aea0dc-19d9-11e6-9d4a-381ba123d03e.jpg)

Pressing [Save] stores the configuration in the EEPROM (flash) and reboots the
device. Connecting again will redirect you to the login page.

![Login Page](https://cloud.githubusercontent.com/assets/1909551/15258780/b8ff3fe6-194d-11e6-8f3d-9ab61ec862d0.jpg)

After successful login you see the example hompage showing the firmware version,
uptime, current time (UTC) and the actual value from the ADC. The page gets
refreshed every two seconds.

![Home Page](https://cloud.githubusercontent.com/assets/1909551/15258778/b8e6b23c-194d-11e6-841f-43ec02a8ce49.jpg)

Configuration
-------------
The configuration page allows setting up of more parameters then was possible in
the initial setup. Here you can also define the mDNS name, MQTT publishing
intervals, or NTP servers.

![Configuration Page](https://cloud.githubusercontent.com/assets/1909551/15258782/b90a8180-194d-11e6-8404-b343963755b6.jpg)

When the firmware was not built as RELEASE version (by setting BUILD_RELEASE = 1
in the Makefile or Makefile.config), there are additional pages accessable that
provide information about the hardware and resource usage. For one there ist the
[Info] page, that shows static information about the firmware and hardware.

![Info Page](https://cloud.githubusercontent.com/assets/1909551/15258781/b9069de0-194d-11e6-8f8b-bfa347236899.jpg)

The [Sys] page gives insights to the resources currently consumed (heap, cpu, net)
and shows the last 20 loglines.

![Sys Page](https://cloud.githubusercontent.com/assets/1909551/15258779/b8f6ccb2-194d-11e6-9cf8-c637fa1e35ec.jpg)

TODO
----
* improved documentation
* configurable soft AP SSID
* disable password fields when wifi is unencrypted
* TLS support for MQTT
* WiFi mesh networking
* support configuration changes via MQTT subscription
* relais output via MQTT subscription

BUGS
----
* build with Arduino IDE fails in load.c complaining about size_t
