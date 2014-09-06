#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dyad.h"

/* A very simple HTTP server which responds to a number of paths with different
 * information. See httpserv2.c for an example of an HTTP server which responds
 * with files. */

static int count = 0;

static void onLine(dyad_Event *e) {
  char path[128];
  if (sscanf(e->data, "GET %127s", path) == 1) {
    /* Print request */
    printf("%s %s\n", dyad_getAddress(e->stream), path);
    /* Send header */
    dyad_writef(e->stream, "HTTP/1.1 200 OK\r\n\r\n");
    /* Handle request */
    if (!strcmp(path, "/")) {
      dyad_writef(e->stream, "<html><body><pre>"
                             "<a href='/date'>date</a><br>"
                             "<a href='/count'>count</a><br>"
                             "<a href='/ip'>ip</a>"
                             "</pre></html></body>" );

    } else if (!strcmp(path, "/date")) {
      time_t t = time(0);
      dyad_writef(e->stream, "%s", ctime(&t));

    } else if (!strcmp(path, "/count")) {
      dyad_writef(e->stream, "%d", ++count);

    } else if (!strcmp(path, "/ip")) {
      dyad_writef(e->stream, "%s", dyad_getAddress(e->stream));

    } else {
      dyad_writef(e->stream, "bad request '%s'", path);
    }
    /* Close stream when all data has been sent */
    dyad_end(e->stream);
  }
}

static void onAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_LINE, onLine, NULL);
}

static void onListen(dyad_Event *e) {
  printf("server listening: http://localhost:%d\n", dyad_getPort(e->stream));
}

static void onError(dyad_Event *e) {
  printf("server error: %s\n", e->msg);
}


int main(void) {
  dyad_Stream *s;
  dyad_init();

  s = dyad_newStream();
  dyad_addListener(s, DYAD_EVENT_ERROR,  onError,  NULL);
  dyad_addListener(s, DYAD_EVENT_ACCEPT, onAccept, NULL);
  dyad_addListener(s, DYAD_EVENT_LISTEN, onListen, NULL);
  dyad_listen(s, 8000);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }

  return 0;
}
