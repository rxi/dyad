#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dyad.h"

/* A very simple HTTP server */

static void onLine(dyad_Event *e) {
  char path[128];
  if (sscanf(e->data, "GET %127s", path) == 1) {
    /* Print request */
    printf("%s %s\n", dyad_getAddress(e->stream), path);
    /* Send header */
    dyad_writef(e->stream, "HTTP/1.1 200 OK\r\n\r\n");
    /* Check request */
    if (strstr(path, "..") || path[1] == '/') {
      /* Handle bad request */
      dyad_writef(e->stream, "bad request '%s'", path);
    } else {
      /* Handle good request */
      FILE *fp = fopen(path + 1, "rb");
      if (fp) {
        int c;
        while ((c = fgetc(fp)) != EOF) {
          dyad_write(e->stream, &c, 1);
        }
        fclose(fp);
      } else {
        dyad_writef(e->stream, "not found '%s'\n", path);
      }
    }
    /* Close stream when all data has been sent */
    dyad_end(e->stream);
  }
}

static void onAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_LINE, onLine, NULL);
}

static void onListen(dyad_Event *e) {
  printf("server listening on port %d\n", dyad_getPort(e->stream));
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
