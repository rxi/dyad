#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dyad.h"

/* An echo server: Echos any data received by a client back to the client */

static void onData(dyad_Event *e) {
  dyad_write(e->stream, e->data, e->size);
}

static void onAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_DATA, onData, NULL);
  dyad_writef(e->remote, "echo server\r\n");
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
  dyad_listen(s, 8000);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }

  return 0;
}
