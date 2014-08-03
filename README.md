
![Header Image](https://cloud.githubusercontent.com/assets/3920290/3791159/d5ebd230-1b2b-11e4-89fd-8650d7a60442.png)

## Overview
Dyad is an asynchronous networking library which aims to be lightweight,
portable and easy to use. Dyad can be used both to create small standalone
servers and to provide network support to existing projects.

## Getting started
The [dyad.c](src/dyad.c?raw=1) and [dyad.h](src/dyad.h?raw=1) files can be
dropped into an existing project; if you're using Windows you will also have to
link to `ws2_32`.

An overview of the API can be found at [doc/api.md](doc/api.md).

Usage examples can be found at [example/](example/).

## Server example
A simple server which listens on port 8000 and echoes whatever is sent to it:
```c
#include "dyad.h"

static void onData(dyad_Event *e) {
  dyad_write(e->stream, e->data, e->size);
}

static void onAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_DATA, onData, NULL);
  dyad_writef(e->remote, "Echo server\r\n");
}

int main(void) {
  dyad_init();

  dyad_Stream *serv = dyad_newStream();
  dyad_addListener(serv, DYAD_EVENT_ACCEPT, onAccept, NULL);
  dyad_listen(serv, 8000);

  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }

  dyad_shutdown();
  return 0;
}
```

## License
This library is free software; you can redistribute it and/or modify it under
the terms of the MIT license. See [LICENSE](LICENSE) for details.
