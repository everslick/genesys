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
