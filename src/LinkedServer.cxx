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

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Handler.hxx"
#include "Log.hxx"

#include <cassert>

LinkedServer::~LinkedServer() noexcept
{
    evtimer_del(&zombie_timeout);

    if (server != nullptr)
        uo_server_dispose(server);
}

void
LinkedServer::ZombieTimeoutCallback(int, short, void *ctx) noexcept
{
    auto &ls = *(LinkedServer *)ctx;

    auto &c = *ls.connection;
    c.RemoveCheckEmpty(ls);
}

inline void
LinkedServer::Zombify() noexcept
{
    is_zombie = true;

    static constexpr struct timeval tv{5, 0};
    evtimer_add(&zombie_timeout, &tv);
}

bool
LinkedServer::OnServerPacket(const void *data, size_t length)
{
    Connection *c = connection;

    assert(c != nullptr);
    assert(!is_zombie);

    const auto action = handle_packet_from_client(client_packet_bindings,
                                                  *this, data, length);
    switch (action) {
    case PacketAction::ACCEPT:
        if (c->client.client != nullptr &&
            (!c->client.reconnecting ||
             *(const unsigned char*)data == PCK_ClientVersion))
            uo_client_send(c->client.client, data, length);
        break;

    case PacketAction::DROP:
        break;

    case PacketAction::DISCONNECT:
        LogFormat(2, "aborting connection to client after packet 0x%x\n",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        c->Remove(*this);
        delete this;

        if (c->servers.empty()) {
            if (c->background)
                LogFormat(1, "backgrounding\n");
            else
                c->Destroy();
        }
        return false;

    case PacketAction::DELETED:
        return false;
    }

    return true;
}

void
LinkedServer::OnServerDisconnect() noexcept
{
    assert(server != nullptr);
    uo_server_dispose(std::exchange(server, nullptr));

    if (expecting_reconnect) {
        LogFormat(2, "client disconnected, zombifying server connection for 5 seconds\n");
        Zombify();
        return;
    }

    Connection *c = connection;
    assert(c != nullptr);
    c->RemoveCheckEmpty(*this);

    delete this;
}
