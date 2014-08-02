#include <stdio.h>
#include "dyad.h"

/* Makes a simple HTTP request and prints the response */

static void onConnect(dyad_Event *e) {
  dyad_writef(e->stream, "GET / HTTP/1.0\r\n\r\n");
}

static void onError(dyad_Event *e) {
  printf("error: %s\n", e->msg);
}

static void onData(dyad_Event *e) {
  printf("%s", e->data);
}

int main(void) {
  dyad_Stream *s;
  dyad_init();

  s = dyad_newStream();
  dyad_addListener(s, DYAD_EVENT_CONNECT, onConnect, NULL);
  dyad_addListener(s, DYAD_EVENT_ERROR,   onError,   NULL);
  dyad_addListener(s, DYAD_EVENT_DATA,    onData,    NULL);
  dyad_connect(s, "xkcd.com", 80);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }
  
  dyad_shutdown();
  return 0;
}
