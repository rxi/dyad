/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef _WIN32
  #define _WIN32_WINNT 0x501
  #define _CRT_SECURE_NO_WARNINGS
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
#else
  #define _POSIX_C_SOURCE 200809L
  #include <unistd.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include "dyad.h"

#define DYAD_VERSION "0.0.0"


#ifdef _WIN32
  #define close(a) closesocket(a)
  #define getsockopt(a,b,c,d,e) getsockopt((a),(b),(c),(char*)(d),(e))
  #define setsockopt(a,b,c,d,e) setsockopt((a),(b),(c),(char*)(d),(e))

  #undef  errno
  #define errno WSAGetLastError()

  #undef  EWOULDBLOCK
  #define EWOULDBLOCK WSAEWOULDBLOCK

  const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    union { struct sockaddr sa; struct sockaddr_in sai;
            struct sockaddr_in6 sai6; } addr;
    int res;
    memset(&addr, 0, sizeof(addr));
    addr.sa.sa_family = af;
    if (af == AF_INET6) {
      memcpy(&addr.sai6.sin6_addr, src, sizeof(addr.sai6.sin6_addr));
    } else {
      memcpy(&addr.sai.sin_addr, src, sizeof(addr.sai.sin_addr));
    }
    res = WSAAddressToString(&addr.sa, sizeof(addr), 0, dst, (LPDWORD) &size);
    if (res != 0) return NULL;
    return dst;
  }
#endif


/*===========================================================================*/
/* Memory                                                                    */
/*===========================================================================*/

static void dyad_panic(const char *fmt, ...);

static void *dyad_realloc(void *ptr, int n) {
  ptr = realloc(ptr, n);
  if (!ptr) {
    dyad_panic("out of memory");
  }
  return ptr;
}


static void dyad_free(void *ptr) {
  free(ptr);
}


/*===========================================================================*/
/* Vector                                                                    */
/*===========================================================================*/

static void dyad_vectorExpand(
  char **data, int *length, int *capacity, int memsz
) {
  if (*length + 1 > *capacity) {
    if (*capacity == 0) {
      *capacity = 1;
    } else {
      *capacity <<= 1;
    }
    *data = dyad_realloc(*data, *capacity * memsz);
  }
}

static void dyad_vectorSplice(
  char **data, int *length, int *capacity, int memsz, int start, int count
) {
  (void) capacity;
  memmove(*data + start * memsz,
          *data + (start + count) * memsz,
          (*length - start - count) * memsz);
}


#define dyad_Vector(T)\
  struct { T *data; int length, capacity; }


#define dyad_vectorUnpack(v)\
  (char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)


#define dyad_vectorInit(v)\
  memset((v), 0, sizeof(*(v)))


#define dyad_vectorDeinit(v)\
  dyad_free((v)->data)


#define dyad_vectorClear(v)\
  ((v)->length = 0)


#define dyad_vectorPush(v, val)\
  ( dyad_vectorExpand(dyad_vectorUnpack(v)),\
    (v)->data[(v)->length++] = (val) )


#define dyad_vectorSplice(v, start, count)\
  ( dyad_vectorSplice(dyad_vectorUnpack(v), start, count),\
    (v)->length -= (count) )


/*===========================================================================*/
/* FdSet                                                                     */
/*===========================================================================*/

/* A wrapper around fd_set for use with select(). The FdSet's underlying fd_set
 * memory is automatically expanded to accommodate fds as they are added.
 *
 * On Windows fd_sets are implemented as arrays; the FD_xxx macros are not used
 * by the wrapper and instead the fd_set struct is manipulated directly. The
 * wrapper should perform better than the normal FD_xxx macros, given that we
 * don't bother with the linear search which FD_SET would perform to check for
 * duplicates.
 *
 * On non-Windows platforms the sets are assumed to be bit arrays. The FD_xxx
 * macros are not used in case their implementation attempts to do bounds
 * checking; instead we manipulate the fd_set's bits directly.
 */

#define DYAD_UNSIGNED_BIT (sizeof(unsigned) * CHAR_BIT)

typedef struct {
  dyad_Vector(fd_set) vec;
} dyad_FdSet;


static void dyad_fdsDeinit(dyad_FdSet *fds) {
  dyad_vectorDeinit(&fds->vec);
}


static fd_set *dyad_fdsGetFdSet(dyad_FdSet *fds) {
  return fds->vec.data;
}


static void dyad_fdsGrow(dyad_FdSet *fds) {
  fd_set s;
  memset(&s, 0, sizeof(s));
  dyad_vectorPush(&fds->vec, s);
}


static void dyad_fdsZero(dyad_FdSet *fds) {
#if _WIN32
  fd_set *s = fds->vec.data;
  if (s) s->fd_count = 0;
#else
  memset(fds->vec.data, 0, fds->vec.length * sizeof(*fds->vec.data)); 
#endif
}


static void dyad_fdsSet(dyad_FdSet *fds, int fd) {
#ifdef _WIN32
  fd_set *s;
  unsigned max = (unsigned) fds->vec.length * FD_SETSIZE;
  if (!fds->vec.data || max < fds->vec.data->fd_count + 1) {
    dyad_fdsGrow(fds);
  }
  s = fds->vec.data;
  s->fd_array[s->fd_count++] = (SOCKET) fd;
#else
  unsigned *p;
  while (fds->vec.length * FD_SETSIZE <= fd) {
    dyad_fdsGrow(fds);
  }
  p = (unsigned*) fds->vec.data;
  p[fd / DYAD_UNSIGNED_BIT] |= 1 << (fd % DYAD_UNSIGNED_BIT);
#endif
}


static int dyad_fdsIsSet(dyad_FdSet *fds, int fd) {
#ifdef _WIN32
  fd_set *s = fds->vec.data;
  unsigned i;
  if (!s) return 0;
  for (i = 0; i < s->fd_count; i++) {
    if (s->fd_array[i] == (SOCKET) fd) {
      return 1;
    }
  }
  return 0;
#else
  unsigned *p;
  if (fds->vec.length * FD_SETSIZE <= fd) return 0;
  p = (unsigned*) fds->vec.data;
  return p[fd / DYAD_UNSIGNED_BIT] & (1 << (fd % DYAD_UNSIGNED_BIT));
#endif
}


/*===========================================================================*/
/* Core                                                                      */
/*===========================================================================*/

typedef struct {
  int event;
  dyad_Callback callback;
  void *udata;
} dyad_Listener;


struct dyad_Stream {
  int state, flags;
  int sockfd;
  char *address;
  int port;
  int bytesSent, bytesReceived;
  double lastActivity, timeout;
  dyad_Vector(dyad_Listener) listeners;
  dyad_Vector(char) lineBuffer;
  dyad_Vector(char) writeBuffer;
  dyad_Stream *next;
};

#define DYAD_FLAG_READY   (1 << 0)
#define DYAD_FLAG_WRITTEN (1 << 1)


static dyad_Stream *dyad_streams;
static int dyad_streamCount;
static char dyad_panicMsgBuffer[128];
static dyad_PanicCallback dyad_panicCallback;
static dyad_FdSet dyad_readSet;
static dyad_FdSet dyad_writeSet;
static dyad_FdSet dyad_exceptSet;
static double dyad_updateTimeout = 1;
static double dyad_tickInterval = 1;
static double dyad_lastTick = 0;


static void dyad_panic(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsprintf(dyad_panicMsgBuffer, fmt, args);
  va_end(args);
  if (dyad_panicCallback) {
    dyad_panicCallback(dyad_panicMsgBuffer);
  } else {
    printf("dyad panic: %s\n", dyad_panicMsgBuffer);
  }
  exit(EXIT_FAILURE);
}


static dyad_Event dyad_createEvent(int type) {
  dyad_Event e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  return e;
}


static const char *dyad_intToStr(int x) {
  static char buf[64];
  sprintf(buf, "%d", x);
  return buf;
}


static void dyad_destroyStream(dyad_Stream *stream);

static void dyad_destroyClosedStreams(void) {
  dyad_Stream *stream = dyad_streams;
  while (stream) {
    if (stream->state == DYAD_STATE_CLOSED) {
      dyad_Stream *next = stream->next;
      dyad_destroyStream(stream);
      stream = next;
    } else {
      stream = stream->next;
    }
  }
}


static void dyad_emitEvent(dyad_Stream *stream, dyad_Event *e);

static void dyad_updateTickTimer(void) {
  /* Update tick timer */
  if (dyad_lastTick == 0) {
    dyad_lastTick = dyad_getTime();
  }
  while (dyad_lastTick < dyad_getTime()) {
    /* Emit event on all streams */
    dyad_Stream *stream;
    dyad_Event e = dyad_createEvent(DYAD_EVENT_TICK);
    e.msg = "a tick has occured";
    stream = dyad_streams;
    while (stream) {
      dyad_emitEvent(stream, &e);
      stream = stream->next;
    }
    dyad_lastTick += dyad_tickInterval;
  }
}


static void dyad_updateStreamTimeouts(void) {
  double currentTime = dyad_getTime();
  dyad_Stream *stream;
  dyad_Event e = dyad_createEvent(DYAD_EVENT_TIMEOUT);
  e.msg = "stream timed out";
  stream = dyad_streams;
  while (stream) {
    if (stream->timeout) {
      if (currentTime - stream->lastActivity > stream->timeout) {
        dyad_emitEvent(stream, &e);
        dyad_close(stream);
      }
    }
    stream = stream->next;
  }
}



/*===========================================================================*/
/* Stream                                                                    */
/*===========================================================================*/

static void dyad_destroyStream(dyad_Stream *stream) {
  dyad_Stream **next;
  /* Close socket */
  if (stream->sockfd != -1) {
    close(stream->sockfd);
  }
  /* Remove from list and decrement count */
  next = &dyad_streams;
  while (*next != stream) {
    next = &(*next)->next;
  }
  *next = stream->next;
  dyad_streamCount--;
  /* Destroy and free */
  dyad_vectorDeinit(&stream->listeners);
  dyad_vectorDeinit(&stream->lineBuffer);
  dyad_vectorDeinit(&stream->writeBuffer);
  dyad_free(stream->address);
  dyad_free(stream);
}


static void dyad_emitEvent(dyad_Stream *stream, dyad_Event *e) {
  int i;
  e->stream = stream;
  for (i = 0; i < stream->listeners.length; i++) {
    dyad_Listener *listener = &stream->listeners.data[i];
    if (listener->event == e->type) {
      e->udata = listener->udata;
      listener->callback(e);
    }
    /* Check to see if this listener was removed: If it was we decrement `i`
     * since the next listener will now be in this ones place */
    if (listener != &stream->listeners.data[i]) {
      i--;
    }
  }
}


static void dyad_streamError(dyad_Stream *stream, const char *msg) {
  dyad_Event e = dyad_createEvent(DYAD_EVENT_ERROR);
  e.msg = msg;
  dyad_emitEvent(stream, &e);
  dyad_close(stream);
}


static void dyad_initAddress(dyad_Stream *stream) {
  union { struct sockaddr sa; struct sockaddr_storage sas;
          struct sockaddr_in sai; struct sockaddr_in6 sai6; } addr;
  socklen_t size;
  memset(&addr, 0, sizeof(addr));
  size = sizeof(addr);
  dyad_free(stream->address);
  if (getpeername(stream->sockfd, &addr.sa, &size) == -1) {
    return;
  }
  if (addr.sas.ss_family == AF_INET6) {
    stream->address = dyad_realloc(NULL, 46);
    inet_ntop(AF_INET6, &addr.sai6.sin6_addr, stream->address, 45);
    stream->port = ntohs(addr.sai6.sin6_port);
  } else {
    stream->address = dyad_realloc(NULL, 16);
    inet_ntop(AF_INET, &addr.sai.sin_addr, stream->address, 15);
    stream->port = ntohs(addr.sai.sin_port);
  }
}


static void dyad_setSocketNonBlocking(dyad_Stream *stream, int opt) {
#ifdef _WIN32
  u_long mode = opt;
  ioctlsocket(stream->sockfd, FIONBIO, &mode);
#else
  fcntl(stream->sockfd, F_SETFL, opt ? O_NONBLOCK : ~O_NONBLOCK);
#endif
}


static void dyad_setSocket(dyad_Stream *stream, int sockfd) {
  stream->sockfd = sockfd;
  dyad_setSocketNonBlocking(stream, 1);
  dyad_initAddress(stream);
}


static int dyad_initSocket(
  dyad_Stream *stream, int domain, int type, int protocol
) {
  stream->sockfd = socket(domain, type, protocol);
  if (stream->sockfd == -1) {
    dyad_streamError(stream, "could not create socket"); 
    return -1;
  }
  dyad_setSocket(stream, stream->sockfd);
  return 0;
}


static int dyad_hasListenerForEvent(dyad_Stream *stream, int event) {
  int i;
  for (i = 0; i < stream->listeners.length; i++) {
    dyad_Listener *listener = &stream->listeners.data[i];
    if (listener->event == event) {
      return 1;
    }
  }
  return 0;
}


static void dyad_handleReceivedData(dyad_Stream *stream) {
  for (;;) {
    /* Receive data */
    dyad_Event e;
    char data[8192];
    int size = recv(stream->sockfd, data, sizeof(data) - 1, 0);
    if (size <= 0) {
      if (size == 0 || errno != EWOULDBLOCK) {
        /* Handle disconnect */
        dyad_close(stream);
        return;
      } else {
        /* No more data */
        return;
      }
    }
    data[size] = 0;
    /* Emit data event */
    e = dyad_createEvent(DYAD_EVENT_DATA);
    e.msg = "received data";
    e.data = data;
    e.size = size;
    dyad_emitEvent(stream, &e);
    /* Update status */
    stream->bytesReceived += size;
    stream->lastActivity = dyad_getTime();
    /* Check stream state in case it was closed during one of the data event
     * handlers. */
    if (stream->state != DYAD_STATE_CONNECTED) {
      return;
    }

    /* Handle line event */
    if (dyad_hasListenerForEvent(stream, DYAD_EVENT_LINE)) {
      int i, start;
      char *buf;
      for (i = 0; i < size; i++) {
        dyad_vectorPush(&stream->lineBuffer, data[i]);
      }
      start = 0;
      buf = stream->lineBuffer.data;
      for (i = 0; i < stream->lineBuffer.length; i++) {
        if (buf[i] == '\n') {
          dyad_Event e;
          buf[i] = '\0';
          e = dyad_createEvent(DYAD_EVENT_LINE);
          e.msg = "received line";
          e.data = &buf[start];
          e.size = i - start;
          /* Check and strip carriage return */
          if (e.size > 0 && e.data[e.size - 1] == '\r') {
            e.data[--e.size] = '\0';
          }
          dyad_emitEvent(stream, &e);
          start = i + 1;
          /* Check stream state in case it was closed during one of the line
           * event handlers. */
          if (stream->state != DYAD_STATE_CONNECTED) {
            return;
          }
        }
      }
      if (start == stream->lineBuffer.length) {
        dyad_vectorClear(&stream->lineBuffer);
      } else {
        dyad_vectorSplice(&stream->lineBuffer, 0, start);
      }
    }
  }
}


static void dyad_acceptPendingConnections(dyad_Stream *stream) {
  for (;;) {
    int sockfd = accept(stream->sockfd, NULL, NULL);
    dyad_Stream *remote;
    dyad_Event e;
    if (sockfd == -1 && errno == EWOULDBLOCK) {
      /* No more waiting sockets */
      return;
    }
    /* Create client stream */
    remote = dyad_newStream();
    remote->state = DYAD_STATE_CONNECTED;
    /* Set stream's socket */
    dyad_setSocket(remote, sockfd);
    /* Emit accept event */
    e = dyad_createEvent(DYAD_EVENT_ACCEPT);
    e.msg = "accepted connection";
    e.remote = remote;
    dyad_emitEvent(stream, &e);
    /* Handle invalid socket -- the stream is still made and the ACCEPT event
     * is still emitted, but its shut immediately with an error */
    if (remote->sockfd == -1) {
      dyad_streamError(remote, "failed to create socket on accept");
      return;
    }
  }
}


static int dyad_flushWriteBuffer(dyad_Stream *stream) {
  stream->flags &= ~DYAD_FLAG_WRITTEN;
  if (stream->writeBuffer.length > 0) {
    /* Send data */
    int size = send(stream->sockfd, stream->writeBuffer.data,
                    stream->writeBuffer.length, 0);
    if (size <= 0) {
      if (errno == EWOULDBLOCK) {
        /* No more data can be written */
        return 0;
      } else {
        /* Handle disconnect */
        dyad_close(stream);
        return 0;
      }
    }
    if (size == stream->writeBuffer.length) {
      dyad_vectorClear(&stream->writeBuffer);
    } else {
      dyad_vectorSplice(&stream->writeBuffer, 0, size);
    }
    /* Update status */
    stream->bytesSent += size;
    stream->lastActivity = dyad_getTime();
  } 

  if (stream->writeBuffer.length == 0) {
    dyad_Event e;
    /* If this is a 'closing' stream we can properly close it now */
    if (stream->state == DYAD_STATE_CLOSING) {
      dyad_close(stream);
      return 0;
    }
    /* Set ready flag and emit 'ready for data' event */
    stream->flags |= DYAD_FLAG_READY;
    e = dyad_createEvent(DYAD_EVENT_READY);
    e.msg = "stream is ready for more data";
    dyad_emitEvent(stream, &e);
  }
  /* Return 1 to indicate that more data can immediately be written to the
   * stream's socket */
  return 1;
}



/*===========================================================================*/
/* API                                                                       */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Core                                                                      */
/*---------------------------------------------------------------------------*/

void dyad_update(void) {
  int maxfd; 
  dyad_Stream *stream;
  struct timeval tv;

  dyad_destroyClosedStreams();
  dyad_updateTickTimer();
  dyad_updateStreamTimeouts();

  /* Create fd sets for select */
  dyad_fdsZero(&dyad_readSet);
  dyad_fdsZero(&dyad_writeSet);
  dyad_fdsZero(&dyad_exceptSet);

  stream = dyad_streams;
  maxfd = 0;
  while (stream) {
    switch (stream->state) {
      case DYAD_STATE_CONNECTED:
        dyad_fdsSet(&dyad_readSet, stream->sockfd);
        if (!(stream->flags & DYAD_FLAG_READY) ||
            stream->writeBuffer.length != 0
        ) {
          dyad_fdsSet(&dyad_writeSet, stream->sockfd);
        }
        break;
      case DYAD_STATE_CLOSING:
        dyad_fdsSet(&dyad_writeSet, stream->sockfd);
        break;
      case DYAD_STATE_CONNECTING:
        dyad_fdsSet(&dyad_writeSet, stream->sockfd);
        dyad_fdsSet(&dyad_exceptSet, stream->sockfd);
        break;
      case DYAD_STATE_LISTENING:
        dyad_fdsSet(&dyad_readSet, stream->sockfd);
        break;
    }
    if (stream->sockfd > maxfd) maxfd = stream->sockfd;
    stream = stream->next;
  }

  /* Init timeout value and do select */
  tv.tv_sec = dyad_updateTimeout;
  tv.tv_usec = (dyad_updateTimeout - tv.tv_sec) * 1e6;

  select(maxfd + 1,
         dyad_fdsGetFdSet(&dyad_readSet),
         dyad_fdsGetFdSet(&dyad_writeSet),
         dyad_fdsGetFdSet(&dyad_exceptSet),
         &tv);

  /* Handle streams */
  stream = dyad_streams;
  while (stream) {
    switch (stream->state) {

      case DYAD_STATE_CONNECTED:
        if (dyad_fdsIsSet(&dyad_readSet, stream->sockfd)) {
          dyad_handleReceivedData(stream);
          if (stream->state == DYAD_STATE_CLOSED) {
            break;
          }
        }
        /* Fall through */

      case DYAD_STATE_CLOSING:
        if (dyad_fdsIsSet(&dyad_writeSet, stream->sockfd)) {
          dyad_flushWriteBuffer(stream);
        }
        break;

      case DYAD_STATE_CONNECTING:
        if (dyad_fdsIsSet(&dyad_writeSet, stream->sockfd)) {
          /* Check socket for error */
          int optval = 0;
          socklen_t optlen = sizeof(optval);
          dyad_Event e;
          getsockopt(stream->sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
          if (optval != 0) goto connectFailed;
          /* Handle successful connection */
          stream->state = DYAD_STATE_CONNECTED;
          stream->lastActivity = dyad_getTime();
          dyad_initAddress(stream);
          /* Emit connect event */
          e = dyad_createEvent(DYAD_EVENT_CONNECT);
          e.msg = "connected to server";
          dyad_emitEvent(stream, &e);
        } else if (dyad_fdsIsSet(&dyad_exceptSet, stream->sockfd)) {
          /* Handle failed connection */
          connectFailed:
          dyad_streamError(stream, "could not connect to server");
        }
        break;

      case DYAD_STATE_LISTENING:
        if (dyad_fdsIsSet(&dyad_readSet, stream->sockfd)) {
          dyad_acceptPendingConnections(stream);
        }
        break;
    }

    /* If data was just now written to the stream we should immediately try to
     * send it */
    if (stream->flags & DYAD_FLAG_WRITTEN &&
        stream->state != DYAD_STATE_CLOSED
    ) {
      dyad_flushWriteBuffer(stream);
    }

    stream = stream->next;
  }
}


void dyad_init(void) {
#ifdef _WIN32
  WSADATA dat;
  int err = WSAStartup(MAKEWORD(2, 2), &dat);
  if (err != 0) {
    dyad_panic("WSAStartup failed (%d)", err);
  }
#else
  /* Stops the SIGPIPE signal being raised when writing to a closed socket */
  signal(SIGPIPE, SIG_IGN);
#endif
}


void dyad_shutdown(void) {
  /* Close and destroy all the streams */
  while (dyad_streams) {
    dyad_close(dyad_streams);
    dyad_destroyStream(dyad_streams);
  }
  /* Clear up everything */
  dyad_fdsDeinit(&dyad_readSet);
  dyad_fdsDeinit(&dyad_writeSet);
  dyad_fdsDeinit(&dyad_exceptSet);
  /* Reset state */
  memset(&dyad_readSet, 0, sizeof(dyad_readSet));
  memset(&dyad_writeSet, 0, sizeof(dyad_writeSet));
  memset(&dyad_exceptSet, 0, sizeof(dyad_exceptSet));
#ifdef _WIN32
  WSACleanup();
#endif
}


const char *dyad_getVersion(void) {
  return DYAD_VERSION;
}


double dyad_getTime(void) {
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  return (ft.dwHighDateTime * 4294967296.0 / 1e7) + ft.dwLowDateTime / 1e7;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
#endif
}


int dyad_getStreamCount(void) {
  return dyad_streamCount;
}


void dyad_setTickInterval(double seconds) {
  dyad_tickInterval = seconds;
}


void dyad_setUpdateTimeout(double seconds) {
  dyad_updateTimeout = seconds;
}


dyad_PanicCallback dyad_atPanic(dyad_PanicCallback func) {
  dyad_PanicCallback old = dyad_panicCallback;
  dyad_panicCallback = func;
  return old;
}


/*---------------------------------------------------------------------------*/
/* Stream                                                                    */
/*---------------------------------------------------------------------------*/

dyad_Stream *dyad_newStream(void) {
  dyad_Stream *stream = dyad_realloc(NULL, sizeof(*stream));
  memset(stream, 0, sizeof(*stream));
  stream->state = DYAD_STATE_CLOSED;
  stream->sockfd = -1;
  stream->lastActivity = dyad_getTime();
  /* Add to list and increment count */
  stream->next = dyad_streams;
  dyad_streams = stream;
  dyad_streamCount++;
  return stream;
}


void dyad_addListener(
  dyad_Stream *stream, int event, dyad_Callback callback, void *udata
) {
  dyad_Listener listener;
  listener.event = event;
  listener.callback = callback;
  listener.udata = udata;
  dyad_vectorPush(&stream->listeners, listener);
}


void dyad_removeListener(
  dyad_Stream *stream, int event, dyad_Callback callback, void *udata
) {
  int i = stream->listeners.length;
  while (i--) {
    dyad_Listener *x = &stream->listeners.data[i];
    if (x->event == event && x->callback == callback && x->udata == udata) {
      dyad_vectorSplice(&stream->listeners, i, 1);
    }
  }
}


void dyad_close(dyad_Stream *stream) {
  dyad_Event e;
  if (stream->state == DYAD_STATE_CLOSED) return;
  stream->state = DYAD_STATE_CLOSED;
  /* Emit event */
  e = dyad_createEvent(DYAD_EVENT_CLOSE);
  e.msg = "stream closed";
  dyad_emitEvent(stream, &e);
  /* Clear buffers */
  dyad_vectorClear(&stream->lineBuffer);
  dyad_vectorClear(&stream->writeBuffer);
}


void dyad_end(dyad_Stream *stream) {
  if (stream->state == DYAD_STATE_CLOSED) return;
  if (stream->writeBuffer.length > 0) {
    stream->state = DYAD_STATE_CLOSING;
  } else {
    dyad_close(stream);
  }
}


int dyad_listenEx(
  dyad_Stream *stream, const char *host, int port, int backlog
) {
  struct addrinfo hints, *ai = NULL;
  int err, optval;
  dyad_Event e;

  /* Get addrinfo */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  err = getaddrinfo(host, dyad_intToStr(port), &hints, &ai);
  if (err) {
    dyad_streamError(stream, "could not get addrinfo"); 
    goto fail;
  }
  /* Init socket */
  err = dyad_initSocket(stream, ai->ai_family, ai->ai_socktype,
                        ai->ai_protocol);
  if (err) goto fail;
  /* Set SO_REUSEADDR so that the socket can be immediately bound without
   * having to wait for any closed socket on the same port to timeout */
  optval = 1;
  setsockopt(stream->sockfd, SOL_SOCKET, SO_REUSEADDR,
             &optval, sizeof(optval));
  /* Bind and listen */
  err = bind(stream->sockfd, ai->ai_addr, ai->ai_addrlen);
  if (err) {
    dyad_streamError(stream, "could not bind socket");
    goto fail;
  }
  err = listen(stream->sockfd, backlog);
  if (err) {
    dyad_streamError(stream, "socket failed on listen");
    goto fail;
  }
  stream->state = DYAD_STATE_LISTENING;
  stream->port = port;
  /* Emit listening event */
  e = dyad_createEvent(DYAD_EVENT_LISTEN);
  e.msg = "socket is listening";
  dyad_emitEvent(stream, &e);
  freeaddrinfo(ai);
  return 0;
  fail:
  if (ai) freeaddrinfo(ai);
  return -1;
}


int dyad_listen(dyad_Stream *stream, int port) {
  return dyad_listenEx(stream, NULL, port, 511);
}


int dyad_connect(dyad_Stream *stream, const char *host, int port) {
  struct addrinfo hints, *ai = NULL;
  int err;

  /* Resolve host */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  err = getaddrinfo(host, dyad_intToStr(port), &hints, &ai);
  if (err) {
    dyad_streamError(stream, "could not resolve host");
    goto fail;
  }
  /* Start connecting */
  err = dyad_initSocket(stream, ai->ai_family, ai->ai_socktype,
                        ai->ai_protocol);
  if (err) goto fail;
  connect(stream->sockfd, ai->ai_addr, ai->ai_addrlen);
  stream->state = DYAD_STATE_CONNECTING;
  freeaddrinfo(ai);
  return 0;
  fail:
  if (ai) freeaddrinfo(ai);
  return -1;
}


void dyad_write(dyad_Stream *stream, void *data, int size) {
  char *p = data;
  while (size--) {
    dyad_vectorPush(&stream->writeBuffer, *p++);
  }
  stream->flags |= DYAD_FLAG_WRITTEN;
}


void dyad_vwritef(dyad_Stream *stream, const char *fmt, va_list args) {
  char buf[64];
  char *str;
  char f[] = "%_";
  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 'f': case 'g': case 'd': case 'i': case 'x': case 'X': case 'p':
          f[1] = *fmt;
          vsprintf(buf, (const char*) f, args);
          str = buf;
          goto writeStr;
        case 'c':
          dyad_vectorPush(&stream->writeBuffer, va_arg(args, int));
          break;
        case 's': {
          str = va_arg(args, char*);
          if (str == NULL) str = "(null)";
          writeStr:
          while (*str) {
            dyad_vectorPush(&stream->writeBuffer, *str++);
          }
          break;
        }
        default:
          dyad_vectorPush(&stream->writeBuffer, *fmt);
      }
    } else {
      dyad_vectorPush(&stream->writeBuffer, *fmt);
    }
    fmt++;
  }
  stream->flags |= DYAD_FLAG_WRITTEN;
}


void dyad_writef(dyad_Stream *stream, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  dyad_vwritef(stream, fmt, args);
  va_end(args);
}


void dyad_setTimeout(dyad_Stream *stream, double seconds) {
  stream->timeout = seconds;
}


void dyad_setNoDelay(dyad_Stream *stream, int opt) {
  opt = !!opt;
  setsockopt(stream->sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
}


int dyad_getState(dyad_Stream *stream) {
  return stream->state;
}


const char *dyad_getAddress(dyad_Stream *stream) {
  return stream->address ? stream->address : "";
}


int dyad_getPort(dyad_Stream *stream) {
  return stream->port;
}


int dyad_getBytesSent(dyad_Stream *stream) {
  return stream->bytesSent;
}


int dyad_getBytesReceived(dyad_Stream *stream) {
  return stream->bytesReceived;
}


int dyad_getSocket(dyad_Stream *stream) {
  return stream->sockfd;
}
