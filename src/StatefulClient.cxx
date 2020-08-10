/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "StatefulClient.hxx"
#include "Client.hxx"

#include <assert.h>

void
connection_client_disconnect(StatefulClient *client)
{
    assert(client->client != nullptr);

    if (client->reconnecting) {
        event_del(&client->reconnect_event);
        client->reconnecting = false;
    }

    client->version_requested = false;

    event_del(&client->ping_event);

    uo_client_dispose(client->client);
    client->client = nullptr;
}