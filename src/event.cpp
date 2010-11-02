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

#include "event.hpp"
#include "err.hpp"
#include "ip.hpp"

#if defined ZMQ_HAVE_WINDOWS
#include "windows.hpp"
#else
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined ZMQ_HAVE_EVENTFD

#include <sys/eventfd.h>

zmq::event_t::event_t ()
{
    // Create eventfd object.
    e = eventfd (0, 0);
    errno_assert (e != -1);
}

zmq::event_t::~event_t ()
{
    int rc = close (e);
    errno_assert (rc != -1);
}

zmq::fd_t zmq::event_t::get_fd ()
{
    return e;
}

void zmq::event_t::set ()
{
    uint64_t val = 1;
    ssize_t nbytes;
    do {
        nbytes = write (e, &val, sizeof (val));
    } while (nbytes == -1 && errno == EINTR);
    errno_assert (nbytes != -1);
    zmq_assert (nbytes == sizeof (val));
}

void zmq::event_t::reset ()
{
    uint64_t val;
    ssize_t nbytes;
    do {
        nbytes = read (e, &val, sizeof (val));
    } while (nbytes == -1 && errno == EINTR);
    errno_assert (nbytes != -1);
    zmq_assert (nbytes == sizeof (val));
    zmq_assert (val == 1);
}

int zmq::event_t::wait ()
{
    //  FIXME This implementation ignores EINTR for now.
    reset ();
    set ();
    return 0;
}

#else // ZMQ_HAVE_EVENTFD

zmq::event_t::event_t ()
{
    //  Create the socketpair for signalling.
    int rc = make_socketpair (&r, &w);
    errno_assert (rc == 0);

    //  Drop the buffer size to the bare minimum.
    int sndbuf = sizeof (char);
#if defined ZMQ_HAVE_WINDOWS
    rc = setsockopt (w, SOL_SOCKET, SO_SNDBUF, (const char*) &sndbuf,
        sizeof (sndbuf));
    wsa_assert (rc != SOCKET_ERROR);
#else
    rc = setsockopt (w, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof (sndbuf));
    errno_assert (rc == 0);
#endif
}

zmq::event_t::~event_t ()
{
#if defined ZMQ_HAVE_WINDOWS
    int rc = closesocket (w);
    wsa_assert (rc != SOCKET_ERROR);

    rc = closesocket (r);
    wsa_assert (rc != SOCKET_ERROR);
#else
    close (w);
    close (r);
#endif
}

zmq::fd_t zmq::event_t::get_fd ()
{
    return r;
}

void zmq::event_t::set ()
{
    if (signaled.get ())
        return;
    char c = 0;
    int nbytes;
    do {
        nbytes = ::send (w, &c, sizeof (char), 0);
#if defined ZMQ_HAVE_WINDOWS
    } while (nbytes == SOCKET_ERROR && WSAGetLastError() == WSAEINTR);
    wsa_assert (nbytes != SOCKET_ERROR);
#else
    } while (nbytes == -1 && errno == EINTR);
    errno_assert (nbytes != -1);
#endif
    zmq_assert (nbytes == sizeof (char));
    signaled.add (1);
    zmq_assert (signaled.get () == 1);
}

void zmq::event_t::reset ()
{
    if (signaled.get () == 0)
        return;
    char c;
    int nbytes;
    do {
        nbytes = ::recv (r, &c, sizeof (char), 0);
#if defined ZMQ_HAVE_WINDOWS
    } while (nbytes == SOCKET_ERROR && WSAGetLastError() == WSAEINTR);
    wsa_assert (nbytes != SOCKET_ERROR);
#else
    } while (nbytes == -1 && errno == EINTR);
    errno_assert (nbytes != -1);
#endif
    zmq_assert (nbytes == sizeof (char));
    signaled.sub (1);
    zmq_assert (signaled.get () == 0);
}

int zmq::event_t::wait ()
{
    char c;
    int nbytes = ::recv (r, &c, sizeof (char), MSG_PEEK);
#if defined ZMQ_HAVE_WINDOWS
    if (nbytes == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) {
        errno = EINTR;
        return -1;
    }
    wsa_assert (nbytes != SOCKET_ERROR);
#else
    if (nbytes == -1 && errno == EINTR)
        return -1;
    errno_assert (nbytes != -1);
#endif
    zmq_assert (nbytes == sizeof (char));
    return 0;
}

int zmq::event_t::make_socketpair (fd_t *r_, fd_t *w_)
{
#if defined ZMQ_HAVE_WINDOWS

    //  Windows has no 'socketpair' function. CreatePipe is no good as pipe
    //  handles cannot be polled on. Here we create the socketpair by hand.
    *w_ = INVALID_SOCKET;
    *r_ = INVALID_SOCKET;

    //  Create listening socket.
    SOCKET listener;
    listener = socket (AF_INET, SOCK_STREAM, 0);
    wsa_assert (listener != INVALID_SOCKET);

    //  Set SO_REUSEADDR and TCP_NODELAY on listening socket.
    BOOL so_reuseaddr = 1;
    int rc = setsockopt (listener, SOL_SOCKET, SO_REUSEADDR,
        (char *)&so_reuseaddr, sizeof (so_reuseaddr));
    wsa_assert (rc != SOCKET_ERROR);
    BOOL tcp_nodelay = 1;
    rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELAY,
        (char *)&tcp_nodelay, sizeof (tcp_nodelay));
    wsa_assert (rc != SOCKET_ERROR);

    //  Bind listening socket to any free local port.
    struct sockaddr_in addr;
    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    addr.sin_port = 0;
    rc = bind (listener, (const struct sockaddr*) &addr, sizeof (addr));
    wsa_assert (rc != SOCKET_ERROR);

    //  Retrieve local port listener is bound to (into addr).
    int addrlen = sizeof (addr);
    rc = getsockname (listener, (struct sockaddr*) &addr, &addrlen);
    wsa_assert (rc != SOCKET_ERROR);

    //  Listen for incomming connections.
    rc = listen (listener, 1);
    wsa_assert (rc != SOCKET_ERROR);

    //  Create the writer socket.
    *w_ = WSASocket (AF_INET, SOCK_STREAM, 0, NULL, 0,  0);
    wsa_assert (*w_ != INVALID_SOCKET);

    //  Set TCP_NODELAY on writer socket.
    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELAY,
        (char *)&tcp_nodelay, sizeof (tcp_nodelay));
    wsa_assert (rc != SOCKET_ERROR);

    //  Connect writer to the listener.
    rc = connect (*w_, (sockaddr *) &addr, sizeof (addr));
    wsa_assert (rc != SOCKET_ERROR);

    //  Accept connection from writer.
    *r_ = accept (listener, NULL, NULL);
    wsa_assert (*r_ != INVALID_SOCKET);

    //  We don't need the listening socket anymore. Close it.
    rc = closesocket (listener);
    wsa_assert (rc != SOCKET_ERROR);

    return 0;

#elif defined ZMQ_HAVE_OPENVMS

    //  Whilst OpenVMS supports socketpair - it maps to AF_INET only.  Further,
    //  it does not set the socket options TCP_NODELAY and TCP_NODELACK which
    //  can lead to performance problems.
    //
    //  The bug will be fixed in V5.6 ECO4 and beyond.  In the meantime, we'll
    //  create the socket pair manually.
    int listener;
    sockaddr_in lcladdr;
    socklen_t lcladdr_len;
    int rc;
    int on = 1;

    zmq_assert (type_ == SOCK_STREAM);

    //  Fill in the localhost address (127.0.0.1).
    memset (&lcladdr, 0, sizeof (lcladdr));
    lcladdr.sin_family = AF_INET;
    lcladdr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    lcladdr.sin_port = 0;

    listener = socket (AF_INET, SOCK_STREAM, 0);
    errno_assert (listener != -1);

    rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELACK, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = bind(listener, (struct sockaddr*) &lcladdr, sizeof (lcladdr));
    errno_assert (rc != -1);

    lcladdr_len = sizeof (lcladdr);

    rc = getsockname (listener, (struct sockaddr*) &lcladdr, &lcladdr_len);
    errno_assert (rc != -1);

    rc = listen (listener, 1);
    errno_assert (rc != -1);

    *w_ = socket (AF_INET, SOCK_STREAM, 0);
    errno_assert (*w_ != -1);

    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELACK, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = connect (*w_, (struct sockaddr*) &lcladdr, sizeof (lcladdr));
    errno_assert (rc != -1);

    *r_ = accept (listener, NULL, NULL);
    errno_assert (*r_ != -1);

    close (listener);

    return 0;

#else // All other implementations support socketpair()

    int sv [2];
    int rc = socketpair (AF_UNIX, SOCK_STREAM, 0, sv);
    errno_assert (rc == 0);
    *w_ = sv [0];
    *r_ = sv [1];
    return 0;

#endif
}

#endif //  ZMQ_HAVE_EVENTFD

