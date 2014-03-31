/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: trevent.c 13868 2013-01-25 23:34:20Z jordan $
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <signal.h>

#include <event2/dns.h>
#include <event2/event.h>

#include "transmission.h"
#include "log.h"
#include "net.h"
#include "session.h"

#ifdef WIN32

#include "utils.h"
#include <winsock2.h>

static int
pgpipe (int handles[2])
{
    SOCKET s;
    struct sockaddr_in serv_addr;
    int len = sizeof (serv_addr);

    handles[0] = handles[1] = INVALID_SOCKET;

    if ((s = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        tr_logAddDebug ("pgpipe failed to create socket: %ui", WSAGetLastError ());
        return -1;
    }

    memset (&serv_addr, 0, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons (0);
    serv_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    if (bind (s, (SOCKADDR *) & serv_addr, len) == SOCKET_ERROR)
    {
        tr_logAddDebug ("pgpipe failed to bind: %ui", WSAGetLastError ());
        closesocket (s);
        return -1;
    }
    if (listen (s, 1) == SOCKET_ERROR)
    {
        tr_logAddNamedDbg ("event","pgpipe failed to listen: %ui", WSAGetLastError ());
        closesocket (s);
        return -1;
    }
    if (getsockname (s, (SOCKADDR *) & serv_addr, &len) == SOCKET_ERROR)
    {
        tr_logAddDebug ("pgpipe failed to getsockname: %ui", WSAGetLastError ());
        closesocket (s);
        return -1;
    }
    if ((handles[1] = socket (PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        tr_logAddDebug ("pgpipe failed to create socket 2: %ui", WSAGetLastError ());
        closesocket (s);
        return -1;
    }

    if (connect (handles[1], (SOCKADDR *) & serv_addr, len) == SOCKET_ERROR)
    {
        tr_logAddDebug ("pgpipe failed to connect socket: %ui", WSAGetLastError ());
        closesocket (s);
        return -1;
    }
    if ((handles[0] = accept (s, (SOCKADDR *) & serv_addr, &len)) == INVALID_SOCKET)
    {
        tr_logAddDebug ("pgpipe failed to accept socket: %ui", WSAGetLastError ());
        closesocket (handles[1]);
        handles[1] = INVALID_SOCKET;
        closesocket (s);
        return -1;
    }
    closesocket (s);
    return 0;
}

static int
piperead (int s, char *buf, int len)
{
    int ret = recv (s, buf, len, 0);

    if (ret < 0) {
        const int werror= WSAGetLastError ();
        switch (werror) {
          /* simplified error mapping (not valid for connect) */
            case WSAEWOULDBLOCK:
                errno = EAGAIN;
                break;
	    case WSAECONNRESET:
	        /* EOF on the pipe! (win32 socket based implementation) */
	        ret = 0;
	        /* fall through */
            default:
                errno = werror;
                break;
        }
    } else
        errno = 0;
    return ret;
}

#define pipe(a) pgpipe (a)
#define pipewrite(a,b,c) send (a, (char*)b,c,0)

#else
#define piperead(a,b,c) read (a,b,c)
#define pipewrite(a,b,c) write (a,b,c)
#endif

#include <unistd.h> /* read (), write (), pipe () */

#include "transmission.h"
#include "platform.h" /* tr_lockLock () */
#include "trevent.h"
#include "utils.h"

/***
****
***/

typedef struct tr_event_handle
{
    uint8_t      die;
    int          fds[2];
    tr_lock *    lock;
    tr_session *  session;
    tr_thread *  thread;
    struct event_base * base;
    struct event * pipeEvent;
}
tr_event_handle;

struct tr_run_data
{
    void  (*func)(void *);
    void *  user_data;
};

#define dbgmsg(...) \
    do { \
        if (tr_logGetDeepEnabled ()) \
            tr_logAddDeep (__FILE__, __LINE__, "event", __VA_ARGS__); \
    } while (0)

static void
readFromPipe (int    fd,
              short  eventType,
              void * veh)
{
    char              ch;
    int               ret;
    tr_event_handle * eh = veh;

    dbgmsg ("readFromPipe: eventType is %hd", eventType);

    /* read the command type */
    ch = '\0';
    do
    {
        ret = piperead (fd, &ch, 1);
    }
    while (!eh->die && ret < 0 && errno == EAGAIN);

    dbgmsg ("command is [%c], ret is %d, errno is %d", ch, ret, (int)errno);

    switch (ch)
    {
        case 'r': /* run in libevent thread */
        {
            struct tr_run_data data;
            const size_t       nwant = sizeof (data);
            const ssize_t      ngot = piperead (fd, &data, nwant);
            if (!eh->die && (ngot == (ssize_t)nwant))
            {
                dbgmsg ("invoking function in libevent thread");
              (data.func)(data.user_data);
            }
            break;
        }

        case '\0': /* eof */
        {
            dbgmsg ("pipe eof reached... removing event listener");
            event_free (eh->pipeEvent);
            break;
        }

        default:
        {
            assert (0 && "unhandled command type!");
            break;
        }
    }
}

static void
logFunc (int severity, const char * message)
{
    if (severity >= _EVENT_LOG_ERR)
        tr_logAddError ("%s", message);
    else
        tr_logAddDebug ("%s", message);
}

static void
libeventThreadFunc (void * veh)
{
    struct event_base * base;
    tr_event_handle * eh = veh;

#ifndef WIN32
    /* Don't exit when writing on a broken socket */
    signal (SIGPIPE, SIG_IGN);
#endif

    /* create the libevent bases */
    base = event_base_new ();

    /* set the struct's fields */
    eh->base = base;
    eh->session->event_base = base;
    eh->session->evdns_base = evdns_base_new (base, true);
    eh->session->events = eh;

    /* listen to the pipe's read fd */
    eh->pipeEvent = event_new (base, eh->fds[0], EV_READ | EV_PERSIST, readFromPipe, veh);
    event_add (eh->pipeEvent, NULL);
    event_set_log_callback (logFunc);

    /* loop until all the events are done */
    while (!eh->die)
        event_base_dispatch (base);

    /* shut down the thread */
    tr_lockFree (eh->lock);
    event_base_free (base);
    eh->session->events = NULL;
    tr_free (eh);
    tr_logAddDebug ("Closing libevent thread");
}

void
tr_eventInit (tr_session * session)
{
    tr_event_handle * eh;

    session->events = NULL;

    eh = tr_new0 (tr_event_handle, 1);
    eh->lock = tr_lockNew ();
    pipe (eh->fds);
    eh->session = session;
    eh->thread = tr_threadNew (libeventThreadFunc, eh);

    /* wait until the libevent thread is running */
    while (session->events == NULL)
        tr_wait_msec (100);
}

void
tr_eventClose (tr_session * session)
{
    assert (tr_isSession (session));

    session->events->die = true;
    tr_logAddDeep (__FILE__, __LINE__, NULL, "closing trevent pipe");
    tr_netCloseSocket (session->events->fds[1]);
}

/**
***
**/

bool
tr_amInEventThread (const tr_session * session)
{
    assert (tr_isSession (session));
    assert (session->events != NULL);

    return tr_amInThread (session->events->thread);
}

/**
***
**/

void
tr_runInEventThread (tr_session * session,
                     void func (void*), void * user_data)
{
    assert (tr_isSession (session));
    assert (session->events != NULL);

    if (tr_amInThread (session->events->thread))
    {
      (func)(user_data);
    }
    else
    {
        const char         ch = 'r';
        int                fd = session->events->fds[1];
        tr_lock *          lock = session->events->lock;
        struct tr_run_data data;

        tr_lockLock (lock);
        pipewrite (fd, &ch, 1);
        data.func = func;
        data.user_data = user_data;
        pipewrite (fd, &data, sizeof (data));
        tr_lockUnlock (lock);
    }
}
