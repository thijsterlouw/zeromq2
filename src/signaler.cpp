/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "signaler.hpp"

zmq::signaler_t::signaler_t () :
event (true)
{
}

zmq::signaler_t::~signaler_t ()
{
}

zmq::fd_t zmq::signaler_t::get_fd ()
{
    return event.get_fd ();
}

void zmq::signaler_t::send (const command_t &cmd_)
{
    sync.lock ();
    queue.write (cmd_, false);
    if (!queue.flush ())
        event.set ();
    sync.unlock ();
}

int zmq::signaler_t::recv (command_t *cmd_, bool block_)
{
    if (!queue.read (cmd_)) {
        if (!block_) {
            errno = EAGAIN;
            return -1;
        }
        int rc = event.wait ();
        if (rc == -1 && errno == EINTR)
            return -1;
        errno_assert (rc == 0);
        zmq_assert (queue.read (cmd_));
    }
    if (!queue.check_read ())
        event.reset ();
    return 0;
}

