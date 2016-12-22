/*
    This file is part of Genesys.

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

    Copyright (C) 2016 Clemens Kirchgatterer <clemens@1541.org>.
*/

/*
  MQTT.cpp - A simple client for MQTT.

  Nick O'Leary
  http://knolleary.net
  Clemens Kirchgatterer
  http://www.1541.org
*/

#include "mqtt.h"

using namespace std;

MQTT::MQTT(const char *host, uint16_t port, const String &fingerprint) {
  state = MQTT_DISCONNECTED;

  this->host = host;
  this->port = port;

  client   = NULL;
  callback = NULL;

  this->fingerprint = fingerprint;
}

MQTT::~MQTT(void) {
  delete (client);
}

bool MQTT::connect(const char *id, const char *key) {
  return (connect(id, key, "", 0, 0, 0, 0));
}

bool MQTT::connect(const char *id, const char *user, const char *pass) {
  return (connect(id, user, pass, 0, 0, 0, 0));
}

bool MQTT::connect(const char *id, const char *user, const char *pass, const char *willTopic, uint8_t willQos, bool willRetain, const char *willMessage) {
  if (!client) {
#ifdef TELEMETRY_TLS_SUPPORT
    client = new WiFiClientSecure();
#else
    client = new WiFiClient();
#endif
  }

  if (!connected()) {
    int result = 0;

    if (!host) return (false);

    result = client->connect(host, port);

    if (result != 1) {
      state = MQTT_CONNECT_FAILED;

      return (false);
    }

#ifdef TELEMETRY_TLS_SUPPORT
    if (!client->verify(fingerprint.c_str(), host)) {
      state = MQTT_WRONG_FINGERPRINT;
      client->stop();

      return (false);
    }
#endif

    nextMsgId = 1;
    // Leave room in the buffer for header and variable length field
    uint16_t length = 5;
    unsigned int j;

#if MQTT_VERSION == MQTT_VERSION_3_1
    uint8_t d[9] = { 0x00,0x06,'M','Q','I','s','d','p', MQTT_VERSION };
#define MQTT_HEADER_VERSION_LENGTH 9
#elif MQTT_VERSION == MQTT_VERSION_3_1_1
    uint8_t d[7] = { 0x00,0x04,'M','Q','T','T', MQTT_VERSION };
#define MQTT_HEADER_VERSION_LENGTH 7
#endif
    for (j = 0; j<MQTT_HEADER_VERSION_LENGTH; j++) {
      buffer[length++] = d[j];
    }

    uint8_t v;
    if (willTopic) {
      v = 0x06 | (willQos<<3) | (willRetain<<5);
    } else {
      v = 0x02;
    }

    if (user != NULL) {
      v = v | 0x80;

      if(pass != NULL) {
        v = v | (0x80>>1);
      }
    }

    buffer[length++] = v;
    buffer[length++] = ((MQTT_KEEPALIVE) >> 8);
    buffer[length++] = ((MQTT_KEEPALIVE) & 0xFF);

    length = writeString(id, buffer, length);

    if (willTopic) {
      length = writeString(willTopic, buffer, length);
      length = writeString(willMessage, buffer, length);
    }

    if (user != NULL) {
      length = writeString(user, buffer, length);
      if(pass != NULL) {
        length = writeString(pass, buffer, length);
      }
    }

    write(MQTTCONNECT, buffer, length-5);

    lastInActivity = lastOutActivity = millis();

    while (!client->available()) {
      unsigned long t = millis();
      if (t-lastInActivity >= ((int32_t) MQTT_SOCKET_TIMEOUT*1000UL)) {
        state = MQTT_CONNECTION_TIMEOUT;
        client->stop();

        return (false);
      }
    }

    uint8_t llen;
    uint16_t len = readPacket(&llen);

    if (len == 4) {
      if (buffer[3] == 0) {
        lastInActivity = millis();
        pingOutstanding = false;
        state = MQTT_CONNECTED;

        return (true);
      } else {
        state = buffer[3];
      }
    }
    client->stop();

    return (false);
  }

  return (true);
}

// reads a byte into result
bool MQTT::readByte(uint8_t *result) {
  uint32_t previousMillis = millis();
  while (!client->available()) {
    uint32_t currentMillis = millis();

    if (currentMillis - previousMillis >= ((int32_t) MQTT_SOCKET_TIMEOUT * 1000)) {
      return (false);
    }
  }
  *result = client->read();

  return true;
}

// reads a byte into result[*index] and increments index
bool MQTT::readByte(uint8_t *result, uint16_t *index){
  uint16_t current_index = *index;
  uint8_t *write_address = &(result[current_index]);
  if (readByte(write_address)) {
    *index = current_index + 1;

    return (true);
  }

  return (false);
}

uint16_t MQTT::readPacket(uint8_t *lengthLength) {
  uint16_t len = 0;

  if (!readByte(buffer, &len)) return (0);

  bool isPublish = (buffer[0] & 0xF0) == MQTTPUBLISH;
  uint32_t multiplier = 1;
  uint16_t length = 0;
  uint8_t digit = 0;
  uint16_t skip = 0;
  uint8_t start = 0;

  do {
    if (!readByte(&digit)) return (0);

    buffer[len++] = digit;
    length += (digit & 127) * multiplier;
    multiplier *= 128;
  } while ((digit & 128) != 0);

  *lengthLength = len-1;

  if (isPublish) {
    // Read in topic length to calculate bytes to skip over for Stream writing
    if (!readByte(buffer, &len)) return (0);
    if (!readByte(buffer, &len)) return (0);

    skip = (buffer[*lengthLength + 1]<<8) + buffer[*lengthLength + 2];
    start = 2;
    if (buffer[0]&MQTTQOS1) {
      // skip message id
      skip += 2;
    }
  }

  for (uint16_t i = start;i<length;i++) {
    if (!readByte(&digit)) return (0);

    if (len < MQTT_MAX_PACKET_SIZE) {
      buffer[len] = digit;
    }

    len++;
  }

  if (len > MQTT_MAX_PACKET_SIZE) {
    len = 0; // This will cause the packet to be ignored.
  }

  return (len);
}

bool MQTT::loop() {
  if (connected()) {
    unsigned long t = millis();

    if ((t - lastInActivity > MQTT_KEEPALIVE * 1000UL) || (t - lastOutActivity > MQTT_KEEPALIVE * 1000UL)) {
      if (pingOutstanding) {
        state = MQTT_CONNECTION_TIMEOUT;
        client->stop();

        return (false);
      } else {
        buffer[0] = MQTTPINGREQ;
        buffer[1] = 0;

        client->write(buffer,2);

        lastOutActivity = t;
        lastInActivity = t;

        pingOutstanding = true;
      }
    }

    if (client->available()) {
      uint8_t llen;
      uint16_t len = readPacket(&llen);
      uint16_t msgId = 0;
      uint8_t *payload;

      if (len > 0) {
        lastInActivity = t;
        uint8_t type = buffer[0]&0xF0;

        if (type == MQTTPUBLISH) {
          if (callback) {
            uint16_t tl = (buffer[llen+1]<<8)+buffer[llen+2];
            char topic[tl+1];

            for (uint16_t i=0; i<tl; i++) {
              topic[i] = buffer[llen+3+i];
            }
            topic[tl] = 0;

            // msgId only present for QOS>0
            if ((buffer[0]&0x06) == MQTTQOS1) {
              msgId = (buffer[llen+3+tl]<<8)+buffer[llen+3+tl+1];
              payload = buffer+llen+3+tl+2;

              callback(topic,payload, len-llen-3-tl-2);

              buffer[0] = MQTTPUBACK;
              buffer[1] = 2;
              buffer[2] = (msgId >> 8);
              buffer[3] = (msgId & 0xFF);

              client->write(buffer, 4);

              lastOutActivity = t;
            } else {
              payload = buffer+llen+3+tl;
              callback(topic, payload, len-llen-3-tl);
            }
          }
        } else if (type == MQTTPINGREQ) {
          buffer[0] = MQTTPINGRESP;
          buffer[1] = 0;

          client->write(buffer,2);
        } else if (type == MQTTPINGRESP) {
          pingOutstanding = false;
        }
      }
    }

    return (true);
  }

  return (false);
}

bool MQTT::publish(const char *topic, const char *payload) {
  return publish(topic, (const uint8_t *)payload, strlen(payload), false);
}

bool MQTT::publish(const char *topic, const char *payload, bool retained) {
  return publish(topic, (const uint8_t *)payload, strlen(payload), retained);
}

bool MQTT::publish(const char *topic, const uint8_t *payload, unsigned int plength) {
  return publish(topic, payload, plength, false);
}

bool MQTT::publish(const char *topic, const uint8_t *payload, unsigned int plength, bool retained) {
  if (connected()) {
    if (MQTT_MAX_PACKET_SIZE < 5+2+strlen(topic) + plength) {
      // Too long
      return (false);
    }

    // Leave room in the buffer for header and variable length field
    uint16_t length = 5;
    length = writeString(topic, buffer, length);

    uint16_t i;
    for (i=0; i<plength; i++) {
      buffer[length++] = payload[i];
    }

    uint8_t header = MQTTPUBLISH;
    if (retained) {
      header |= 1;
    }

    return (write(header, buffer, length-5));
  }

  return (false);
}

bool MQTT::write(uint8_t header, uint8_t *buf, uint16_t length) {
  uint8_t lenBuf[4];
  uint8_t llen = 0;
  uint8_t digit;
  uint8_t pos = 0;
  uint16_t rc;
  uint16_t len = length;

  do {
    digit = len % 128;
    len = len / 128;
    if (len > 0) {
      digit |= 0x80;
    }
    lenBuf[pos++] = digit;
    llen++;
  } while (len>0);

  buf[4-llen] = header;
  for (int i=0; i<llen; i++) {
    buf[5-llen+i] = lenBuf[i];
  }

  rc = client->write(buf+(4-llen), length+1+llen);
  lastOutActivity = millis();

  return (rc == 1+llen+length);
}

bool MQTT::subscribe(const String &topic, uint8_t qos) {
  if (qos < 0 || qos > 1) {
    return (false);
  }

  if (MQTT_MAX_PACKET_SIZE < 9 + topic.length()) {
    // Too long
    return (false);
  }

  if (connected()) {
    // Leave room in the buffer for header and variable length field
    uint16_t length = 5;

    nextMsgId++;
    if (nextMsgId == 0) {
      nextMsgId = 1;
    }

    buffer[length++] = (nextMsgId >> 8);
    buffer[length++] = (nextMsgId & 0xFF);
    length = writeString(topic.c_str(), buffer, length);
    buffer[length++] = qos;

    return (write(MQTTSUBSCRIBE|MQTTQOS1, buffer, length-5));
  }

  return (false);
}

bool MQTT::unsubscribe(const String &topic) {
  if (MQTT_MAX_PACKET_SIZE < 9 + topic.length()) {
    // Too long
    return (false);
  }

  if (connected()) {
    uint16_t length = 5;

    nextMsgId++;
    if (nextMsgId == 0) {
      nextMsgId = 1;
    }

    buffer[length++] = (nextMsgId >> 8);
    buffer[length++] = (nextMsgId & 0xFF);
    length = writeString(topic.c_str(), buffer, length);

    return write(MQTTUNSUBSCRIBE|MQTTQOS1, buffer, length-5);
  }

  return (false);
}

void MQTT::disconnect() {
  buffer[0] = MQTTDISCONNECT;
  buffer[1] = 0;

  client->write(buffer,2);
  state = MQTT_DISCONNECTED;
  client->stop();

  lastInActivity = lastOutActivity = millis();
}

uint16_t MQTT::writeString(const char *string, uint8_t *buf, uint16_t pos) {
  const char *idp = string;
  uint16_t i = 0;

  pos += 2;
  while (*idp) {
    buf[pos++] = *idp++;
    i++;
  }
  buf[pos-i-2] = (i >> 8);
  buf[pos-i-1] = (i & 0xFF);

  return (pos);
}


bool MQTT::connected() {
  bool ret = false;

  if (client) {
    ret = (int)client->connected();

    if (!ret) {
      if (state == MQTT_CONNECTED) {
        state = MQTT_CONNECTION_LOST;

        client->flush();
        client->stop();
      }
    }
  }

  return (ret);
}

void MQTT::ReceiveCallback(function<void(char *, uint8_t *, unsigned int)> cb) {
  callback = cb;
}
