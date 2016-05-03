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

/*
 * trut: Trusted Gnutella client.
 */

#ifndef __TRUT_H__
#define __TRUT_H__

#define modulus modu

#include <ptp/ptp.h>
#include <ptp/list.h>
#include <ptp/key.h>
#include <ptp/auth.h>
#include <ptp/id.h>
#include <ptp/store.h>
#include <ptp/collect.h>
#include <ptp/net.h>
#include <ptp/thread.h>
#include "gnutella.h"

#undef modulus
#undef AddPort

/**
 * Trut: Trutella client.
 */
class Trut
{
public:
	class Port;
	class Host;
	class File;
	class Group;
	class Shared;

	enum GetStatus {GET_OK,	GET_DONE, GET_ERROR};
        enum JoinStatus {JOIN_OK, JOIN_CREATED, JOIN_ERROR};

	typedef void (*SearchCallback)(File *file, void *context);
	typedef void (*GetCallback)(File *file,
				    GetStatus status,
				    BYTE *data,
				    unsigned long size,
				    void *context);
	typedef void (*JoinCallback)(Group *group,
                                     JoinStatus status,
                                     void *context);
        typedef int (*AcceptCallback)(Group *group,
                                      const char *id,
                                      void *context);
	typedef void (*OpenCallback)(Host *host, void **context);
	typedef void (*CloseCallback)(Host *host, void *context);

	Trut(PTP::Store *store);
	~Trut();

	PTP::Net::Ip GetIp() const {return m_ip;}
	void SetOpenCallback(OpenCallback open) {m_open = open;}
	void SetCloseCallback(CloseCallback close) {m_close = close;}

        Port *AddPort(PTP::Net::Port port = 0);
        void RemovePort(Port *port);
        void RemoveAllPorts();

	Host *AddHost(const char *url);
	void RemoveHost(Host *host);
	void RemoveAllHosts();
	void GetHosts(void (*callback)(Host *host));

	void Search(const char *search,
		    SearchCallback callback,
		    void *context,
		    Group *group = NULL);
	void SearchStop(void *context = NULL);

	void Get(File *file,
		 const char *path,
		 GetCallback callback,
		 void *context);
	void GetWait(File *file);
	void GetStop(File *file);

        void JoinGroup(const char *name,
                       int wait,
                       JoinCallback join,
                       AcceptCallback accept,
                       void *context);
        Group *FindGroup(const char *name);
        void LeaveGroup(Group *group);
        void LeaveAllGroups();

	Shared *AddShared(const char *path, const char *ext, Group *group);
	void RemoveShared(Shared *shared);
	void UpdateShared(Shared *shared, const char *ext, Group *group);
	void RescanAllShared();
	void RemoveAllShared();

protected:
	enum
	{
		CHALLENGE_TIME = 10 * 60,
		READ_SIZE = 4096,
		PORT_DEFAULT = 6346,
		HEADER_SIZE = 1024,
	};
	
	struct SearchContext;
	struct TransferContext;
	struct GetContext;
	struct PutContext;

	int HandleSearchResponse(Host *host, Gnutella::SearchResp *resp);
	int HandleSearchRequest(Host *host, Gnutella::SearchRqst *rqst);
	int HandleConnect(Host *host, Gnutella::Packet *packet);

	int GetAuth(PTP::Net::Connection *conn, const BYTE *rqst);
	int PutAuth(PTP::Net::Connection *conn,
		    const PTP::Identity **id,
		    BYTE *resp);
	static int GetAuthValue(const char *hdr,
				const char *name,
				BYTE *value,
				int size);
	static void AppendAuthValue(char *hdr,
				    const char *name,
				    const BYTE *value,
				    int size);

	static void FindKey(File *file, void *context);
	static void FetchKey(File *file,
			     GetStatus status,
			     BYTE *data,
			     unsigned long size,
			     void *context);

	static void *PortThread(void *context);
	static void *HostThread(void *context);
	static void *PutThread(void *context);
	static void *GetThread(void *context);

	static int PutRead(BYTE *buffer, int size, void *context);
	static int PutWrite(const BYTE *buffer, int size, void *context);
	static int GetRead(BYTE *buffer, int size, void *context);
	static int GetWrite(const BYTE *buffer, int size, void *context);

	PTP::Net::Ip m_ip;
	PTP::Net::Port m_port;

	PTP::List m_ports;
	PTP::List m_hosts;
	PTP::List m_groups;
	PTP::List m_shared;

	PTP::Store *m_store;
	PTP::Identity *m_local;
	PTP::Authenticator *m_auth;

	PTP::Collection m_collect;
	PTP::List m_searches;
	PTP::List m_gets;

	OpenCallback m_open;
	CloseCallback m_close;
};

/**
 * Trut::Port: Trutella server port.
 */
class Trut::Port:protected PTP::List::Entry
{
public:
	PTP::Net::Ip GetPort() const {return m_conn->GetPort();}

protected:
	friend class Trut;

	Port():m_conn(NULL) {}
	~Port() {delete m_conn;}

	Trut *m_trut;
	PTP::Net::Connection *m_conn;
	PTP::Thread m_thread;
};

/**
 * Trut::Port: Trutella inbound or outbound connection.
 */
class Trut::Host:protected PTP::List::Entry
{
public:
	const char *GetUrl() const {return m_url;}
	PTP::Net::Ip GetIp() const {return m_conn->GetIp();}
	PTP::Net::Port GetPort() const {return m_conn->GetPort();}
	PTP::Net::Connection::Dir GetDirection() const
		{return m_conn->GetDir();}

protected:
	friend class Trut;

	Host():m_url(NULL), m_conn(NULL) {}
	~Host()
	{
		delete [] m_url;
		delete m_conn;
	}

	void SendGnutella(const BYTE *pkt);
	BYTE *ReceiveGnutella();

	Trut *m_trut;
	char *m_url;
	PTP::Net::Connection *m_conn;
	PTP::Thread m_thread;
	void *m_context;
};

/**
 * Trut::File: Trutella search result.
 */
class Trut::File:protected PTP::List::Entry
{
public:
	File():m_name(NULL) {}
	~File()	{delete [] m_name;}

	const char *GetName() const {return m_name;}
	PTP::Net::Ip GetIp() const {return m_ip;}
	PTP::Net::Port GetPort() const {return m_port;}
	int GetSpeed() const {return m_speed;}
	unsigned long GetSize() const {return m_size;}
	const Group *GetGroup() const {return m_group;}
	
protected:
	friend class Trut;

	Trut *m_trut;
	char *m_name;
	PTP::Net::Ip m_ip;
	PTP::Net::Port m_port;
	int m_speed;
	unsigned long m_size;
	Group *m_group;
	unsigned long m_ref;
};

/**
 * Trut::Group: Trutella secure group.
 */
class Trut::Group:protected PTP::List::Entry
{
public:
	const char *GetName() const {return m_name;}

protected:
	friend class Trut;

	Group():m_name(NULL), m_key(NULL) {}
	~Group()
	{
		delete m_key;
		delete [] m_name;
	}

	Trut *m_trut;
	char *m_name;

	int m_responses;

	Trut::JoinCallback m_join;
	Trut::AcceptCallback m_accept;
	void *m_context;

	PTP::Key *m_key;
};

/**
 * Trut::Shared: Trutella shared directory.
 */
class Trut::Shared:protected PTP::List::Entry
{
public:
	const char *GetPath() const {return m_path;}
	const char *GetExt() const {return m_ext;}
	const Group *GetGroup() const {return m_group;}
	
protected:
	friend class Trut;

	Shared():m_path(NULL), m_ext(NULL) {}
	~Shared()
	{
		delete m_path;
		delete m_ext;
	}

	char *m_path;
	char *m_ext;
	Group *m_group;
};

#endif // __TRUT_H__

