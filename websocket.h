#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

//enum {
//  WS_EVENT_CONNECTED,
//  WS_EVENT_DISCONNECTED,
//  WS_EVENT_PUBLISH,
//  WS_EVENT_PUBLISHED
//};

//void ws_register_event_cb(void (*)(uint16_t));

bool websocket_init(void);
void websocket_poll(void);

void websocket_disconnect_clients(void);
void websocket_broadcast_message(const char *msg);

#endif // _WEBSOCKET_H_
