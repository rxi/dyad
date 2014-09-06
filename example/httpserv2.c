#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dyad.h"

/* An example of a simple HTTP server which serves up files. We make use of
 * `udata` and the DYAD_EVENT_READY event to send files in chunks as needed
 * instead of loading the entire file into the stream's write buffer in one go.
 * This allows us to send large files without any issues */

typedef struct {
  FILE *fp;
} Client;


static void client_onReady(dyad_Event *e) {
  Client *client = e->udata;
  int c;
  int count = 32000;
  while (count--) {
    if ((c = fgetc(client->fp)) != EOF) {
      dyad_write(e->stream, &c, 1);
    } else {
      dyad_end(e->stream);
      break;
    }
  }
}

static void client_onLine(dyad_Event *e) {
  Client *client = e->udata;
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
      dyad_end(e->stream);
    } else {
      /* Handle good request */
      client->fp = fopen(path + 1, "rb");
      if (client->fp) {
        /* Remove this event handler (we don't care about anything else the
         * client has to say) */
        dyad_removeListener(e->stream, DYAD_EVENT_LINE, client_onLine, client);
        /* Add the event handler for sending the file */
        dyad_addListener(e->stream, DYAD_EVENT_READY, client_onReady, client);
      } else {
        dyad_writef(e->stream, "not found '%s'\n", path);
        dyad_end(e->stream);
      }
    }
  }
}

static void client_onClose(dyad_Event *e) {
  Client *client = e->udata;
  if (client->fp) fclose(client->fp);
  free(client);
}


static void server_onAccept(dyad_Event *e) {
  Client *client = calloc(1, sizeof(*client));
  dyad_addListener(e->remote, DYAD_EVENT_LINE,  client_onLine,  client);
  dyad_addListener(e->remote, DYAD_EVENT_CLOSE, client_onClose, client);
}

static void server_onListen(dyad_Event *e) {
  printf("server listening: http://localhost:%d\n", dyad_getPort(e->stream));
}

static void server_onError(dyad_Event *e) {
  printf("server error: %s\n", e->msg);
}


int main(void) {
  dyad_Stream *serv;
  dyad_init();

  serv = dyad_newStream();
  dyad_addListener(serv, DYAD_EVENT_ERROR,  server_onError,  NULL);
  dyad_addListener(serv, DYAD_EVENT_ACCEPT, server_onAccept, NULL);
  dyad_addListener(serv, DYAD_EVENT_LISTEN, server_onListen, NULL);
  dyad_listen(serv, 8000);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }

  dyad_shutdown();
  return 0;
}
