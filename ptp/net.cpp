/*
 * Copyright (c) 2001 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * 3. Neither the name of the Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef WIN32
#include <winsock.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <ptp/net.h>
#include <ptp/debug.h>

PTP::Net::Ip PTP::Net::Connection::s_proxyIp = 0;
PTP::Net::Port PTP::Net::Connection::s_proxyPort = 0;
int PTP::Net::Connection::s_proxyTimeout = 0;

#define STRNCMP_CONST(x, y) strncmp((const char*)(x), (y), sizeof(y) - 1)

#ifdef WIN32

typedef int socklen_t;

class NetInit
{
public:
	NetInit();
	~NetInit();
};

#undef s_net
static NetInit s_net;

/*
 * NetInit::NetInit: Initialize networking.
 */
NetInit::NetInit()
{
	WSADATA info;
	WSAStartup(MAKEWORD(2, 2), &info);
}

/*
 * NetInit::~NetInit: Terminate networking.
 */
NetInit::~NetInit()
{
	WSACleanup();
}

#endif // WIN32

/**
 * PTP::Net::Lookup: Lookup IP address and port of a URL.
 * Type: static
 * @url: URL string or "localhost" for local address.
 * @port: [$OUT] URL port number or NULL.
 * @defport: Default port number or 0 for none (default).
 * Returns: IP address or %INVALID on error.
 * Example:
 *   PTP::Net::Port port;
 *   PTP::Net::Ip ip = PTP::Net::Lookup("http://www.linux.org", &port, 8080);
 *   if (ip == PTP::Net::INVALID)
 *      return -1;
 *   ...
 */
PTP::Net::Ip
PTP::Net::Lookup(const char *url, Port *port, Port defport)
{
	Port p = defport;
	if (STRNCMP_CONST(url, "http://") == 0)
	{
		url += 7;
		if (!p)
			p = 80;
	}

	struct hostent *entry = NULL;
	const char *sep = strchr(url, ':');
	if (sep)
		p = (Port) strtoul(sep + 1, NULL, 10);
	else
		sep = strchr(url, '/');

	if (port)
		*port = p;

	char *name = NULL;
	if (sep)
	{
		int size = sep - url;
		name = new char[size + 1];
		memcpy(name, url, size);
		name[size] = '\0';
		url = name;
	}

	char host[MAX_HOSTNAME];
	if (strcmp(url, "localhost") == 0)
	{
		if (gethostname(host, sizeof(host)) == 0)
			url = host;
	}
	entry = gethostbyname(url);
	delete [] name;

	if (!entry)
		return 0;

	for (int i = 0; entry->h_addr_list[i]; i++)
	{
		Ip ip = ntohl(*(Ip*) entry->h_addr_list[i]);
		if (ip)
			return ip;
	}
	
	return 0;
}

/*
 * PTP::Net::Get32: Retrieve a 32-bit value in network byte order.
 * @src: Value pointer.
 * Returns: Value in native byte order.
 */
UINT32
PTP::Net::Get32(const void *src)
{
	const BYTE *s = (const BYTE*) src;
	return ((((UINT32) s[0]) << 24)
		| (((UINT32) s[1]) << 16)
		| (((UINT32) s[2]) << 8)
		| (UINT32) s[3]);
}

/*
 * PTP::Net::Get16: Retrieve a 16-bit value in network byte order.
 * @src: Value pointer.
 * Returns: Value in native byte order.
 */
UINT16
PTP::Net::Get16(const void *src)
{
	const BYTE *s = (const BYTE*) src;
	return ((((UINT16) s[0]) << 8) | (UINT16) s[1]);
}

/*
 * PTP::Net::Set32: Store a 32-bit value in network byte order.
 * @dst: [$OUT] Destination (network byte order).
 * @src: Source value (native byte order).
 */
void
PTP::Net::Set32(void *dst, UINT32 src)
{
	BYTE *d = (BYTE*) dst;
	d[0] = (src >> 24) & 0xff;
	d[1] = (src >> 16) & 0xff;
	d[2] = (src >> 8) & 0xff;
	d[3] = (src & 0xff);
}

/*
 * PTP::Net::Set16: Store a 16-bit value in network byte order.
 * @dst: [$OUT] Destination (network byte order).
 * @src: Source value (native byte order).
 */
void
PTP::Net::Set16(void *dst, UINT16 src)
{
	BYTE *d = (BYTE*) dst;
	d[0] = (src >> 8) & 0xff;
	d[1] = (src & 0xff);
}

/**
 * PTP::Net::Connection::Connection: Create an outbound (client) connection.
 * @type: Connection type (%RAW or %HTTP).
 * @ip: Destination IP address.
 * @port: Destination TCP port.
 * Example:
 *   PTP::Net::Connection c(PTP::Net::Connection::HTTP,
 *                          PTP::Net::Lookup("http://www.linux.org"),
 *                          80);
 */
PTP::Net::Connection::Connection(Type type, Net::Ip ip, Net::Port port)
	:PTP::List::Entry(),
	 m_type(type), m_dir(OUTBOUND),
	 m_ip(ip), m_port(port),
	 m_s(-1), m_close(1), m_proxy(UNKNOWN),
	 m_unget(NULL), m_ungetLast(NULL), m_ungetNext(NULL)
{
}

/**
 * PTP::Net::Connection::Connection: Create an inbound (server) connection.
 * @type: Connection type (%RAW or %HTTP).
 * @port: Local TCP port or %ANY.
 * Example:
 *   PTP::Net::Connection c(PTP::Net::Connection::HTTP, 8080);
 */
PTP::Net::Connection::Connection(Type type, Net::Port port)
	:PTP::List::Entry(),
         m_type(type), m_dir(INBOUND),
	 m_ip(0), m_port(port),
	 m_s(-1), m_close(1), m_proxy(UNKNOWN),
	 m_unget(NULL), m_ungetLast(NULL), m_ungetNext(NULL)
{
}

/**
 * PTP::Net::Connection::Connection: Create a connection from an open socket.
 * @type: Connection type (%RAW or %HTTP).
 * @dir: Connection direction (%INBOUND or %OUTBOUND).
 * @s: Socket.
 * @close: 1 to close socket on destruction or 0 to leave socket open.
 * Example:
 *   int s = socket(...);
 *   connect(s, ...);
 *   PTP::Net::Connection c(PTP::Net::Connection::HTTP,
 *                          PTP::Net::Connection::OUTBOUND,
 *                          s,
 *                          1);
 */
PTP::Net::Connection::Connection(Type type, Dir dir, int s, int close)
	:PTP::List::Entry(),
         m_type(type), m_dir(dir),
	 m_ip(0), m_port(0),
	 m_s(s), m_close(close), m_proxy(UNKNOWN),
	 m_unget(NULL), m_ungetLast(NULL), m_ungetNext(NULL)
{
}

#ifdef PTPTL_DLL

/*
 * PTP::Net::Connection::Connection: Copy constructor.
 * @conn: Source Connection.
 */
PTP::Net::Connection::Connection(const Connection& conn)
{
	assert(0);
}

/*
 * PTP::Net::Connection::operator=: Copy constructor.
 * @conn: Source Connection.
 */
PTP::Net::Connection&
PTP::Net::Connection::operator=(const Connection& conn)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::Net::Connection::~Connection: Close and destroy connection.
 */
PTP::Net::Connection::~Connection()
{
	if (!this)
		return;
	m_mutex.Lock();
	if (m_unget)
	{
		delete [] m_unget;
		m_unget = NULL;
	}
	m_mutex.Unlock();
	Close();
}

/**
 * PTP::Net::Connection::GetType
 * Returns: Connection type (%RAW or %HTTP).
 */
PTP::Net::Connection::Type
PTP::Net::Connection::GetType() const
{
	return m_type;
}

/**
 * PTP::Net::Connection::GetDir
 * Returns: Connection direction (%INBOUND or %OUTBOUND).
 */
PTP::Net::Connection::Dir
PTP::Net::Connection::GetDir() const
{
	return m_dir;
}

/**
 * PTP::Net::Connection::GetIp
 * Returns: Connected IP address or %INVALID if none.
 */
PTP::Net::Ip
PTP::Net::Connection::GetIp() const
{
	return m_ip;
}

/**
 * PTP::Net::Connection::GetPort
 * Returns: Connected TCP port or %INVALID if none.
 */
PTP::Net::Port
PTP::Net::Connection::GetPort() const
{
	return m_port;
}

/**
 * PTP::Net::Connection::GetSocket
 * Returns: Socket or -1 if none.
 */
int
PTP::Net::Connection::GetSocket() const
{
	return m_s;
}

/**
 * PTP::Net::Connection::Open: Open the connection.
 * @timeout: Milliseconds to wait for connect (0 for infinite timeout).
 * Returns: 0 on success (or already open) or -1 on error.
 * Notes: &Open is called automatically if the connection is
 *        currently closed and &Accept, &ReadAll, &WriteAll, ...
 *        are called.  A direct call to &Open can be useful for
 *        verifying that the connection to the remote client will
 *        succeed.
 * Example:
 *   PTP::Net::Connection c(PTP::Net::Connection::HTTP,
 *                          PTP::Net::Lookup("http://www.linux.org"),
 *                          80);
 *   if (c.$Open() < 0)
 *     printf("Can not connect to http://www.linux.org\n");
 */
int
PTP::Net::Connection::Open(int timeout)
{
	if (m_s >= 0)
		return 0;

	int s = socket(AF_INET, SOCK_STREAM, PROTOCOL);
	if (s < 0)
		return -1;

	int port = m_port;

	switch (m_dir)
	{
	case OUTBOUND:
		if (!m_ip || m_port <= 0)
		{
			Close(s);
			return -1;
		}

		if (!s_proxyIp)
			m_proxy = NO_PROXY;
		switch (m_proxy)
		{
		case NO_PROXY:
			if (Connect(s, m_ip, m_port, timeout))
			{
				Close(s);
				return -1;
			}
			break;
		case PROXY:
			if (Connect(s, s_proxyIp, s_proxyPort, timeout))
			{
				Close(s);
				return -1;
			}
			break;
		case UNKNOWN:
			if (s_proxyTimeout
			    && Connect(s,
				       m_ip,
				       m_port,
				       timeout ? timeout:s_proxyTimeout) == 0)
			{
				m_proxy = NO_PROXY;
			}
			else
			{
				if (s_proxyTimeout)
				{
					Close(s);
					s = socket(AF_INET,
						   SOCK_STREAM,
						   PROTOCOL);
				}
				if (s >= 0
				    && Connect(s,
					       s_proxyIp,
					       s_proxyPort,
					       timeout) == 0)
				{
					m_proxy = PROXY;
				}
				else
				{
					Close(s);
					return -1;
				}
			}
			break;
		}
		break;

	case INBOUND:
		for (;;)
		{
			sockaddr_in addr;
			socklen_t addrSize = sizeof(addr);

			int active = 0;
			if (port)
			{
				int test = socket(AF_INET,
						  SOCK_STREAM,
						  PROTOCOL);
				if (test < 0)
				{
					Close(s);
					return -1;
				}
				
				active = (Connect(test,
						  Net::LOOPBACK,
						  port,
						  1) == 0);
				Close(test);
			}
			
			if (!active)
			{
				int opt = 1;
				setsockopt(s,
					   SOL_SOCKET,
					   SO_REUSEADDR,
					   (char*) &opt,
					   sizeof(opt));
				
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = htonl(Net::ANY);
				addr.sin_port = htons(port);
				
				if (bind(s,
					 (struct sockaddr*) &addr,
					 sizeof(addr)) == 0
				    && getsockname(s,
						   (struct sockaddr*) &addr,
						   &addrSize) == 0)
				{
					port = ntohs(addr.sin_port);
					break;
				}
			}
			
			port++;
		}
		
		if (listen(s, LISTEN_BACKLOG) < 0)
		{
			Close(s);
			return -1;
		}
		break;
	}

	m_s = s;
	m_port = port;

	return 0;
}

/**
 * PTP::Net::Connection::Close: Close the connection.
 */
void
PTP::Net::Connection::Close()
{
	if (m_s >= 0 && m_close)
	{
		Close(m_s);
		m_s = -1;
	}
}

/**
 * PTP::Net::Connection::Accept: Accept a new (inbound) connection.
 * @detType: 1 to determine connection type.
 * Returns: Inbound (client) connection or NULL on error.
 * Example:
 *   PTP::Net::Connection server(PTP::Net::Connection::HTTP, 8080);
 *   PTP::Net::Connection *client = server.$Accept();
 *   if (client)
 *   {
 *     printf("Connection from %lu.%lu.%lu.%lu:%u\n",
 *            (client->GetIp() >> 24) & 0xff,
 *            (client->GetIp() >> 16) & 0xff,
 *            (client->GetIp() >> 8) & 0xff,
 *            (client->GetIp() & 0xff),
 *            client->GetPort());
 *     delete client;
 *   }
 */
PTP::Net::Connection *
PTP::Net::Connection::Accept(int detType)
{
	if (m_dir != INBOUND || Open() < 0)
		return NULL;

	struct sockaddr_in addr;
	socklen_t addrSize = sizeof(addr);
	int s = accept(m_s, (struct sockaddr*) &addr, &addrSize);
	if (s < 0)
		return NULL;

	Connection *c = new Connection(m_type, INBOUND, s, 1);
	if (c)
	{
		c->m_ip = ntohl(addr.sin_addr.s_addr);
		c->m_port = ntohs(addr.sin_port);
	}

	if (detType)
	{
		char buffer[64];
		int size = c->Read((BYTE*) buffer, sizeof(buffer));
		if (size <= 0)
		{
			delete c;
			return NULL;
		}
		c->Unget((BYTE*) buffer, size);
		
		char tunnel[] = "PUT /" PTP_NET_TUNNEL_PREFIX " HTTP/1.0\r\n";
		if (STRNCMP_CONST(buffer, tunnel) == 0)
		{
			c->m_type = RAW;
		}
		else if (STRNCMP_CONST(buffer, "GET ") == 0
			 || STRNCMP_CONST(buffer, "PUT ") == 0
			 || STRNCMP_CONST(buffer, "POST ") == 0)
		{
			c->m_type = HTTP;
		}
		else
		{
			c->m_type = RAW;
		}
	}

	return c;
}

/**
 * PTP::Net::Connection::ReadHttpHdr: Receive an HTTP header.
 * @hdr: [$OUT] Header data or NULL to allocate.
 * @maxsize: Maximum header size or -1 for unlimited.
 * @status: [$OUT] HTTP status code or NULL.
 * @contentsize: [$OUT] Content size or NULL.
 * @timeout: Milliseconds to wait for data or 0 for infinite.
 * Returns: Header data on success or NULL on error.
 * Example:
 *   PTP::Net::Connection *c = ...;
 *   int status;
 *   char *buffer = c->$ReadHttpHdr(NULL, 1024, &status, NULL);
 *   if (!buffer)
 *     return -1;
 */
char *
PTP::Net::Connection::ReadHttpHdr(
	char *hdr,
	int maxsize,
	int *status,
	int *contentsize,
	int timeout)
{
	if (maxsize < -1 || (hdr && maxsize < 0) || Open() < 0)
		return NULL;

	int total = (maxsize >= 0) ? maxsize:512;
	char *h = hdr;
	if (!h)
		h = new char[total + 1];

	int size = 0;
	for (;;)
	{
		int remain = total - size - 1;
		if (remain <= 0)
		{
			if (maxsize >= 0)
				break;
			else
			{
				char *old = h;
				total *= 2;
				h = new char[total];
				memcpy(h, old, size);
				delete [] old;
				continue;
			}
		}

		int s = Read((BYTE*) h + size, remain, timeout);
		if (s < 0)
		{
			size = -1;
			break;
		}
		size += s;

		h[size] = '\0';
		int unsize = UngetData((BYTE*) h, size);
		if (unsize >= 0)
		{
			size -= unsize;
			break;
		}
		else if (!s)
			break;
	}

	if (size <= 0)
	{
		if (!hdr)
			delete [] h;
		return NULL;
	}

	if (status)
	{
		if (STRNCMP_CONST(h, "HTTP") != 0)
			*status = HTTP_BAD_REQUEST;
		else
		{
			const char *st = h + strcspn(h, " ");
			st += strspn(st, " ");
			*status = (isdigit(*st)
				   ? strtoul(st, NULL, 10)
				   :HTTP_BAD_REQUEST);
		}
	}

	if (contentsize)
	{
		const char *len = strstr(h, "\nContent-length:");
		if (!len)
			len = strstr(h, "\nContent-Length:");
		*contentsize = len ? ((int) strtoul(len + 16, NULL, 10)):-1;
	}

	return h;
}
      
/**
 * PTP::Net::Connection::ReadHttp: Receive an HTTP header and data.
 * @data: [$OUT] Data buffer or NULL.
 * @maxsize: Maximum content data size or -1 for unlimited.
 * @status: [$OUT] HTTP status code or NULL.
 * @contentsize: [$OUT] Content size or NULL.
 * @timeout: Milliseconds to wait for data or 0 for infinite.
 * Returns: Content data on success or NULL on error.
 * Example:
 *   PTP::Net::Connection *c = ...;
 *   int size;
 *   char *buffer = c->$ReadHttp(NULL, 4096, NULL, &size);
 *   if (!buffer)
 *     return -1;
 */
BYTE *
PTP::Net::Connection::ReadHttp(
	BYTE *data,
	int maxsize,
	int *status,
	int *contentsize,
	int timeout)
{
	if (maxsize < -1 || (data && maxsize < 0) || Open() < 0)
		return NULL;
	
	int size = 0;
	char *hdr = ReadHttpHdr(NULL, maxsize, status, &size, timeout);
	delete [] hdr;
	if (!hdr || (maxsize >= 0 && size > maxsize))
	{
		delete [] hdr;
		return NULL;
	}

	int total = (size >= 0) ? (size + 1):1024;
	BYTE *d = data;
	if (!d)
		d = new BYTE[total];
	if (maxsize < 0 && size > 0)
		maxsize = size;

	size = 0;
	for (;;)
	{
		int remain = total - size - 1;
		int s = ReadAll((BYTE*) d + size, remain, timeout);
		size += s;
		if (s < 0)
		{
			size = -1;
			break;
		}
		else if (s < remain || maxsize >= 0)
			break;

		BYTE *old = d;
		total *= 2;
		d = new BYTE[total];
		memcpy(d, old, size);
		delete [] old;
	}

	if (contentsize)
		*contentsize = size;

	return d;
}

/**
 * PTP::Net::Connection::WriteHttp: Send an HTTP header and data.
 * @method: HTTP method (eg. "GET", "PUT", ...).
 * @url: Destination HTTP URL (eg. "/index.html").
 * @hdr: Extra HTTP header information or NULL.
 * @data: Content data or NULL.
 * @type: Content type or NULL for "application/binary".
 * @size: Content size or -1 if @data is a string.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Net::Connection *c = ...;
 *   c->$WriteHttp("GET",
 *                "/index.html",
 *                "Connection: Keep-Alive\r\n",
 *                NULL,
 *                NULL,
 *                0);
 */
int
PTP::Net::Connection::WriteHttp(
	const char *method,
	const char *url,
	const char *hdr,
	const BYTE *data,
	const char *type,
	int size)
{
	if (Open() < 0)
		return -1;

	if (!method)
		method = "PUT";
	if (!url)
		url = "/" PTP_NET_TUNNEL_PREFIX;
	if (!hdr)
		hdr = "";
	if (!type)
		type = "application/binary";
	if (size == -1 && data)
		size = strlen((const char*) data);
	char *buffer = new char[(strlen(url)
				 + strlen(hdr)
				 + strlen(type)
				 + 256)];

	if (m_proxy != PROXY)
		sprintf(buffer, "%s %s HTTP/1.0\r\n%s", method, url, hdr);
	else
	{
		sprintf(buffer,
			"%s http://%lu.%lu.%lu.%lu:%u%s HTTP/1.0\r\n%s",
			method,
			(m_ip >> 24) & 0xff,
			(m_ip >> 16) & 0xff,
			(m_ip >> 8) & 0xff,
			(m_ip & 0xff),
			m_port,
			url,
			hdr);
	}
	if (size)
	{
		sprintf(buffer + strlen(buffer),
			"Content-type: %s\r\n"
			"Content-length: %d\r\n",
			type,
			size);
	}
	strcat(buffer, "\r\n");

	int hsize = strlen(buffer);
	int stat = (WriteAll((BYTE*) buffer, hsize) == hsize
		    && (!data || WriteAll(data, size) == size)) ? 0:-1;
	delete [] buffer;
	return stat;
}

/**
 * PTP::Net::Connection::WriteHttp: Send an HTTP header and data.
 * @status: HTTP status code ($HTTP_OK, $HTTP_BAD_REQUEST, ...).
 * @hdr: Extra HTTP header information or NULL.
 * @data: Content data or NULL.
 * @type: Content type or NULL for "application/binary".
 * @size: Content size or -1 if @data is a string.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Net::Connection *c = ...;
 *   c->$WriteHttp(PTP::Net::HTTP_OK,
 *                "Connection: Close\r\n",
 *                (const BYTE*) "<HTML></HTML>",
 *                "text/html",
 *                -1);
 */
int
PTP::Net::Connection::WriteHttp(
	int status,
	const char *hdr,
	const BYTE *data,
	const char *type,
	int size)
{
	if (Open() < 0)
		return -1;

	if (!hdr)
		hdr = "";
	if (!type)
		type = "application/binary";
	if (size == -1 && data)
		size = strlen((const char*) data);
	char *buffer = new char[strlen(hdr) + strlen(type) + 256];

	const char *statusstr = "";
	switch (status)
	{
	case HTTP_OK: statusstr = " OK"; break;
	case HTTP_BAD_REQUEST: statusstr = " Bad request"; break;
	case HTTP_UNAUTHORIZED: statusstr = " Unauthorized"; break;
	case HTTP_NOT_FOUND: statusstr = " Not found"; break;
	}
	sprintf(buffer, "HTTP %d%s\r\n%s", status, statusstr, hdr);
	if (size)
	{
		sprintf(buffer + strlen(buffer),
			"Content-type: %s\r\n"
			"Content-length: %d\r\n",
			type,
			size);
	}
	strcat(buffer, "\r\n");

	int hsize = strlen(buffer);
	int stat = (WriteAll((BYTE*) buffer, hsize) == hsize
		    && (!data || WriteAll(data, size) == size))	? 0:-1;
	delete [] buffer;
	return stat;
}

/**
 * PTP::Net::Connection::ReadAll: Read data until entire buffer is full.
 * @data: [$OUT] Data buffer.
 * @size: Buffer size.
 * @timeout: Milliseconds to wait for data or 0 for infinite.
 * Returns: Number of bytes read or -1 on error.
 */
int
PTP::Net::Connection::ReadAll(BYTE *data, int size, int timeout)
{
	if (Open() < 0)
		return -1;

	int readsize = 0;
	while (readsize < size)
	{
		int s = Read(data + readsize, size - readsize, timeout);
		if (s < 0)
			return -1;
		else if (s == 0)
			break;
		readsize += s;
	}
	return readsize;
}

/**
 * PTP::Net::Connection::WriteAll: Write data until entire buffer is sent.
 * @data: Data buffer.
 * @size: Buffer size.
 * Returns: Number of bytes written or -1 on error.
 */
int
PTP::Net::Connection::WriteAll(const BYTE *data, int size)
{
	if (Open() < 0)
		return -1;
	if (size <= 0)
		return size;

	int writesize = 0;
	while (writesize < size)
	{
#ifdef WIN32
		int s = send(m_s,
			     (const char*)(data + writesize),
			     size - writesize,
			     0);
#else
		int s = write(m_s,
			      (const char*)(data + writesize),
			      size - writesize);
#endif
		if (s < 0)
			return -1;
		else if (s == 0)
			break;
		writesize += s;
	}
	return writesize;
}

/**
 * PTP::Net::Connection::Read: Read data from connection.
 * @data: [$OUT] Data buffer.
 * @size: Buffer size.
 * @timeout: Milliseconds to wait for data (0 for infinite timeout).
 * Returns: Number of bytes read or -1 on error.
 */
int
PTP::Net::Connection::Read(BYTE *data, int size, int timeout)
{
	if (Open() < 0)
		return -1;
	if (size <= 0)
		return size;

	int readsize = Get(data, size);
	if (!readsize)
	{
		if (timeout)
			Wait(m_s, 0, timeout);
#ifdef WIN32
		readsize = recv(m_s, (char*) data, size, 0);
#else
		readsize = read(m_s, (char*) data, size);
#endif
	}
	return readsize;
}

/**
 * PTP::Net::Connection::Unget: Return data to read queue.
 * @data: Data buffer.
 * @size: Buffer size.
 * Returns: 0 on success or -1 on failure.
 */
int
PTP::Net::Connection::Unget(const BYTE *data, int size)
{
	m_mutex.Lock();
	BYTE *old = m_unget;
	if (!old)
	{
		m_unget = new BYTE[size];
		memcpy(m_unget, data, size);
		m_ungetNext = m_unget;
		m_ungetLast = m_unget + size;
	}
	else
	{
		int oldsize = m_ungetLast - m_ungetNext;
		m_unget = new BYTE[size + oldsize];
		memcpy(m_unget, data, size);
		memcpy(m_unget + size, m_ungetNext, oldsize);
		delete [] old;
		m_ungetNext = m_unget;
		m_ungetLast = m_unget + size + oldsize;
	}
	m_mutex.Unlock();

	return 0;
}

/**
 * PTP::Net::Connection::SetProxy: Set proxy settings.
 * Type: static
 * @ip: Proxy IP address.
 * @port: Proxy TCP port.
 * @timeout: Milliseconds to wait for a direct connection
 *           (0 for proxy only).
 */
void
PTP::Net::Connection::SetProxy(Net::Ip ip, Net::Port port, int timeout)
{
	s_proxyIp = ip;
	s_proxyPort = port;
	s_proxyTimeout = timeout;
}

/*
 * PTP::Net::Connection::Get: Read data from read queue.
 * @data: [$OUT] Data buffer.
 * @size: Buffer size.
 * Returns: Number of bytes read.
 */
int
PTP::Net::Connection::Get(BYTE *data, int size)
{
	m_mutex.Lock();
	int readsize =  m_ungetLast - m_ungetNext;
	if (readsize)
	{
		if (readsize > size)
			readsize = size;
		
		memcpy(data, m_ungetNext, readsize);
		m_ungetNext += readsize;
		
		if (m_ungetNext >= m_ungetLast)
		{
			delete [] m_unget;
			m_unget = NULL;
			m_ungetNext = NULL;
			m_ungetLast = NULL;
		}
	}
	m_mutex.Unlock();

	return readsize;
}

/*
 * PTP::Net::Connection::UngetData: Save data read beyond the HTTP header.
 * @hdr: HTTP header.
 * @size: Header size.
 * Returns: Size of saved data or -1 if none.
 */
int
PTP::Net::Connection::UngetData(BYTE *hdr, int size)
{
	char *data = strstr((char*) hdr, "\r\n\r\n");
	if (data)
		data += 4;
	else
	{
		data = strstr((char*) hdr, "\n\n");
		if (data)
			data += 2;
		else
			return -1;
	}
	
	size -= (data - (char*) hdr);
	if (size > 0)
	{
		Unget((BYTE*) data, size);
		*data = '\0';
	}
	return size;
}

/*
 * PTP::Net::Connection::Connect: Connect to an IP address.
 * Type: static
 * @s: Socket.
 * @ip: Destination IP address.
 * @port: Destination TCP port.
 * @timeout: Milliseconds to wait for connect (or 0 for infinite timeout).
 * Returns: 0 on success or -1 on error.
 */
int
PTP::Net::Connection::Connect(int s,
			      PTP::Net::Ip ip,
			      PTP::Net::Port port,
			      int timeout)
{
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(ip);

	// set non-blocking mode
	if (timeout)
	{
#ifdef WIN32
		u_long val = 1;
		ioctlsocket(s, FIONBIO, &val);
#else
		fcntl(s, F_SETFL, O_NONBLOCK);
#endif
	}

	int status = connect(s, (struct sockaddr*) &addr, sizeof(addr));
	if (status)
		status = Wait(s, 1, timeout);

	// set blocking mode
	if (timeout)
	{
#ifdef WIN32
		u_long val = 0;
		ioctlsocket(s, FIONBIO, &val);
#else
		fcntl(s, F_SETFL, 0);
#endif
	}

	return status;
}

/*
 * PTP::Net::Connection::Wait: Wait for data.
 * @s: Socket.
 * @write: 1 to wait for write or 0 to wait for read.
 * @timeout: Milliseconds to wait.
 * Returns: 0 on success or -1 on timeout.
 */
int
PTP::Net::Connection::Wait(int s, int write, int timeout)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(s, &fds);
	
	struct timeval tmo;
	memset(&tmo, 0, sizeof(tmo));
	tmo.tv_sec = timeout / 1000;
	tmo.tv_usec = (timeout % 1000) * 1000;

	int status = 0;
	if (write)
		status = select(s + 1, NULL, &fds, NULL, &tmo);
	else
		status = select(s + 1, &fds, NULL, NULL, &tmo);

	if (status == 1)
	{
		socklen_t size = sizeof(status);
		getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &status, &size);
	}
	else
		status = -1;
	return ((status == 0) ? 0:-1);
}

/*
 * PTP::Net::Connection::Close: Close a socket.
 * Type: static
 * @s: Socket.
 */
void
PTP::Net::Connection::Close(int s)
{
	if(s >= 0)
	{
#ifdef WIN32
		closesocket(s);
#else
		close(s);
#endif
	}
}
