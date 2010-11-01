/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __ZMQ_EVENT_HPP_INCLUDED__
#define __ZMQ_EVENT_HPP_INCLUDED__

#include "platform.hpp"
#include "fd.hpp"

namespace zmq
{

    class event_t
    {
    public:

        event_t ();
        ~event_t ();

        fd_t get_fd ();

        //  Signal the event.
        int set ();

        //  Reset the event.
        int reset ();

        //  Wait till event is signaled.
        int wait ();

    private:

#if defined ZMQ_HAVE_EVENTFD
        //  Associated eventfd file decriptor.
        fd_t e;
#else
        static int make_socketpair (fd_t *r_, fd_t *w_);

        //  Write & read end of the socketpair.
        fd_t w;
        fd_t r;
#endif

        //  Disable copying of the object.
        event_t (const event_t&);
        void operator = (const event_t&);
    };

}

#endif
