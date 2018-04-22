#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dyad.h"

/* An Unix server: Connects and send message to a server */

static const char data_to_be_sent [] = {"Dyad.c is an asynchronous networking library"};

static void onConnect(dyad_Event *e) {
  printf("connected: %s\n", e->msg);
}

static void onData(dyad_Event *e) {
  printf("%s", e->data);
}

static void onError(dyad_Event *e) {
  fprintf(stderr, "%s", e->data);
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  dyad_Stream *s;
  int bytes_sent;
  const int size = strlen(data_to_be_sent);

  if (argc != 2) {
      fprintf(stderr, "usage: %s <unix socket>\n", argv[0]);
      return EXIT_FAILURE;
  }

  dyad_init();

  s = dyad_newStream();
  dyad_addListener(s, DYAD_EVENT_CONNECT, onConnect, NULL);
  dyad_addListener(s, DYAD_EVENT_DATA,    onData,    NULL);
  dyad_addListener(s, DYAD_EVENT_ERROR,   onError,   NULL);
  if (dyad_unix_connect(s, argv[1]) == -1) {
      return EXIT_FAILURE;
  }

  dyad_write(s, data_to_be_sent, size);
  while (bytes_sent < size) {
      bytes_sent = dyad_getBytesSent(s);
      dyad_update();
  }

  dyad_shutdown();
  return EXIT_SUCCESS;
}
