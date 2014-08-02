#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dyad.h"

/* A slightly better simple HTTP server: Unlike the simpler httpserv.c example
 * we make use of 'udata' and the DYAD_EVENT_READY event to send files in
 * chunks as needed instead of loading the entire file into the stream's write
 * buffer in one go; this allows us to send large files without any issue. */

typedef struct {
  FILE *fp;
} Conn;


/*---------------------------------------------------------------------------*/
/* Client Stream                                                             */
/*---------------------------------------------------------------------------*/

static void onReady(dyad_Event *e) {
  Conn *conn = e->udata;
  int c;
  int count = 32000;
  while (count--) {
    if ((c = fgetc(conn->fp)) != EOF) {
      dyad_write(e->stream, &c, 1);
    } else {
      dyad_end(e->stream);
      break;
    }
  }
}

static void onLine(dyad_Event *e) {
  Conn *conn = e->udata;
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
      conn->fp = fopen(path + 1, "rb");
      if (conn->fp) {
        /* Remove this event handler (we don't care about anything else the
         * client has to say) */
        dyad_removeListener(e->stream, DYAD_EVENT_LINE, onLine, conn);
        /* Add the event handler for sending the file */
        dyad_addListener(e->stream, DYAD_EVENT_READY, onReady, conn);
      } else {
        dyad_writef(e->stream, "not found '%s'\n", path);
        dyad_end(e->stream);
      }
    }
  }
}

static void onClose(dyad_Event *e) {
  Conn *conn = e->udata;
  if (conn->fp) fclose(conn->fp);
  free(conn);
}


/*---------------------------------------------------------------------------*/
/* Server Stream                                                             */
/*---------------------------------------------------------------------------*/

static void onAccept(dyad_Event *e) {
  Conn *conn = calloc(1, sizeof(*conn));
  dyad_addListener(e->remote, DYAD_EVENT_LINE,  onLine,  conn);
  dyad_addListener(e->remote, DYAD_EVENT_CLOSE, onClose, conn);
}

static void onListen(dyad_Event *e) {
  printf("server listening on port %d\n", dyad_getPort(e->stream));
}

static void onError(dyad_Event *e) {
  printf("server error: %s\n", e->msg);
}


int main(void) {
  dyad_Stream *serv;
  dyad_init();

  serv = dyad_newStream();
  dyad_addListener(serv, DYAD_EVENT_ERROR,  onError,  NULL);
  dyad_addListener(serv, DYAD_EVENT_ACCEPT, onAccept, NULL);
  dyad_addListener(serv, DYAD_EVENT_LISTEN, onListen, NULL);
  dyad_listen(serv, 8000);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }

  dyad_shutdown();
  return 0;
}
