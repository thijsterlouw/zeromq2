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

#ifndef __ZMQ_SIGNALER_HPP_INCLUDED__
#define __ZMQ_SIGNALER_HPP_INCLUDED__

#include <deque>

#include "event.hpp"
#include "command.hpp"
#include "config.hpp"
#include "mutex.hpp"

namespace zmq
{

    class signaler_t
    {
    public:

        signaler_t ();
        ~signaler_t ();

        fd_t get_fd ();
        void send (const command_t &cmd_);
        int recv (command_t *cmd_, bool block_);
        
    private:

        //  Event used to signal availability of commands on the reader side.
        event_t event;

        //  If true, the underlying event is signaled.
        bool signaled;

        //  The queue of commands.
        typedef std::deque <command_t> queue_t;
        queue_t queue;

        //  Synchronisation of access to the queue.
        mutex_t sync;

        //  Disable copying of fd_signeler object.
        signaler_t (const signaler_t&);
        void operator = (const signaler_t&);
    };

}

#endif
