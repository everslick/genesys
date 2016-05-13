#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_

enum {
  MQTT_EVENT_CONNECTED,
  MQTT_EVENT_DISCONNECTED,
  MQTT_EVENT_PUBLISH,
  MQTT_EVENT_PUBLISHED
};

void mqtt_register_event_cb(void (*)(uint16_t));

bool mqtt_init(void);
bool mqtt_poll(void);

#endif // _TRANSPORT_H_
