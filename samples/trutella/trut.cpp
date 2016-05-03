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

#include <stdio.h>
#include <string.h>
#include <ptp/rand.h>
#include <ptp/encode.h>
#include <ptp/debug.h>
#include "trut.h"

#define STRLEN_CONST(x) (sizeof(x) - 1)
#define STRNCMP_CONST(x, y) strncmp((const char*)(x), (y), sizeof(y) - 1)

/*
 * Trut::SearchContext: Pending search information.
 */
struct Trut::SearchContext:public PTP::List::Entry
{
	BYTE m_guid[Gnutella::GUID_SIZE];
	SearchCallback m_callback;
	void *m_context;
	Group *m_group;
};

/*
 * Trut::TransferContext: Pending file transfer.
 */
struct Trut::TransferContext
{
	TransferContext(FILE *f, PTP::Net::Connection *c = NULL)
		:fp(f), conn(c), callback(NULL), size(0) {}
	FILE *fp;
	PTP::Net::Connection *conn;
	Trut::GetCallback callback;
	Trut::File *file;
	void *context;
	unsigned long size;
};

/*
 * Trut::GetContext: Pending file download.
 */
struct Trut::GetContext:public PTP::List::Entry
{
	GetContext():m_conn(NULL), m_path(NULL) {}
	~GetContext()
	{
		delete [] m_path;
		delete m_conn;
	}

	Trut *m_trut;
	PTP::Key *m_key;
	const PTP::Identity *m_id;

	PTP::Net::Connection *m_conn;
	Trut::File *m_file;
	char *m_path;

	Trut::GetCallback m_callback;
	void *m_context;

	unsigned long m_size;

	PTP::Thread m_thread;
};

/*
 * Trut::PutContext: Pending file upload.
 */
struct Trut::PutContext
{
	Trut::Host *m_host;
	BYTE *m_resp;
	const PTP::Identity *m_id;
};

/*
 * Trut::Host::SendGnutella: Send a Gnutella packet.
 * @pkt: Gnutella packet.
 */
void
Trut::Host::SendGnutella(const BYTE *pkt)
{
        int size = 0;
        if (STRNCMP_CONST((char*) pkt, "GNUTELLA") != 0)
        {
                Gnutella::Packet *p = (Gnutella::Packet *) pkt;
                size = sizeof(*p) + Gnutella::Get32(p->size);
        }
        else
                size = strlen((char*) pkt);
        m_conn->WriteAll(pkt, size);
}

/*
 * Trut::Host::ReceiveGnutella: Receive a Gnutella packet.
 * Returns: Gnutella packet or NULL on error.
 */
BYTE *
Trut::Host::ReceiveGnutella()
{
        char buffer[512];
        int readsize = m_conn->Read((BYTE*) buffer, sizeof(buffer) - 1);
        if (readsize <= 0)
                return NULL;

        int size = 0;
        if (STRNCMP_CONST(buffer, GNUTELLA_CONNECT_REQUEST) != 0
            && STRNCMP_CONST(buffer, GNUTELLA_CONNECT_RESPONSE) != 0)
        {
                Gnutella::Packet *pkt = (Gnutella::Packet*) buffer;
                size = sizeof(*pkt) + Gnutella::Get32(pkt->size);
        }
        else
        {
                buffer[readsize] = '\0';
                size = readsize;
        }

	if (size < 0)
		return NULL;
	
        BYTE *pkt = new BYTE[size];
        memcpy(pkt, buffer, readsize);
        if (size > readsize
	    && m_conn->ReadAll(pkt + readsize, size - readsize) < 0)
        {
                delete [] pkt;
                pkt = NULL;
        }
        return pkt;
}

/**
 * Trut::Trut: Class constructor.
 * @store: Certificate store.
 */
Trut::Trut(PTP::Store *store)
	:m_store(store), m_open(NULL), m_close(NULL)
{
	m_ip = PTP::Net::Lookup("localhost");
	m_port = PORT_DEFAULT;
	m_local = m_store->Find(NULL, 1);
	m_auth = new PTP::Authenticator(m_store);
}

/**
 * Trut::~Trut: Class destructor.
 */
Trut::~Trut()
{
	delete m_auth;
	SearchStop();
	LeaveAllGroups();
	RemoveAllShared();
	RemoveAllPorts();
	RemoveAllHosts();
}

/**
 * Trut::AddPort: Create a new server port.
 * @port: TCP port number.
 * Returns: Port connection or NULL on error.
 */
Trut::Port *
Trut::AddPort(PTP::Net::Port port)
{
	Port *p = new Port;
	p->m_trut = this;
	p->m_conn = new PTP::Net::Connection(PTP::Net::Connection::RAW,
					     port ? port:PORT_DEFAULT);
	if (p->m_conn->Open())
	{
		delete p;
		return NULL;
	}

	m_port = (PTP::Net::Port) p->GetPort();
	m_ports.Insert(p);
	p->m_thread.Start(PortThread, p);

	return p;
}

/*
 * Trut::PortThread: Handle server connection.
 * @context: Port context.
 * Returns: NULL.
 */
void *
Trut::PortThread(void *context)
{
	Port *port = (Port*) context;
	for (;;)
	{
		PTP::Net::Connection *conn = port->m_conn->Accept(1);
		if (!conn)
			continue;

		char url[256];
		sprintf(url,
			"%lu.%lu.%lu.%lu:%u",
			(conn->GetIp() >> 24) & 0xff,
			(conn->GetIp() >> 16) & 0xff,
			(conn->GetIp() >> 8) & 0xff,
			(conn->GetIp() & 0xff),
			conn->GetPort());

		Host *host = new Host;
		host->m_trut = port->m_trut;
		host->m_conn = conn;
		host->m_url = strdup(url);
		
		if (host->m_conn->GetType() == PTP::Net::Connection::HTTP)
		{
			PutContext ctx;
			ctx.m_host = host;
			ctx.m_resp = new BYTE[HEADER_SIZE];
			ctx.m_id = NULL;

			if (host->m_trut->PutAuth(conn,
						  &ctx.m_id,
						  ctx.m_resp) == 0)
				host->m_thread.Start(PutThread, &ctx);
			else
			{
				delete [] ctx.m_resp;
				delete host;
			}
		}
		else
		{
			host->m_trut->m_hosts.Insert(host);
			if (host->m_trut->m_open)
				(*host->m_trut->m_open)(host,
							&host->m_context);
			host->m_thread.Start(HostThread, host);
		}
	}

	port->m_trut->m_ports.Remove(port);
	delete port;
	PTP::Thread::Exit(0);

	return NULL;
}

/*
 * Trut::PutThread: Handle file transmit.
 * @context: Transmit context.
 * Returns: NULL.
 */
void *
Trut::PutThread(void *context)
{
	PutContext *ctx = (PutContext*) context;

	Host *host = ctx->m_host;
	PTP::Collection::Entry *entry = NULL;
	PTP::Key *key = NULL;
	int size = 0;

	char *hdr = new char[HEADER_SIZE];
	if (!host->m_conn->ReadHttpHdr(hdr, HEADER_SIZE))
	{
		delete [] hdr;
		delete [] ctx->m_resp;
		delete host;
		PTP::Thread::Exit(0);
		return NULL;
	}

	if (STRNCMP_CONST(hdr, "GET /get/") == 0)
	{
		char *start = hdr + STRLEN_CONST("GET /get/");
		unsigned long ref = strtoul(start, NULL, 10);
		entry = host->m_trut->m_collect.Find(ref);
	}
	else if (STRNCMP_CONST(hdr, "GET /gets/") == 0)
	{
		char *start = hdr + STRLEN_CONST("GET /gets/");
		char *end = strchr(start, '/');
		*end = '\0';
		Group *group = host->m_trut->FindGroup(start);
		if (!group || !group->m_key)
			goto fail;
		key = group->m_key;
		start = end + 1;
		unsigned long ref = strtoul(start, NULL, 16);
		entry = host->m_trut->m_collect.Find(ref);

		start = (char*) entry->GetName();
		end = strrchr(start, '/');

		int iskey = (strncmp(start, "/secure/", 8) == 0
			     && end
			     && strcmp(end + 1, "key") == 0);
		if (iskey)
			key = NULL;

		if (iskey
		    && group->m_accept
		    && ctx->m_id
		    && (*group->m_accept)(group,
					  ctx->m_id->GetName(),
					  group->m_context) == 0)
			goto fail;
	}
	else
		goto fail;

	if (!entry)
		goto fail;

	// get data size
	size = entry->GetSize();
	if (ctx->m_id)
	{
		size = PTP::Identity::CIPHERTEXT_SIZE;
	}
	else if (key)
	{
		if (entry->GetPath())
		{
			TransferContext ctx(fopen(entry->GetPath(), "rb"));
			if (!ctx.fp)
				goto fail;
			size = key->Encrypt(PutRead, NULL, &ctx);
			fclose(ctx.fp);
		}
		else
		{
			size = key->Encrypt(entry->GetData(),
					    entry->GetSize(),
					    NULL);
		}
	}

	// send header
	sprintf(hdr,
		"HTTP 200\r\n"
		"Content-type:application/binary\r\n"
		"Content-length:%d\r\n"
		"%s\r\n",
		size,
		ctx->m_resp);
	host->m_conn->WriteAll((BYTE*) hdr, strlen(hdr));

	// send data
	if (entry->GetPath())
	{
		TransferContext ctx(fopen(entry->GetPath(), "rb"),
				    host->m_conn);
		if (!ctx.fp)
			goto fail;
		if (key)
			key->Encrypt(PutRead, PutWrite, &ctx);
		else
			PTP::Key::Transfer(PutRead, PutWrite, &ctx);
		fclose(ctx.fp);
	}
	else
	{
		if (ctx->m_id)
		{
			BYTE buffer[PTP::Identity::CIPHERTEXT_SIZE];
			ctx->m_id->Encrypt(entry->GetData(),
					   entry->GetSize(),
					   buffer);
			host->m_conn->WriteAll(buffer, sizeof(buffer));
		}
		else if (key)
		{
			BYTE *buffer = new BYTE[size];
			key->Encrypt(entry->GetData(),
				     entry->GetSize(),
				     buffer);
			host->m_conn->WriteAll(buffer, size);
			delete [] buffer;
		}
		else
		{
			host->m_conn->WriteAll(entry->GetData(),
					       entry->GetSize());
		}
	}

	delete [] hdr;
	delete [] ctx->m_resp;
	delete host;
	PTP::Thread::Exit(0);

	return NULL;

 fail:
	sprintf(hdr,
		"HTTP 404\r\n"
		"Content-type: text/html\r\n\r\n"
		"<B>404 NOT FOUND</B>\n");
	host->m_conn->WriteAll((BYTE*) hdr, strlen(hdr));

	delete [] hdr;
	delete [] ctx->m_resp;
	delete host;
	PTP::Thread::Exit(0);

	return NULL;
}

/**
 * Trut::RemovePort: Destroy a server port.
 * @port: Port connection.
 */
void
Trut::RemovePort(Port *port)
{
	m_ports.Remove(port);
	port->m_thread.Kill();
	delete port;
}

/**
 * Trut::RemoveAllPorts: Destroy all server ports.
 */
void
Trut::RemoveAllPorts()
{
	Port *port;
	m_ports.Lock();
	PTP_LIST_FOREACH(Port, port, &m_ports)
        {
		m_ports.Remove(port, 0);
		port->m_thread.Kill();
		delete port;
	}
	m_ports.Unlock();
}

/**
 * Trut::AddHost: Create a new Gnutella connection.
 * @url: Destination URL.
 * Returns: Client connection or NULL on error.
 */
Trut::Host *
Trut::AddHost(const char *url)
{
	PTP::Net::Port port;
	PTP::Net::Ip ip = PTP::Net::Lookup(url, &port, PORT_DEFAULT);
	if (!ip)
		return NULL;

	Host *host = new Host;
	host->m_trut = this;
	host->m_url = strdup(url);
	host->m_conn = new PTP::Net::Connection(PTP::Net::Connection::RAW,
						ip,
						port);
	host->SendGnutella((BYTE*) GNUTELLA_CONNECT_REQUEST);

	BYTE *pkt = host->ReceiveGnutella();
	int ok = (pkt && STRNCMP_CONST((char*) pkt,
				       GNUTELLA_CONNECT_RESPONSE) == 0);
	delete [] pkt;

	if (!ok)
	{
		delete host;
		return NULL;
	}

	m_hosts.Insert(host);
	if (m_open)
		(*m_open)(host, &host->m_context);

	host->m_thread.Start(HostThread, host);

	return host;
}

/*
 * Trut::HostThread: Handle client connection.
 * @context: Client context.
 * Returns: NULL.
 */
void *
Trut::HostThread(void *context)
{
	Host *host = (Host*) context;
	for (;;)
	{
		Gnutella::Packet *pkt
			= (Gnutella::Packet*) host->ReceiveGnutella();
		if (!pkt)
			break;
		
		if (STRNCMP_CONST((char*) pkt, GNUTELLA_CONNECT_REQUEST) == 0)
		{
			host->SendGnutella((BYTE*) GNUTELLA_CONNECT_RESPONSE);
		}
		else
		{
			switch (pkt->type)
			{
			case Gnutella::SEARCH_RESPONSE:
				host->m_trut->HandleSearchResponse(
					host,
					(Gnutella::SearchResp*) pkt);
				break;
			case Gnutella::SEARCH_REQUEST:
				host->m_trut->HandleSearchRequest(
					host,
					(Gnutella::SearchRqst*) pkt);
				break;
			default:
				break;
			}
		}

		delete [] ((BYTE*) pkt);
	}

	host->m_trut->m_hosts.Remove(host);
	if (host->m_trut->m_close)
		(*host->m_trut->m_close)(host, host->m_context);
	delete host;
	PTP::Thread::Exit(0);

	return NULL;
}

/**
 * Trut::RemoveHost: Destroy a client connection.
 * @host: Client connection.
 */
void
Trut::RemoveHost(Host *host)
{
	m_hosts.Remove(host);
	host->m_thread.Kill();
	if (m_close)
		(*m_close)(host, host->m_context);
	delete host;
}

/**
 * Trut::RemoveAllHosts: Destroy all client connections.
 */
void
Trut::RemoveAllHosts()
{
	Host *host;
	m_hosts.Lock();
	PTP_LIST_FOREACH(Host, host, &m_hosts)
	{
		m_hosts.Remove(host, 0);
		host->m_thread.Kill();
		if (m_close)
			(*m_close)(host, host->m_context);
		delete host;
	}
	m_hosts.Unlock();
}

/**
 * Trut::GetHosts: Enumerate clients connections.
 * @callback: Connection callback.
 */
void
Trut::GetHosts(void (*callback)(Host *host))
{
	if (!callback)
		return;
	Host *host;
	m_hosts.Lock();
	PTP_LIST_FOREACH(Host, host, &m_hosts)
	{
		(*callback)(host);
	}
	m_hosts.Unlock();
}

/**
 * Trut::Search: Send a search request.
 * @str: Search string.
 * @callback: Search response callback.
 * @context: Callback context.
 * @group: Search group.
 */
void
Trut::Search(const char *str,
	     SearchCallback callback,
	     void *context,
	     Group *group)
{
	if (!str || !callback)
		return;

	SearchContext *entry = new SearchContext;
	memset(entry, 0, sizeof(*entry));
	PTP::Random::Fill(entry->m_guid, sizeof(entry->m_guid));
	entry->m_callback = callback;
	entry->m_context = context;
	entry->m_group = group;
	m_searches.Insert(entry);

	BYTE *cipher = NULL;
	if (entry->m_group && entry->m_group->m_key)
	{
		int hdrsize = (STRLEN_CONST("/secure/")
			       + strlen(entry->m_group->GetName())
			       + 1);
		int encsize = entry->m_group->m_key->Encrypt(
			NULL,
			strlen(str),
			NULL);
		cipher = new BYTE[hdrsize + (encsize * 2) + 4];

		sprintf((char*) cipher,
			"/secure/%s/",
			entry->m_group->GetName());
		entry->m_group->m_key->Encrypt(
			(const BYTE*) str,
			strlen(str),
			cipher + hdrsize);

		PTP::Encoding::EncodeBase64(
			cipher + hdrsize,
			encsize,
			(char*)(cipher + hdrsize),
			encsize);
		str = (const char*) cipher;
	}

	int size = sizeof(Gnutella::SearchRqst) + strlen(str) + 1;
	BYTE *buffer = new BYTE[size];
	memset(buffer, 0, size);

	Gnutella::SearchRqst *rqst = (Gnutella::SearchRqst*) buffer;
	memcpy(rqst->guid, entry->m_guid, sizeof(rqst->guid));
	rqst->type = Gnutella::SEARCH_REQUEST;
	rqst->ttl = Gnutella::DEFAULT_TTL;
	rqst->hops = 0;
	Gnutella::Set32(rqst->size, size - sizeof(Gnutella::Packet));
	Gnutella::Set16(rqst->speed, 0);
	strcpy(rqst->search, str);

	delete [] cipher;

	Host *host;
	m_hosts.Lock();
	PTP_LIST_FOREACH(Host, host, &m_hosts)
	{
		host->SendGnutella(buffer);
	}
	m_hosts.Unlock();

	delete [] buffer;
}

/**
 * Trut::SearchStop: Terminate a search request.
 * @context: Search callback context.
 */
void
Trut::SearchStop(void *context)
{
	SearchContext *search;
	m_searches.Lock();
	PTP_LIST_FOREACH(SearchContext, search, &m_searches)
	{
		if (context && search->m_context != context)
			continue;
		m_searches.Remove(search, 0);
		delete search;
	}
	m_searches.Unlock();
}

/*
 * Trut::GetAuth: Handle client-side authentication.
 * @conn: Client connection.
 * @rqst: HTTP request.
 * Returns: 0 on success or -1 on failed authentication.
 */
int
Trut::GetAuth(PTP::Net::Connection *conn, const BYTE *rqst)
{
	int secure = (STRNCMP_CONST(rqst, "GET /gets/") == 0);
	char *hdr = new char[HEADER_SIZE];

	strcpy(hdr, (char*) rqst);
	BYTE key[PTP::Identity::KEY_SIZE];
	m_local->GetKey(key);
	if (secure)
		AppendAuthValue(hdr, "Identity", key, sizeof(key));
	strcat(hdr, "\r\n");
	conn->WriteAll((BYTE*) hdr, strlen(hdr));

	if (!conn->ReadHttpHdr(hdr, HEADER_SIZE))
	{
		delete [] hdr;
		return -1;
	}

	if (!secure)
	{
		conn->Unget((BYTE*) hdr, strlen(hdr));
		delete [] hdr;
		return 0;
	}

	conn->Close();

	BYTE rkey[PTP::Identity::KEY_SIZE];
	BYTE rchal[PTP::Authenticator::CHALLENGE_SIZE];
	if (GetAuthValue(hdr, "Identity", rkey, sizeof(rkey))
	    || GetAuthValue(hdr, "Challenge", rchal, sizeof(rchal)))
	{
		delete [] hdr;
		return -1;
	}
	const PTP::Identity *rid = m_store->Find(NULL, 0, rkey);
	if (!rid)
	{
		delete [] hdr;
		return -1;
	}

	strcpy(hdr, (char*) rqst);
	BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
	m_auth->Challenge(rid, CHALLENGE_TIME, (void*) rid, chal);
	AppendAuthValue(hdr, "Challenge", chal, sizeof(chal));
	BYTE resp[PTP::Authenticator::RESPONSE_SIZE];
	m_auth->Respond(rchal, resp);
	AppendAuthValue(hdr, "Response", resp, sizeof(resp));
	strcat(hdr, "\r\n");
	conn->WriteAll((BYTE*) hdr, strlen(hdr));

	if (!conn->ReadHttpHdr(hdr, HEADER_SIZE))
	{
		delete [] hdr;
		return -1;
	}
	
	BYTE rresp[PTP::Authenticator::RESPONSE_SIZE];
	if (GetAuthValue(hdr, "Response", rresp, sizeof(rresp))
	    || m_auth->Verify(rresp) == NULL)
	{
		delete [] hdr;
		return -1;
	}

	conn->Unget((BYTE*) hdr, strlen(hdr));
	delete [] hdr;

	return 0;
}

/*
 * Trut::PutAuth: Handle server-side authentication.
 * @conn: Client connection.
 * @id: [!OUT] Client identity.
 * @rsp: [!OUT] Authentication response.
 * Returns: 0 on success or -1 on failed or partial authentication.
 */
int
Trut::PutAuth(PTP::Net::Connection *conn, const PTP::Identity **id, BYTE *rsp)
{
	const char *rqst = "HTTP/1.1 401 Unauthorized\r\n"
		           "Connection: Keep-Alive\r\n";

	rsp[0] = '\0';

	char *hdr = new char[HEADER_SIZE];
	if (!conn->ReadHttpHdr(hdr, HEADER_SIZE))
	{
		delete [] hdr;
		return -1;
	}

	int secure = (STRNCMP_CONST(hdr, "GET /gets/") == 0);
	if (!secure)
	{
		conn->Unget((BYTE*) hdr, strlen(hdr));
		delete [] hdr;
		return 0;
	}

	BYTE rkey[PTP::Identity::KEY_SIZE];
	if (GetAuthValue(hdr, "Identity", rkey, sizeof(rkey)) == 0)
	{
		const PTP::Identity *rid = m_store->Find(NULL, 0, rkey);
		if (!rid)
		{
			delete [] hdr;
			return -1;
		}
		
		strcpy(hdr, rqst);
		BYTE key[PTP::Identity::KEY_SIZE];
		m_local->GetKey(key);
		AppendAuthValue(hdr, "Identity", key, sizeof(key));
		BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
		m_auth->Challenge(rid, CHALLENGE_TIME, (void*) rid, chal);
		AppendAuthValue(hdr, "Challenge", chal, sizeof(chal));
		strcat(hdr, "\r\n");
		conn->WriteAll((BYTE*) hdr, strlen(hdr));

		delete [] hdr;
		return -1;
	}

	BYTE rresp[PTP::Authenticator::RESPONSE_SIZE];
	BYTE rchal[PTP::Authenticator::CHALLENGE_SIZE];
	if (GetAuthValue(hdr, "Response", rresp, sizeof(rresp))
	    || GetAuthValue(hdr, "Challenge", rchal, sizeof(rchal)))
	{
		delete [] hdr;
		return -1;
	}
	
	const PTP::Identity *rid
		= (const PTP::Identity*) m_auth->Verify(rresp);
	if (!rid)
	{
		delete [] hdr;
		return -1;
	}
	conn->Unget((BYTE*) hdr, strlen(hdr));
	
	strcpy(hdr, rqst);
	BYTE resp[PTP::Authenticator::RESPONSE_SIZE];
	m_auth->Respond(rchal, resp);
	AppendAuthValue((char*) rsp, "Response", resp, sizeof(resp));
	*id = rid;
	
	delete [] hdr;

	return 0;
}

/*
 * Trut::GetAuthValue: Retrieve HTTP header value.
 * @hdr: HTTP header.
 * @name: Value name.
 * @value: [!OUT] Value data.
 * @size: Expected value size.
 * Returns: 0 on sucess or -1 on error.
 */
int
Trut::GetAuthValue(const char *hdr, const char *name, BYTE *value, int size)
{
	int namesize = strlen(name);
	char *start = (char*) hdr;
	for (;;)
	{
		start = strstr(start, name);
		if (!start)
			return -1;
		start += namesize;
		if (*start == ':')
			break;
	}

	start++;
	start += strspn(start, " \t");
	char *end = start + strspn(start,
				   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				   "abcdefghijklmnopqrstuvwxyz"
				   "0123456789+/");
	int valuesize = end - start;
	valuesize = PTP::Encoding::DecodeBase64(start, valuesize, value);

	return 0;
}

/*
 * Trut::AppendAuthValue: Add HTTP header value.
 * @hdr: [!OUT] HTTP header.
 * @name: Value name.
 * @value: Value data.
 * @size: Value size.
 */
void
Trut::AppendAuthValue(char *hdr, const char *name, const BYTE *value, int size)
{
	strcat(hdr, name);
	strcat(hdr, ": ");
	hdr += strlen(hdr);
	PTP::Encoding::EncodeBase64(value, size, hdr, size);
	strcat(hdr, "\r\n");
}

/**
 * Trut::Get: Retrieve file.
 * @file: File.
 * @path: Destination file name.
 * @callback: Get callback.
 * @context: Callback context.
 */
void
Trut::Get(File *file, const char *path, GetCallback callback, void *context)
{
	GetContext *get = new GetContext;
	get->m_trut = this;
	get->m_file = file;
	get->m_path = path ? strdup(path):NULL;
	get->m_callback = callback;
	get->m_context = context;

	get->m_conn = new PTP::Net::Connection(
		PTP::Net::Connection::HTTP,
		file->GetIp(),
		file->GetPort());

	const Group *group = file->GetGroup();
	get->m_key = group ? group->m_key:NULL;
	get->m_id = NULL;

	char *hdr = new char[HEADER_SIZE];
	if (group)
	{
		if (strcmp(file->GetName(), "key") == 0)
			get->m_id = m_local;
		sprintf(hdr,
			"GET /gets/%s/%04lx HTTP/1.0\r\n"
			"Connection: Keep-Alive\r\n",
			group->GetName(),
			file->m_ref);
	}
	else
	{
		sprintf(hdr,
			"GET /get/%lu/%s HTTP/1.0\r\n"
			"Connection: Keep-Alive\r\n",
			file->m_ref,
			file->GetName());
	}

	int status = 0;
	int size = 0;
	
	if (GetAuth(get->m_conn, (BYTE*) hdr)
	    || !get->m_conn->ReadHttpHdr(hdr, HEADER_SIZE, &status, &size)
	    || status != PTP::Net::HTTP_OK
	    || size <= 0)
	{
		if (get->m_callback)
		{
			(*get->m_callback)(get->m_file,
					   GET_ERROR,
					   NULL,
					   0,
					   get->m_context);
		}
		delete [] hdr;
		delete get;
		return;
	}

	get->m_size = size;
	delete [] hdr;
	m_gets.Insert(get);
	get->m_thread.Start(GetThread, get);
}

/**
 * Trut::GetThread: Handle file receive.
 * @context: Receive context.
 * Returns: NULL.
 */
void *
Trut::GetThread(void *context)
{
	GetContext *get = (GetContext*) context;

	int size = 0;
	BYTE *buffer = NULL;

	GetStatus status = GET_ERROR;
	if (get->m_path)
	{
		TransferContext ctx(fopen(get->m_path, "wb"), get->m_conn);
		ctx.callback = get->m_callback;
		ctx.file = get->m_file;
		ctx.context = get->m_context;
		ctx.size = 0;
		if (ctx.fp)
		{
			if (get->m_key)
			{
				size = get->m_key->Decrypt(GetRead,
							   GetWrite,
							   &ctx);
			}
			else
			{
				size = PTP::Key::Transfer(GetRead,
							  GetWrite,
							  &ctx);
			}
			fclose(ctx.fp);
			status = GET_DONE;
		}
	}
	else
	{
		buffer = new BYTE[get->m_size];
		size = get->m_conn->ReadAll(buffer, get->m_size);
		if (get->m_id)
			size = get->m_id->Decrypt(buffer, buffer);
		else if (get->m_key)
			size = get->m_key->Decrypt(buffer, size, buffer);
		status = GET_DONE;
	}

	if (size < 0)
		status = GET_ERROR;
	if (get->m_callback)
	{
		(*get->m_callback)(get->m_file,
				   status,
				   buffer,
				   size,
				   get->m_context);
	}

	delete [] buffer;
	get->m_trut->m_gets.Remove(get);
	delete get;
	PTP::Thread::Exit(0);

	return NULL;
}

/**
 * Trut::GetWait: Wait for get request to complete.
 * @file: File.
 */
void
Trut::GetWait(File *file)
{
	GetContext *get;
	m_gets.Lock();
	PTP_LIST_FOREACH(GetContext, get, &m_gets)
	{
		if (get->m_file == file)
			break;
	}
	m_gets.Unlock();
	if (get)
		get->m_thread.Wait();
}

/**
 * Trut::GetStop: Terminate get request.
 * @file: File.
 */
void
Trut::GetStop(File *file)
{
	GetContext *get;
	m_gets.Lock();
	PTP_LIST_FOREACH(GetContext, get, &m_gets)
	{
		if (get->m_file == file)
		{
			get->m_thread.Kill();
			m_gets.Remove(get, 0);
			delete get;
			break;
		}
	}
	m_gets.Unlock();
}

/**
 * Trut::JoinGroup: Join a secure group.
 * @name: Group name.
 * @wait: Time (in seconds) to wait for a group reply.
 * @join: Group join callback.
 * @accept: Group accept callback.
 * @context: Callback context.
 */
void
Trut::JoinGroup(const char *name,
		int wait,
		JoinCallback join,
		AcceptCallback accept,
		void *context)
{
	JoinStatus status = JOIN_ERROR;
	char *keyname = NULL;

	Group *group = FindGroup(name);
	if (!group || group->m_key)
	{
		if (!group)
		{
			group = new Group;
			group->m_trut = this;
			group->m_name = strdup(name);
			m_groups.Insert(group);
		}
		
		group->m_join = join;
		group->m_accept = accept;
		group->m_context = context;
		group->m_responses = 0;
		
		keyname = new char[(strlen(group->GetName()) + 32)];
		sprintf(keyname, "/secure/%s/key", group->GetName());
		
		Search(keyname, FindKey, group, NULL);
		PTP::Thread::Sleep(wait);
		
		if (group->m_key || group->m_responses > 0)
		{
			delete [] keyname;
			return;
		}
		
		SearchStop(group);
		
		group->m_key = new PTP::Key;
		{
			BYTE keydata[PTP::Key::KEY_SIZE];
			group->m_key->Export(keydata);
			
			PTP::Collection::Entry *entry
				= new PTP::Collection::Entry(keyname,
							     keydata,
							     sizeof(keydata));
			m_collect.Add(entry);
			status = JOIN_CREATED;
		}
	}
	else
		status = JOIN_OK;

	delete [] keyname;
	if (group->m_join)
	{
		(*group->m_join)(group, status, group->m_context);
	}
}

/*
 * Trut::FindKey: Group search response callback.
 * @file: Key file.
 * @context: Callback context.
 */
void
Trut::FindKey(File *file, void *context)
{
        Group *group = (Group*) context;
        if (!group || !group->m_trut)
                return;
        if (!group->m_key && strcmp("key", file->GetName()) == 0)
        {
                group->m_responses++;
                file->m_group = group;
                group->m_trut->Get(file, NULL, FetchKey, group);
        }

        delete file;
}

/*
 * Trut::FetchKey: Group key get callback.
 * @file: Key file.
 * @status: Get status.
 * @data: Key data.
 * @size: Data size.
 * @context: Callback context.
 */
void
Trut::FetchKey(File *file,
	       GetStatus status,
	       BYTE *data,
	       unsigned long size,
	       void *context)
{
        Group *group = (Group*) context;
        Trut *trut = group->m_trut;

        switch (status)
        {
        case GET_OK:
                return;
        case GET_DONE:
		{
			group->m_key = new PTP::Key(data);
			char *keyname = new char[
				(strlen(group->GetName())
				 + STRLEN_CONST("/secure/" "/key")
				 + 1)];
			sprintf(keyname, "/secure/%s/key", group->GetName());
			PTP::Collection::Entry *entry
				= new PTP::Collection::Entry(keyname,
							     data,
							     size);
			trut->m_collect.Add(entry);
			delete [] keyname;

			if (group->m_join)
			{
				(*group->m_join)(group,
						 JOIN_OK,
						 group->m_context);
			}
		}
                break;
        default:
                if (group->m_join)
                        (*group->m_join)(group, JOIN_ERROR, group->m_context);
                break;
	}
}

/**
 * Trut::FindGroup: Locate group by name.
 * @name: Group name.
 * Returns: Group.
 */
Trut::Group *
Trut::FindGroup(const char *name)
{
	if (!name)
		return NULL;
	Group *group;
	m_groups.Lock();
	PTP_LIST_FOREACH(Group, group, &m_groups)
	{
		if (strcmp(group->GetName(), name) == 0)
			break;
	}
	m_groups.Unlock();
	return group;
}

/**
 * Trut::LeaveGroup: Leave secure group.
 * @group: Group.
 */
void
Trut::LeaveGroup(Group *group)
{
	if (!group)
		return;
	m_groups.Remove(group);
	delete group;
}

/**
 * Trut::LeaveAllGroups: Leave all secure groups.
 */
void
Trut::LeaveAllGroups()
{
	Group *group;
	m_groups.Lock();
	PTP_LIST_FOREACH(Group, group, &m_groups)
	{
		m_groups.Remove(group, 0);
		delete group;
	}
	m_groups.Unlock();
}

/**
 * Trut::AddShared: Share directory files with a group.
 * @path: Directory pathname.
 * @ext: File extension list.
 * @group: Group.
 * Returns: Shared directory information.
 */
Trut::Shared *
Trut::AddShared(const char *path, const char *ext, Group *group)
{
	Shared *shared = new Shared;
	shared->m_path = strdup(path);
	shared->m_ext = ext ? strdup(ext):NULL;
	shared->m_group = group;
	m_shared.Insert(shared);
	m_collect.Add(shared->GetPath(), shared->GetExt(), group);
	RescanAllShared();
	return shared;
}

/**
 * Trut::RemoveShared: Remove directory from shared list.
 * @shared: Shared directory information.
 */
void
Trut::RemoveShared(Shared *shared)
{
	m_shared.Remove(shared);
	m_collect.Remove(shared->GetPath());
	RescanAllShared();
	delete shared;
}

/**
 * Trut::UpdateShared: Update shared directory information.
 * @shared: Shared directory information.
 * @ext: File extension list.
 * @group: Group.
 */
void
Trut::UpdateShared(Shared *shared, const char *ext, Group *group)
{
	m_collect.Remove(shared->GetPath());
	delete [] shared->m_ext;
	shared->m_ext = ext ? strdup(ext):NULL;
	shared->m_group = group;
	m_collect.Add(shared->GetPath(), shared->GetExt(), group);
	RescanAllShared();
}

/**
 * Trut::RescanAllShared: Scan shared directories for matching files.
 */
void
Trut::RescanAllShared()
{
	m_collect.Rescan();
}

/**
 * Trut::RemoveAllShared: Remove all directories from shared list.
 */
void
Trut::RemoveAllShared()
{
	Shared *shared;
	m_shared.Lock();
	PTP_LIST_FOREACH(Shared, shared, &m_shared)
	{
		m_collect.Remove(shared->GetPath());
		m_shared.Remove(shared, 0);
		delete shared;
	}
	m_shared.Unlock();
}

/*
 * Trut::HandleSearchResponse: Handle a search response.
 * @host: Source of response.
 * @resp: Response packet.
 * Returns: 0 on success or -1 on invalid search response.
 */
int
Trut::HandleSearchResponse(Host *host, Gnutella::SearchResp *resp)
{
        SearchContext *search;
        m_searches.Lock();
	PTP_LIST_FOREACH(SearchContext, search, &m_searches)
        {
		if (memcmp(search->m_guid,
			   resp->guid,
			   sizeof(search->m_guid)) == 0)
			break;
	}
        m_searches.Unlock();

        if (!search)
                return -1;

	if (search->m_group)
	{
		BYTE *data = (BYTE*) resp + sizeof(Gnutella::Packet);
		int size = Gnutella::Get32(resp->size);
		size = search->m_group->m_key->Decrypt(data, size, data);
		if (size <= 0)
			return -1;
		Gnutella::Set32(resp->size, size);
	}

        if (!search->m_callback)
                return 0;
	
        Gnutella::SearchEntry *entry = (Gnutella::SearchEntry*)(resp + 1);
        for (int i = 0; i < resp->count; i++)
        {
                File *file = new File;
		file->m_trut = this;
                file->m_name = strdup(entry->name);
                file->m_size = Gnutella::Get32(entry->size);
                file->m_ref = Gnutella::Get32(entry->ref);
                file->m_ip = ntohl(Gnutella::Get32(resp->ip));
                file->m_port = Gnutella::Get16(resp->port);
                file->m_speed = Gnutella::Get32(resp->speed);
		file->m_group = search->m_group;
		
                (*search->m_callback)(file, search->m_context);
                
                entry = entry->GetNext();
        }
	
        return 0;
}

/**
 * Trut::HandleSearchRequest: Handle a search request.
 * @host: Request source.
 * @rqst: Search request.
 * Returns: 0 on success or -1 on error.
 */
int
Trut::HandleSearchRequest(Host *host, Gnutella::SearchRqst *rqst)
{
	char *str = rqst->search;

	BYTE *plain = NULL;
	Group *group = NULL;
	int key = 0;

	if (STRNCMP_CONST(str, "/secure/") == 0)
	{
		char *start = str + STRLEN_CONST("/secure/");
		char *end = strchr(start, '/');
		if (!end)
			return -1;
		*end = '\0';
		group = FindGroup(start);
		*end = '/';
		
		start = end + 1;
		key = (strcmp(start, "key") == 0);
		if (key)
			group = NULL;
		
		if (group)
		{
			int ciphersize = strlen(start);
			int plainsize = group->m_key->Decrypt(
				NULL,
				ciphersize >> 1,
				NULL);
			plain = new BYTE[ciphersize + 4];
			ciphersize = PTP::Encoding::DecodeBase64(
				start,
				ciphersize,
				plain);
			plainsize = group->m_key->Decrypt(plain,
							  ciphersize,
							  plain);
			if (plainsize < 0)
			{
				delete [] plain;
				return 0;
			}
			plain[plainsize] = '\0';
			str = (char*) plain;
                }
        }

	PTP::Collection::Entry *entry = NULL;
        int size = ((sizeof(Gnutella::SearchResp)
		     + sizeof(Gnutella::SearchTrailer)));
        int count = 0;
        for (;;)
        {
                entry = m_collect.Find(str, entry);
                if (!entry)
                        break;
		if (entry->GetContext() == (void*) group)
		{
			const char *name = key ? "key":entry->GetName();
			size += (sizeof(Gnutella::SearchEntry) + strlen(name));
			count++;
		}
        }

        if (!count)
	{
		delete [] plain;
		return 0;
	}

	int osize = size;
	if (group)
	{
		size -= sizeof(Gnutella::Packet);
		size = group->m_key->Encrypt(NULL, size, NULL);
		size += sizeof(Gnutella::Packet);
	}

	Gnutella::SearchResp *resp = (Gnutella::SearchResp*) new BYTE[size];
        memset(resp, 0, size);

        memcpy(resp->guid, rqst->guid, sizeof(resp->guid));
        resp->type = Gnutella::SEARCH_RESPONSE;
        resp->ttl = Gnutella::DEFAULT_TTL;
        resp->hops = 0;
        Gnutella::Set32(resp->size, size - sizeof(Gnutella::Packet));
        resp->count = count;
        Gnutella::Set16(resp->port, m_port);
        Gnutella::Set32(resp->ip, htonl(m_ip));
        Gnutella::Set32(resp->speed, 0);

        Gnutella::SearchEntry *srch = (Gnutella::SearchEntry*)(resp + 1);
        entry = NULL;
        for (;;)
        {
                entry = m_collect.Find(str, entry);
                if (!entry)
                        break;
		if (entry->GetContext() == (void*) group)
		{
			const char *name = key ? "key":entry->GetName();
			Gnutella::Set32(srch->ref, entry->GetId());
			Gnutella::Set32(srch->size, entry->GetSize());
			strcpy(srch->name, name);
			srch = srch->GetNext();
		}
	}

	delete [] plain;

        Gnutella::SearchTrailer *trailer = (Gnutella::SearchTrailer*) srch;
        PTP::Random::Fill(trailer->guid, sizeof(trailer->guid));

	if (group)
	{
		BYTE *data = (BYTE*) resp + sizeof(Gnutella::Packet);
		int size = osize - sizeof(Gnutella::Packet);
		size = group->m_key->Encrypt(data, size, data);
	}
	
        host->SendGnutella((const BYTE*) resp);

        delete [] ((BYTE*) resp);

	return 0;
}

/**
 * Trut::GetRead: Read data from network.
 * @buffer: [!OUT] Data buffer.
 * @size: Data size.
 * @context: Transfer context.
 * Returns: Read size.
 */
int
Trut::GetRead(BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	int s = ctx->conn->Read(buffer, size);
	return s;
}

/**
 * Trut::GetWrite: Write data to a file.
 * @buffer: Data buffer.
 * @size: Data size.
 * @context: Transfer context.
 * Returns: Write size.
 */
int
Trut::GetWrite(const BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	int s = fwrite(buffer, 1, size, ctx->fp);
	if (s >= 0)
	{
		ctx->size += s;
		if (ctx->callback)
		{
			(*ctx->callback)(ctx->file,
					 Trut::GET_OK,
					 NULL,
					 ctx->size,
					 ctx->context);
		}
	}
	return s;
}

/**
 * Trut::PutRead: Read data from a file.
 * @buffer: [!OUT] Data buffer.
 * @size: Data size.
 * @context: Transfer context.
 * Returns: Read size.
 */
int
Trut::PutRead(BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	return fread(buffer, 1, size, ctx->fp);
}

/**
 * Trut::PutWrite: Write data to the network.
 * @buffer: Data buffer.
 * @size: Data size.
 * @context: Transfer context.
 * Returns: Write size.
 */
int
Trut::PutWrite(const BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	return ctx->conn->WriteAll(buffer, size);
}
