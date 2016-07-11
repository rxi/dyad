#pragma once
#include "dyad.h"

namespace dyadpp {
	class dyad_event;
	typedef void(*dyadpp_Callback)(dyad_event);

	class dyad
	{
	public:
		static void begin() {
			for (; getStreamCount() > 0; update() );
		}
#ifdef DYADPP_USE_DANGEROUS
		static void begin_thread();
		static void destory_thread();
#endif
		static void init() {
			dyad_init();
		}
		static void update() {
			dyad_update();
		}
		static void shutdown();
		static const char *getVersion() {
			return dyad_getVersion();
		}
		static double getTime() {
			return dyad_getTime();
		}
		static int getStreamCount() {
			return dyad_getStreamCount();
		}
		static void setUpdateTimeout(double seconds) {
			dyad_setUpdateTimeout(seconds);
		}
		static void setTickInterval(double seconds) {
			dyad_setTickInterval(seconds);
		}
		static dyad_PanicCallback panic(dyad_PanicCallback func) {
			return dyad_atPanic(func);
		}
	};
#ifdef DYADPP_USE_DANGEROUS
	inline void dyad::shutdown() { dyad_shutdown(); destory_thread(); }
#ifdef _WIN32
	static HANDLE thread;
	inline void dyad::begin_thread() {
		thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)begin, NULL, 0, NULL);
	}
	inline void dyad::destory_thread() {
		TerminateThread(thread, 0);
	}
#else
	static pthread_t thread;
	inline void dyad::begin_thread() {
		pthread_create(&thread, NULL, (void*)&begin, NULL);
	}
	inline void dyad::destory_thread() {
		pthread_kill(thread, 0);
	}
#endif
#else
	inline void dyad::shutdown() { dyad_shutdown(); }
#endif
	class dyad_listener;
	class dyad_client;
	class dyad_server;

	class dyad_stream
	{
	public:
		dyad_stream(dyad_Stream *stream) {
			m_wrap = stream;
		}
		dyad_stream() : m_wrap(dyad_newStream()) {
			m_wrap = dyad_newStream();
		}
		//~dyad_stream() { kill(); } /* RAII issue */

		int listen(int port) {
			return dyad_listen(m_wrap, port);
		}
		int listen(const char *host, int port, int backlog = INT_MAX) {
			return dyad_listenEx(m_wrap, host, port, backlog);
		}
		int connect(const char *host, int port) {
			return dyad_connect(m_wrap, host, port);
		}
		void addListener(int event, dyad_Callback callback, void *udata = NULL) {
			dyad_addListener(m_wrap, event, callback, udata);
		}
		void addListener(int event, dyadpp_Callback callback, void *udata = NULL) {
			dyad_addListener(m_wrap, event, (dyad_Callback)callback, udata);
		}
		void removeListener(int event, dyad_Callback callback, void *udata = NULL) {
			dyad_removeListener(m_wrap, event, callback, udata);
		}
		void removeListener(int event, dyadpp_Callback callback, void *udata = NULL) {
			dyad_removeListener(m_wrap, event, (dyad_Callback)callback, udata);
		}
		void removeAllListeners(int event) {
			dyad_removeAllListeners(m_wrap, event);
		}
		dyad_listener on(int event);
		void write(const void *data, int size) {
			dyad_write(m_wrap, data, size);
		}
		void send(const void *data, int size) {
			write(data, size);
		}
		void writef(const char *fmt, ...) {
			dyad_writef(m_wrap, fmt);
		}
		void sendf(const char *fmt, ...) {
			writef(fmt);
		}
		void printf(const char *fmt, ...) {
			writef(fmt);
		}
		void end() {
			dyad_end(m_wrap);
		}
		void close() {
			dyad_close(m_wrap);
		}
		void setTimeout(double seconds) {
			dyad_setTimeout(m_wrap, seconds);
		}
		void setNoDelay(bool delay) {
			dyad_setNoDelay(m_wrap, delay);
		}

		int getState() {
			return dyad_getState(m_wrap);
		}
		const char *getAddress() {
			return dyad_getAddress(m_wrap);
		}
		int getPort() {
			return dyad_getPort(m_wrap);
		}
		int getBytesReceived() {
			return dyad_getBytesReceived(m_wrap);
		}
		int getBytesSent() {
			return dyad_getBytesSent(m_wrap);
		}
		dyad_Socket getSocket() {
			return dyad_getSocket(m_wrap);
		}
		dyad_Stream *getPtr() {
			return m_wrap;
		}
		void kill() {
			end(); 
			close();
		}

		dyad_client asClient();
		dyad_server asServer();
	private:
		dyad_Stream *m_wrap;
	};

	class dyad_event
	{
	public:
		dyad_event(dyad_Event *event) {
			m_wrap = event;
		}

		int getType() {
			return m_wrap->type;
		}
		void* getUserData() {
			return m_wrap->udata;
		}

		dyad_stream getStream() {
			return dyad_stream(m_wrap->stream);
		}
		dyad_stream getSender() {
			return getStream();
		}

		dyad_stream getRemoteStream() {
			return dyad_stream(m_wrap->remote);
		}
		dyad_client getClient();

		const char *getEventMessage() {
			return m_wrap->msg;
		}
		const char *getData() {
			return m_wrap->data;
		}
		int getDataSize() {
			return m_wrap->size;
		}
		dyad_Event *getEvent() {
			return m_wrap;
		}
		bool isServerEvent() {
			return m_wrap->remote != NULL;
		}
	private:
		dyad_Event *m_wrap;
	};

	class dyad_listener
	{
	public:
		dyad_listener(dyad_stream stream, int event) : m_stream(stream), m_event(event) {
			m_stream = stream;
			m_event = event;
		}

		void add(dyad_Callback callback, void *udata = NULL) {
			m_stream.addListener(m_event, callback, udata);
		}
		void add(dyadpp_Callback callback, void *udata = NULL) {
			m_stream.addListener(m_event, callback, udata);
		}
		void remove(dyad_Callback callback, void *udata = NULL) {
			m_stream.removeListener(m_event, callback, udata);
		}
		void remove(dyadpp_Callback callback, void *udata = NULL) {
			m_stream.removeListener(m_event, callback, udata);
		}
		void removeAll() {
			m_stream.removeAllListeners(m_event);
		}
		void operator +=(dyad_Callback callback) { 
			add(callback); 
		}
		void operator +=(dyadpp_Callback callback) { 
			add(callback); 
		}
		void operator -=(dyad_Callback callback) { 
			remove(callback);  
		}
		void operator -=(dyadpp_Callback callback) { 
			remove(callback); 
		}
	private:
		dyad_stream m_stream;
		int m_event;
	};
	inline dyad_listener dyad_stream::on(int event) {
		return dyad_listener(*this, event);
	}

	class dyad_client {
	public:
		dyad_client(dyad_stream stream) {
			m_stream = stream;
		}
		dyad_client() {
			
		}
		dyad_client(const char *host, int port) {
			m_stream.connect(host, port);
		}
		void connect(const char *host, int port) {
			m_stream.connect(host, port);
		}

		const char *getAddress() {
			return m_stream.getAddress();
		}
		int getPort() {
			return m_stream.getPort();
		}
		dyad_listener onDestroyed() {
			return m_stream.on(DYAD_EVENT_DESTROY);
		}
		dyad_listener onBufferReady() {
			return m_stream.on(DYAD_EVENT_READY);
		}
		dyad_listener onConnect() {
			return m_stream.on(DYAD_EVENT_CONNECT);
		}
		dyad_listener onData() {
			return m_stream.on(DYAD_EVENT_DATA);
		}
		dyad_listener onLine() {
			return m_stream.on(DYAD_EVENT_LINE);
		}
		dyad_listener onClose() {
			return m_stream.on(DYAD_EVENT_CLOSE);
		}
		dyad_listener onError() {
			return m_stream.on(DYAD_EVENT_ERROR);
		}
		dyad_listener onTimeout() {
			return m_stream.on(DYAD_EVENT_TIMEOUT);
		}

		dyad_stream getStream() {
			return m_stream;
		}
	private:
		dyad_stream m_stream;
	};
	dyad_client dyad_event::getClient() {
		return dyad_client(getRemoteStream());
	}
	inline dyad_client dyad_stream::asClient() {
		return dyad_client(*this);
	}

	class dyad_server {
	public:
		dyad_server() {

		}
		dyad_server(dyad_stream stream) {
			m_stream = stream;
		}
		dyad_server(const char *host, int port) {
			m_host = host;
			m_port = port;
		}
		dyad_server(int port, bool ipv4 = false) {
			m_host = ipv4 ? "0.0.0.0" : NULL;
			m_port = port;
		}

		void begin() {
			if (!m_host) {
				m_stream.listen(m_port);
			}
			else {
				m_stream.listen(m_host, m_port);
			}
		}
		void listen() {
			begin();
		}
		void end() {
			m_stream.kill();
		}
		void shutdown() {
			end();
		}

		const char *setAddress(const char *host) {
			m_host = host;
		}
		void setPort(int port) {
			m_port = port;
		}
		const char *getAddress() {
			return m_host;
		}
		int getPort() {
			return m_port;
		}
		dyad_listener onDestroyed() {
			return m_stream.on(DYAD_EVENT_DESTROY);
		}
		dyad_listener onBufferReady() {
			return m_stream.on(DYAD_EVENT_READY);
		}
		dyad_listener onListen() {
			return m_stream.on(DYAD_EVENT_LISTEN);
		}
		dyad_listener onAccept() {
			return m_stream.on(DYAD_EVENT_ACCEPT);
		}
		dyad_listener onData() {
			return m_stream.on(DYAD_EVENT_DATA);
		}
		dyad_listener onLine() {
			return m_stream.on(DYAD_EVENT_LINE);
		}
		dyad_listener onClose() {
			return m_stream.on(DYAD_EVENT_CONNECT);
		}
		dyad_listener onError() {
			return m_stream.on(DYAD_EVENT_ERROR);
		}
		dyad_listener onTimeout() {
			return m_stream.on(DYAD_EVENT_TIMEOUT);
		}
		dyad_stream getStream() {
			return m_stream;
		}
	private:
		const char *m_host;
		int m_port;
		dyad_stream m_stream;
	};
	inline dyad_server dyad_stream::asServer() {
		return dyad_server(*this);
	}
}
