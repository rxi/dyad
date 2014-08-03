#include <stdio.h>
#include <string.h>
#include "dyad.h"

/* A simple IRC bot. Connects to an IRC network, joins a channel then sits
 * idle, responding to the server's PING messges and printing everything the
 * server sends it. */

static char *server  = "irc.afternet.org";
static char *channel = "#dyadbots";
static char nick[32];
static int  isRegistered = 0;


static void onConnect(dyad_Event *e) {
  /* Generate a random nick name */
  sprintf(nick, "testbot%04x", (int)(dyad_getTime()) % 0xFFFF);
  /* Introduce ourselves to the server */
  dyad_writef(e->stream, "NICK %s\r\n", nick);
  dyad_writef(e->stream, "USER %s %s bla :%s\r\n", nick, nick, nick);
}

static void onError(dyad_Event *e) {
  printf("error: %s\n", e->msg);
}

static void onLine(dyad_Event *e) {
  printf("%s\n", e->data);
  /* Handle PING */
  if (!memcmp(e->data, "PING", 4)) {
    dyad_writef(e->stream, "PONG%s\r\n", e->data + 4);
  }
  /* Handle RPL_WELCOME */
  if (!isRegistered && strstr(e->data, "001")) {
    /* Join channel */
    dyad_writef(e->stream, "JOIN %s\r\n", channel);
    isRegistered = 1;
  }
}

int main(void) {
  dyad_Stream *s;
  dyad_init();

  s = dyad_newStream();
  dyad_addListener(s, DYAD_EVENT_CONNECT, onConnect, NULL);
  dyad_addListener(s, DYAD_EVENT_ERROR,   onError,   NULL);
  dyad_addListener(s, DYAD_EVENT_LINE,    onLine,    NULL);
  dyad_connect(s, server, 6667);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }
  
  dyad_shutdown();
  return 0;
}
