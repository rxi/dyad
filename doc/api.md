# Dyad API

* [Functions](#functions)
* [Events](#events)
* [States](#states)
* [Types](#types)

## Functions

#### void dyad\_init(void)
Initializes dyad. This should be called before any other dyad function is
called.

#### void dyad\_update(void)
Handles all the active streams and events. This should be called with each
iteration of the program's main loop. By default `dyad_update()` will block for
a maximum of 1 second, `dyad_update()` can be made not to block by using
`dyad_setUpdateTimeout()` to set the timeout to `0`.

#### void dyad\_shutdown(void)
Emits the `DYAD_EVENT_CLOSE` event on all current streams then shuts down and
clears up everything. This should be called at the end of the program or when
we no longer need dyad.

#### const char *dyad\_getVersion(void)
Returns the version of the library as a string.

#### double dyad\_getTime(void)
Returns the current time in seconds. This time should only be used for
comparisons, as no specific epoch is guaranteed.

#### int dyad\_getStreamCount(void) 
Returns the number of currently active streams.

#### void dyad\_setUpdateTimeout(double seconds)
Sets the maximum number of seconds the `dyad_update()` function can block for.
If `seconds` is `0` then `dyad_update()` will not block.

#### void dyad\_setTickInterval(double seconds) 
Sets the interval in seconds that the `DYAD_EVENT_TICK` event is emited to all
streams; the default is 1 second.

#### dyad\_PanicCallback dyad\_atPanic(dyad\_PanicCallback func)
Sets the function which will be called when dyad encounters an error it cannot
recover from. The old atPanic function is returned or `NULL` if none was set.

---

#### dyad\_Stream* dyad\_newStream(void)
Creates and returns a new stream. The returned stream is in a closed state by
default, `dyad_listen()`, `dyad_listenEx()` or `dyad_connect()` should be
called on the stream to take it out of the closed state. See
[dyad\_Stream](#dyad_stream).

#### int dyad\_listen(dyad\_Stream \*stream, int port)
Makes the `stream` begin listening on the given `port` on all local interfaces.

#### int dyad\_listenEx(dyad\_Stream \*stream, const char \*host, int port, int backlog)
Performs the same task as `dyad_listen()` but provides additional options:
`host` is the address of the local interface the stream should listen on.
`backlog` is the maximum length of the queue of pending connections.

#### int dyad\_connect(dyad\_Stream \*stream, const char \*host, int port)
Connects the `stream` to the remote `host`.

#### void dyad\_addListener(dyad\_Stream \*stream, int event, dyad\_Callback callback, void \*udata)
Adds a listener for the `event` to the `stream`. When the event occurs the
`callback` is called and the event's udata field is set to `udata`. If several
listeners are added for the same event they are emitted in the order which they
were added. See [Events](#events).

#### void dyad\_removeListener(dyad\_Stream \*stream, int event, dyad\_Callback callback, void *udata)
Removes an event listener which was added with the `dyad_addListener()`
function.

#### void dyad\_removeAllListeners(dyad\_Stream \*stream, int event)
Removes all listeners for the given `event`. If `event` is `DYAD_EVENT_NULL`
then all listeners for all events are removed.

#### void dyad\_write(dyad\_Stream \*stream, const void \*data, int size)
Writes the `data` of the given `size` to the stream. If you want to send a
null terminated string use `dyad_writef()` instead.

#### void dyad\_writef(dyad\_Stream \*stream, const char \*fmt, ...)
Writes a formatted string to the `stream`. The function is similar to
`sprintf()` with the following differences:
* Dyad takes care of allocating enough memory for the result.
* There are no flags, widths or precisions; only the following *standard*
  specifiers are supported: `%%` `%s` `%f` `%g` `%d` `%i` `%c` `%x` `%X` `%p`.
* The `%r` specifier is provided, this takes a `FILE*` argument
  and writes the contents until the EOF is reached.
* The `%b` specifier is provided, this takes a `void*` argument followed by an
  `int` argument representing the number of bytes to be written.


#### void dyad\_vwritef(dyad\_Stream stream, const char \*fmt, va\_list args)
`dyad_vwritef()` is to `dyad_writef()` what `vsprintf()` is to `sprintf()`.

#### void dyad\_end(dyad\_Stream \*stream)
Finishes sending any data the `stream` has left in its write buffer then closes
the stream. The stream will stop receiving data once this function is called.

#### void dyad\_close(dyad\_Stream \*stream)
Immediately closes the `stream`, discarding any data left in its write buffer.

#### void dyad\_setTimeout(dyad\_Stream \*stream, double seconds)
Sets the number of seconds the `stream` can go without any activity (receiving
or sending data) before it is automatically closed. If `0` seconds is set then
the stream will never timeout, this is the default.

#### void dyad\_setNoDelay(dyad\_Stream \*stream, int opt)
If `opt` is `1` then Nagle's algorithm is disabled for the stream's socket,
`0` enables it; by default it is enabled.

#### int dyad\_getState(dyad\_Stream \*stream)
Returns the current state of the `stream`, for example a connected stream would
return the value `DYAD_STATE_CONNECTED`. See [States](#states).

#### const char \*dyad\_getAddress(dyad\_Stream \*stream)
Returns the current IP address for the `stream`. For listening streams this is
the local address, for connected streams it is the remote address.

#### int dyad\_getPort(dyad\_Stream \*stream)
Returns the current `port` the `stream` is using. For listening streams this is
the local port, for connected streams it is the remote port.

#### int dyad\_getBytesReceived(dyad\_Stream \*stream)
Returns the number of bytes which have been received by the `stream` since it
was made.

#### int dyad\_getBytesSent(dyad\_Stream \*stream)
Returns the number of bytes which have been sent by the `stream` since it was
made. This does not include the data in the stream's write buffer which is
still waiting to be sent.

#### dyad_Socket dyad\_getSocket(dyad\_Stream \*stream)
Returns the socket used by the `stream`.


## Events

#### DYAD\_EVENT\_DESTROY
Emitted when a stream is destroyed. Once all the listeners for this event have
returned then the associated stream's pointer should no longer be considered
valid.

#### DYAD\_EVENT\_ACCEPT
Emitted when a listening stream accepts a connection. The `remote` field of the
event represents the connected client's stream.

#### DYAD\_EVENT\_LISTEN
Emitted when a listening stream begins listening.

#### DYAD\_EVENT\_CONNECT
Emitted when a connecting stream successfully makes the connection to its host.

#### DYAD\_EVENT\_CLOSE
Emitted when a stream is closed. Closed streams are automatically destroyed by
`dyad_update()`, see [DYAD\_EVENT\_DESTROY](#dyad_event_destroy)

#### DYAD\_EVENT\_READY
Emitted once each time the stream becomes ready to send more data. This event
is useful when writing a large file to a stream, allowing you to send it in
smaller chunks rather than copying all the data to the stream's write buffer at
once.

#### DYAD\_EVENT\_DATA
Emitted whenever the stream receives data. The event's `data` field is the
received data (null terminated), the `size` field is set to the size of the
received data (excluding the null terminator).

#### DYAD\_EVENT\_LINE
Emitted whenever the stream receives a line of data. The event's `data` field
is set to the received line stripped of the `\n` or `\r\n` and null terminated,
the `size` field is the length of string.

#### DYAD\_EVENT\_ERROR
Emitted whenever an error occurs on a stream. For example, when creating a
listening socket this event will be emitted if it cannot be bound. The stream
is immediately closed after this event.

#### DYAD\_EVENT\_TIMEOUT
Emitted when the stream times out (see `dyad_setTimeout()`). The stream is
immediately closed after this event.

#### DYAD\_EVENT\_TICK
Emitted every time a tick occurs. A tick is an event which is emitted on every
stream at a constant interval; this interval can be set using
`dyad_setTickInterval()`.


## States

#### DYAD\_STATE\_CLOSED
The stream is closed. Streams are created in this state, and, if left in this
state, are destroyed automatically.

#### DYAD\_STATE\_CLOSING
The stream is connected but still has data in its write buffer thats waiting to
be sent before it closes.

#### DYAD\_STATE\_CONNECTING
The stream is attempting to connect to a host.

#### DYAD\_STATE\_CONNECTED
The stream is connected and can send or receive data.

#### DYAD\_STATE\_LISTENING
The stream is currently listening for connections to accept.


## Types

#### dyad\_Socket
Represents a socket for the given platform.

#### dyad\_Stream
A stream created with `dyad_newStream()` or by a listening server when it
accepts a connection. Dyad handles the resources allocated for all streams;
closed streams are automatically destroyed by the `dyad_update()` function.

#### dyad\_Event
Contains an event's information, a pointer of which is passed to a
`dyad_Callback` function when an event occurs. This struct contains the
following fields:

Field                 | Description
----------------------|-------------------------------------------------------
int type              | The type of event which was emitted
void \*udata          | The `udata` pointer passed to `dyad_addEventListener()`
dyad\_Stream \*stream | The stream which emitted this event
dyad\_Stream \*remote | The client stream when a connection is accepted
const char \*msg      | A description of the event or error message
char \*data           | The events associated data
int size              | The size in bytes of the event's associated data

The `dyad_Event` struct and the data pointed to by its `data` field should
never be modified by an event callback.

#### dyad\_Callback
An event listener callback function which can be passed to
`dyad_addListener()`. The function should be of the following
form:
```c
void func(dyad_Event *event);
```

#### dyad\_PanicCallback
An atPanic callback function which can be passed to `dyad_atPanic()`. The
function should be of the following form:
```c
void func(const char *message);
```

