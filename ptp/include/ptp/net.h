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

#ifndef __PTP_NET_H__
#define __PTP_NET_H__

#include <sys/types.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <ptp/ptp.h>
#include <ptp/list.h>
#include <ptp/mutex.h>

/**
 * PTP::Net: Simple networking support.
 * Synopsis: #include <ptp/net.h>
 */
class PTP::Net
{
public:
	class Connection;

	/**
	 * PTP::Net::Ip: IP address.
	 */
	typedef unsigned long Ip;

	/**
	 * PTP::Net::Port: TCP port number.
	 */
	typedef unsigned short Port;

	enum
	{
		/**
		 * PTP::Net::LOOPBACK: Default local IP address.
		 */
		LOOPBACK = 0x7f000001,

		/**
		 * PTP::Net::ANY: Any IP address or port.
		 */
		ANY = 0,

		/**
		 * PTP::Net::INVALID: Invalid IP address or port.
		 */
		INVALID = 0
	};

	enum
	{
		HTTP_OK = 200,
		HTTP_BAD_REQUEST = 400,
		HTTP_UNAUTHORIZED = 401,
		HTTP_NOT_FOUND = 404
	};

	EXPORT static Ip Lookup(const char *url,
				Port *port = NULL,
				Port defport = 0);

	EXPORT static UINT32 Get32(const void *src);
	EXPORT static UINT16 Get16(const void *src);
	EXPORT static void Set32(void *dst, UINT32 src);
	EXPORT static void Set16(void *dst, UINT16 src);

protected:
	enum {MAX_HOSTNAME = 256};
};

/**
 * PTP::Net::Connection: Simple network connection
 */
class EXPORT PTP::Net::Connection:public PTP::List::Entry
{
public:
	enum Type
	{
		/**
		 * PTP::Net::Connection::RAW: Raw application data type.
		 */
		RAW = 1,

		/**
		 * PTP::Net::Connection::HTTP: HTTP data type.
		 */
		HTTP = 2
	};
	enum Dir
	{
		/**
		 * PTP::Net::Connection::OUTBOUND
		 *
		 * Connection originating from the local machine.
		 */
		OUTBOUND = 1,

		/**
		 * PTP::Net::Connection::INBOUND
		 *
		 * Connection destined to the local machine.
		 */
		INBOUND = 2
	};

	Connection(Type type, Net::Ip ip, Net::Port port);
	Connection(Type type, Net::Port port);
	Connection(Type type, Dir dir, int fd, int close);
	~Connection();

	Type GetType() const;
	Dir GetDir() const;
	Net::Ip GetIp() const;
	Net::Port GetPort() const;
	int GetSocket() const;

	int Open(int timeout = 0);
	void Close();
	Connection *Accept(int detType = 0);

	char *ReadHttpHdr(
		char *hdr = NULL,
		int maxsize = -1,
		int *status = NULL,
		int *contentsize = NULL,
		int timeout = 0);
	BYTE *ReadHttp(
		BYTE *data = NULL,
		int maxsize = -1,
		int *status = NULL,
		int *contentsize = NULL,
		int timeout = 0);
	int WriteHttp(
		const char *method,
		const char *url,
		const char *hdr = NULL,
		const BYTE *data = NULL,
		const char *type = NULL,
		int size = 0);
	int WriteHttp(
		int status,
		const char *hdr = NULL,
		const BYTE *data = NULL,
		const char *type = NULL,
		int size = 0);

	int ReadAll(BYTE *data, int size, int timeout = 0);
	int WriteAll(const BYTE *data, int size);

	int Read(BYTE *data, int size, int timeout = 0);
	int Unget(const BYTE *data, int size);

	static void SetProxy(Net::Ip ip,
			     Net::Port port,
			     int timeout = 1000);

protected:
	enum
	{
		PROTOCOL = 6,
		LISTEN_BACKLOG = 6,
		MAX_PROXY_HEADER = 256
	};

	enum ProxyState
	{
		NO_PROXY = 0,
		PROXY = 1,
		UNKNOWN = 2
	};

	Connection(const Connection& conn);
	Connection& operator=(const Connection& conn);

	int Get(BYTE *data, int size);
	int UngetData(BYTE *hdr, int size);

	static int Connect(int s,
			   PTP::Net::Ip ip,
			   PTP::Net::Port port,
			   int timeout);
	static int Wait(int s, int write, int timeout);
	static void Close(int s);

	Type m_type;
	Dir m_dir;
	Net::Ip m_ip;
	Net::Port m_port;
	
	int m_s;
	int m_close;
	ProxyState m_proxy;

	BYTE *m_unget;
	BYTE *m_ungetLast;
	BYTE *m_ungetNext;

	PTP::Mutex m_mutex;

	static Net::Ip s_proxyIp;
	static Net::Port s_proxyPort;
	static int s_proxyTimeout;
};

#endif // __PTP_NET_H__
