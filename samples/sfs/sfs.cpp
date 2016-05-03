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
 * sfs: Secure File Sharing
 */

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <ptp/ptp.h>
#include <ptp/id.h>
#include <ptp/auth.h>
#include <ptp/store.h>
#include <ptp/collect.h>
#include <ptp/net.h>
#include <ptp/key.h>
#include <ptp/rand.h>
#include <ptp/debug.h>

// Uncomment to build for performance testing
//#define PERF_TEST 1

const char *prog = NULL;

#define STRLEN_CONST(x) (sizeof(x) - 1)
#define STRNCMP_CONST(x, y) strncmp((x), (y), sizeof(y) - 1)

#define SFS_DEFAULT_PORT 8080
#define SFS_READ_SIZE 8192

#define SFS_FLAGS_ROLL 0x1
#define SFS_FLAGS_PLAINTEXT_XFER 0x2

#define SFS_AUTH_URL "/auth"
#define SFS_RESP_URL "/resp"
#define SFS_SEARCH_URL "/search"

/*
 * SFS network protocol
 *
 * Authentication:
 *   HTTP PUT /auth | CERT
 *   HTTP OK | CHAL | CERT
 *   HTTP PUT /resp | RESP | CHAL | ENVELOPE(FLAGS)
 *   HTTP OK | RESP | ENVELOPE(KEYID | SHADOW | KEY)
 *
 * Search:
 *   HTTP PUT /search | KEYID | E(DATA)
 *   HTTP OK | E(DATA)
 *
 * Get:
 *   HTTP GET /KEYID/FILEID
 *   HTTP OK | E(DATA)
 */

struct Key:public PTP::List::Entry
{
	struct Shared
	{
		BYTE keyid[4];
		BYTE shadow[4];
		BYTE key[PTP::Key::KEY_SIZE];
	};

	BYTE id[PTP::Identity::KEY_SIZE];
	PTP::Net::Ip ip;
	PTP::Net::Port port;
	unsigned long keyid;
	unsigned long shadow;
	int flags;
	Shared shared;
};

struct Response:public PTP::List::Entry
{
	int id;
	char *name;
	char *url;
};

struct TransferContext
{
	TransferContext(PTP::Net::Connection *conn, FILE *filep)
		:c(conn), fp(filep) {}

	PTP::Net::Connection *c;
	FILE *fp;
};


static Key *
FindKey(PTP::List *keys,
	const PTP::Identity *ident,
	PTP::Net::Ip ip,
	PTP::Net::Port port)
{
	BYTE id[PTP::Identity::KEY_SIZE];
	if (ident)
		ident->GetKey(id);

	Key *key = NULL;
	keys->Lock();
	PTP_LIST_FOREACH(Key, key, keys)
	{
		if (key->ip == ip
		    && key->port == port
		    && (!ident || memcmp(key->id, id, sizeof(id)) == 0))
			break;
	}
	keys->Unlock();
	return key;
}

static Key *
CreateKey(PTP::List *keys, const PTP::Identity *id, PTP::Net::Ip ip, int flags)
{
	static unsigned long keyid = 0;

	Key *key = FindKey(keys, id, ip, 0);
	if (!key)
	{
		key = new Key;
		id->GetKey(key->id);
		key->ip = ip;
		key->port = 0;
		key->keyid = 0;
		keys->Insert(key);
		flags |= SFS_FLAGS_ROLL;
	}

	if (flags & SFS_FLAGS_ROLL)
	{
		key->keyid = keyid++;
		PTP::Net::Set32(key->shared.keyid, key->keyid);
		PTP::Random::Fill((BYTE*) &key->shadow, sizeof(key->shadow));
		PTP::Net::Set32(key->shared.shadow, key->shadow);
		PTP::Random::Fill(key->shared.key, sizeof(key->shared.key));
		unsigned long shadow2 = 0;
		PTP::Random::Fill((BYTE*) &shadow2, sizeof(shadow2));
		key->shadow ^= shadow2;
	}
	key->flags = (flags & ~SFS_FLAGS_ROLL);
	
	return key;
}

static Key *
ImportKey(PTP::List *keys,
	  const PTP::Identity *id,
	  PTP::Net::Ip ip,
	  PTP::Net::Port port,
	  int flags,
	  const Key::Shared *shared)
{
	Key *key = FindKey(keys, id, ip, port);
	if (!key)
	{
		key = new Key;
		id->GetKey(key->id);
		key->ip = ip;
		key->port = port;
		key->keyid = PTP::Net::Get32(shared->keyid);
		key->shadow = PTP::Net::Get32(shared->shadow);
		memcpy(&key->shared, shared, sizeof(key->shared));
		keys->Insert(key);
	}
	else
	{
		key->keyid = PTP::Net::Get32(shared->keyid);
		key->shadow = PTP::Net::Get32(shared->shadow);
		memcpy(&key->shared, shared, sizeof(key->shared));
	}
	key->flags = (flags & ~SFS_FLAGS_ROLL);
	return key;
}

static Key *
FindKey(PTP::List *keys, unsigned long keyid)
{
	Key *key = NULL;
	keys->Lock();
	PTP_LIST_FOREACH(Key, key, keys)
	{
		if (key->keyid == keyid)
			break;
	}
	keys->Unlock();
	return key;
}

static void
DestroyKeys(PTP::List *keys)
{
	Key *key = NULL;
	keys->Lock();
	PTP_LIST_FOREACH(Key, key, keys)
	{
		keys->Remove(key, 0);
		delete key;
	}
	keys->Unlock();
}

static int
InsertResponse(PTP::List *resps, int *count, char *start, char **end)
{
	start = strstr(start, "HREF=\"");
	if (!start)
		return -1;
	start += STRLEN_CONST("HREF=\"");
	char *e = strchr(start, '"');
	if (!e)
		return -1;

	int size = e - start;
	char *url = new char[size + 1];
	memcpy(url, start, size);
	url[size] = '\0';

	start = e + 2;
	e = strchr(start, '<');
	if (!e)
	{
		delete [] url;
		return -1;
	}

	size = e - start;
	char *name = new char[size + 1];
	memcpy(name, start, size);
	name[size] = '\0';

	Response *resp = new Response;
	(*count)++;
	resp->id = *count;
	resp->name = name;
	resp->url = url;
	resps->Append(resp);

	if (end)
		*end = e + 1;

	return 0;
}

static Response *
FindResponse(PTP::List *resps, int id)
{
	Response *resp;
	resps->Lock();
	PTP_LIST_FOREACH(Response, resp, resps)
	{
		if (resp->id == id)
			break;
	}
	resps->Unlock();
	return resp;
}

static void
ShowResponses(PTP::List *resps, int showid)
{
	resps->Lock();
	Response *resp;
	PTP_LIST_FOREACH(Response, resp, resps)
	{
		printf("  ");
		if (showid)
			printf("%d) ", resp->id);
		printf("%s (%s)\n", resp->name, resp->url);
	}
	resps->Unlock();
}

static void
DestroyResponses(PTP::List *resps)
{
	resps->Lock();
	Response *resp;
	PTP_LIST_FOREACH(Response, resp, resps)
	{
		resps->Remove(resp, 0);
		delete [] resp->name;
		delete [] resp->url;
		delete resp;
	}
	resps->Unlock();
}

static PTP::Net::Connection *
Connect(const char *url)
{
	PTP::Net::Port port;
	PTP::Net::Ip ip = PTP::Net::Lookup(url, &port, SFS_DEFAULT_PORT);
	if (!ip || !port)
	{
		printf("%s: invalid host `%s'.\n", prog, url);
		return NULL;
	}

	PTP::Net::Connection *c
		= new PTP::Net::Connection(PTP::Net::Connection::HTTP,
					   ip,
					   port);
	if (c->Open())
	{
		printf("%s: connection to `%s' failed.\n", prog, url);
		delete c;
		return NULL;
	}

	return c;
}

static int
Insert(PTP::Store *store, PTP::Identity *id)
{
	PTP::Identity *local = store->Find(NULL, 1);
	if (local && strcmp(local->GetName(), id->GetName()) == 0)
		return 0;

	PTP::Identity *issuer = id;
	if (id && strcmp(id->GetName(), id->GetIssuerName()) != 0)
		issuer = store->Find(id->GetIssuerName());
	if (!issuer || issuer->Verify(id) != 0)
		return -1;

	PTP::Identity *old = store->Find(id->GetName(), 0);
	if (old)
		store->Remove(old);
	store->Insert(id, 0);
	store->Save();

	return 0;
}

static Key *
Auth(PTP::Store *store, PTP::List *keys, PTP::Net::Connection *c, int flags)
{
	if (!c)
		return NULL;

	Key *key = FindKey(keys, NULL, c->GetIp(), c->GetPort());
	if (key)
		return key;

	const PTP::Identity *localId = store->Find(NULL, 1);
	int size = PTP::Store::Export(localId, 0, NULL, NULL, NULL);
	BYTE *buffer = new BYTE[size];
	PTP::Store::Export(localId, 0, NULL, NULL, buffer);

	int st = c->WriteHttp("PUT", SFS_AUTH_URL, NULL, buffer, NULL, size);
	delete [] buffer;
	if (st)
	{
		c->Close();
		return NULL;
	}

	buffer = c->ReadHttp(NULL, -1, NULL, &size);
	c->Close();
	if (!buffer || size <= PTP::Authenticator::CHALLENGE_SIZE)
	{
		delete [] buffer;
		return NULL;
	}

	BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
	memcpy(chal, buffer, sizeof(chal));
	PTP::Identity *remoteId = PTP::Store::Import(
		buffer + sizeof(chal),
		size - sizeof(chal),
		NULL,
		NULL);
	delete [] buffer;
	if (!remoteId || Insert(store, remoteId))
	{
		delete remoteId;
		return NULL;
	}

	BYTE fl = (BYTE) flags;
	size = PTP::Store::ExportEnvelope(
		&fl,
		sizeof(fl),
		NULL,
		remoteId,
		localId);
	size += (PTP::Authenticator::RESPONSE_SIZE
		 + PTP::Authenticator::CHALLENGE_SIZE);
	buffer = new BYTE[size];

	PTP::Authenticator auth(store);
	auth.Respond(chal, buffer);
	auth.Challenge(remoteId,
		       60,
		       (void*) remoteId,
		       buffer + PTP::Authenticator::RESPONSE_SIZE);
	PTP::Store::ExportEnvelope(
		&fl,
		sizeof(fl),
		buffer
		+ PTP::Authenticator::RESPONSE_SIZE
		+ PTP::Authenticator::CHALLENGE_SIZE,
		remoteId,
		localId);

	st = c->WriteHttp("PUT", SFS_RESP_URL, NULL, buffer, NULL, size);
	delete [] buffer;
	if (st)
	{
		c->Close();
		delete remoteId;
		return NULL;
	}

	buffer = c->ReadHttp(NULL, -1, NULL, &size);
	c->Close();

	if (!buffer
	    || size <= PTP::Authenticator::RESPONSE_SIZE
	    || auth.Verify(buffer) != remoteId
	    || PTP::Store::ImportEnvelope(
		    buffer + PTP::Authenticator::RESPONSE_SIZE,
		    size - PTP::Authenticator::RESPONSE_SIZE,
		    buffer,
		    localId,
		    remoteId) != sizeof(Key::Shared))
	{
		delete [] buffer;
		delete remoteId;
		return NULL;
	}

	key = ImportKey(keys,
			remoteId,
			c->GetIp(),
			c->GetPort(),
			flags,
			(const Key::Shared*) buffer);
	delete [] buffer;
	delete remoteId;
	if (!key)
		return NULL;

	return key;
}

static int
HandleAuth(PTP::Store *store,
	   PTP::Authenticator *auth,
	   PTP::List *keys,
	   PTP::Net::Connection *c)
{	
	int size = 0;
	BYTE *buffer = c->ReadHttp(NULL, -1, NULL, &size);
	if (!buffer)
		return -1;

	const PTP::Identity *localId = store->Find(NULL, 1);

	PTP::Identity *remoteId
		= PTP::Store::Import(buffer, size, NULL, NULL);
	delete [] buffer;
	if (!remoteId)
	{
		c->Close();
		return -1;
	}
	
	size = PTP::Authenticator::CHALLENGE_SIZE;
	size += PTP::Store::Export(localId, 0, NULL, NULL, NULL);
	
	buffer = new BYTE[size];
	auth->Challenge(remoteId, 60, (void*) remoteId,	buffer);
	PTP::Store::Export(
		localId,
		0,
		NULL,
		NULL,
		buffer + PTP::Authenticator::CHALLENGE_SIZE);
	
	int st = c->WriteHttp(PTP::Net::HTTP_OK, NULL, buffer, NULL, size);
	delete [] buffer;
	c->Close();

	if (st)
		return -1;
	return 0;
}

static int
HandleResp(PTP::Store *store,
	   PTP::Authenticator *auth,
	   PTP::List *keys,
	   PTP::Net::Connection *c)
{	
	int size = 0;
	BYTE *buffer = c->ReadHttp(NULL, -1, NULL, &size);
	if (!buffer)
		return -1;

	int esize = (size
		     - PTP::Authenticator::RESPONSE_SIZE
		     - PTP::Authenticator::CHALLENGE_SIZE);
	if (esize <= 0)
	{
		delete [] buffer;
		return -1;
	}
	
	const PTP::Identity *localId = store->Find(NULL, 1);
	PTP::Identity *remoteId
		= (PTP::Identity*) auth->Verify(buffer);
	if (!remoteId)
	{
		delete [] buffer;
		return -1;
	}

	BYTE resp[PTP::Authenticator::RESPONSE_SIZE];
	memcpy(resp, buffer, sizeof(resp));
	BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
	memcpy(chal, buffer + sizeof(resp), sizeof(chal));

	BYTE fl;
	if (PTP::Store::ImportEnvelope(
		buffer + (size - esize),
		esize,
		&fl,
		localId,
		remoteId) != sizeof(fl))
	{
		delete remoteId;
		delete [] buffer;
		return -1;
	}
	int flags = (int) fl;
	delete [] buffer;
	
	Key *key = CreateKey(keys, remoteId, c->GetIp(), flags);
	if (!key)
		return -1;
	
	size = PTP::Store::ExportEnvelope(
		(BYTE*) &key->shared,
		sizeof(key->shared),
		NULL,
		remoteId,
		localId);
	size += PTP::Authenticator::RESPONSE_SIZE;
	buffer = new BYTE[size];
	auth->Respond(chal, buffer);
	PTP::Store::ExportEnvelope(
		(BYTE*) &key->shared,
		sizeof(key->shared),
		buffer + PTP::Authenticator::RESPONSE_SIZE,
		remoteId,
		localId);
	delete remoteId;

	int st = c->WriteHttp(PTP::Net::HTTP_OK, NULL, buffer, NULL, size);
	delete [] buffer;
	c->Close();
	
	if (st)
		return -1;
	return 0;
}

static int
Search(PTP::Net::Connection *c, Key *key, const char *str, PTP::List *resps)
{
	DestroyResponses(resps);

	PTP::Key k(key->shared.key);
	int size = k.Encrypt(NULL, strlen(str), NULL);
	if (size <= 0)
		return -1;
	size += 4;

	BYTE *buffer = new BYTE[size];
	memcpy(buffer, key->shared.keyid, 4);
	k.Encrypt((const BYTE*) str, strlen(str), buffer + 4);

	int st = c->WriteHttp("PUT", SFS_SEARCH_URL, NULL, buffer, NULL, size);
	delete [] buffer;
	if (st)
		return -1;

	buffer = c->ReadHttp(NULL, -1, NULL, &size);
	if (!buffer)
		return -1;

	size = k.Decrypt(buffer, size, buffer);
	if (size <= 0)
	{
		delete [] buffer;
		return -1;
	}
	buffer[size] = '\0';

	int count = 0;
	char *b = (char*) buffer;
	for (;;)
	{
		if (InsertResponse(resps, &count, b, &b))
			break;
	}

	delete [] buffer;

	return 0;
}

static int
HandleSearch(PTP::Collection *collect,
	     PTP::List *keys,
	     PTP::Net::Ip localIp,
	     PTP::Net::Port localPort,
	     PTP::Net::Connection *c)
{	
	int datasize;
	BYTE *data = c->ReadHttp(NULL, -1, NULL, &datasize);
	if (!data)
		return -1;

	unsigned long keyid = PTP::Net::Get32(data);
	Key *key = FindKey(keys, keyid);
	if (!key)
	{
		delete [] data;
		return -1;
	}

	PTP::Key k(key->shared.key);
	int size = k.Decrypt(data + 4, datasize - 4, data);
	if (size <= 0)
	{
		delete [] data;
		return -1;
	}
	data[size] = '\0';

	BYTE buffer[2048];
	char *b = (char*) buffer;
	PTP::Collection::Entry *entry = NULL;
	b += sprintf(b, "<HTML>\n");
	for (;;)
	{
		entry = collect->Find((char*) data, entry);
		if (!entry)
			break;
		b += sprintf(b,
			     "<A HREF=\""
			     "http://%lu.%lu.%lu.%lu:%u/%08lx/%08lx"
			     "\">%s</A><BR>\n",
			     (localIp >> 24) & 0xff,
			     (localIp >> 16) & 0xff,
			     (localIp >> 8) & 0xff,
			     localIp & 0xff,
			     localPort,
			     PTP::Net::Get32(key->shared.keyid),
			     entry->GetId() ^ key->shadow,
			     entry->GetName());
	}
	b += sprintf(b, "</HTML>\n");
	size = k.Encrypt(buffer, strlen((char*) buffer), buffer);
	delete [] data;
	
	if (c->WriteHttp(PTP::Net::HTTP_OK, NULL, buffer, NULL, size))
		return -1;

	return 0;
}

static int
GetRead(BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	return ctx->c->Read(buffer, size);
}

static int
GetWrite(const BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	return fwrite(buffer, 1, size, ctx->fp);
}

static int
Get(PTP::Net::Connection *c,
    Key *key,
    const char *url,
    const char *path,
    int *total)
{
	if (STRNCMP_CONST(url, "http://") == 0)
		url = strchr(url + 7, '/');
	else
		url = strchr(url, '/');

	if (c->WriteHttp("GET", url, NULL, NULL, NULL, 0))
		return -1;

	int status;
	char *hdr = c->ReadHttpHdr(NULL, -1, &status, NULL);
	delete [] hdr;
	if (status != PTP::Net::HTTP_OK)
		return -1;

	FILE *fp = fopen(path, "wb");
	if (!fp)
		return -1;

	PTP::Key k(key->shared.key);
	TransferContext ctx(c, fp);
	int size = 0;

	if ((key->flags & SFS_FLAGS_PLAINTEXT_XFER) == 0)
		size = k.Decrypt(GetRead, GetWrite, &ctx, SFS_READ_SIZE);
	else
	{
		size = PTP::Key::Transfer(GetRead,
					  GetWrite,
					  &ctx,
					  SFS_READ_SIZE);
	}
	fclose(fp);

	if (size <= 0)
	{
		unlink(path);
		return -1;
	}
	if (total)
		*total = size;

	return 0;
}

static int
HandleGetRead(BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	return fread(buffer, 1, size, ctx->fp);
}

static int
HandleGetWrite(const BYTE *buffer, int size, void *context)
{
	TransferContext *ctx = (TransferContext*) context;
	return ctx->c->WriteAll(buffer, size);
}

static int
HandleGet(PTP::Collection *collect, PTP::List *keys, PTP::Net::Connection *c)
{	
	char *hdr = c->ReadHttpHdr(NULL, -1, NULL, NULL);
	if (!hdr)
		return -1;

	char *d = hdr + 5;
	unsigned long keyid = strtoul(d, &d, 16);
	unsigned long id = strtoul(d + 1, NULL, 16);
	delete [] hdr;
	
	Key *key = FindKey(keys, keyid);
	if (!key)
		return -1;
	id ^= key->shadow;
	
	PTP::Collection::Entry *entry = collect->Find(id);
	if (!entry)
		return -1;

	FILE *fp = fopen(entry->GetPath(), "rb");
	if (!fp)
		return -1;

	PTP::Key k(key->shared.key);
	int size = k.Encrypt(NULL, entry->GetSize(), NULL);
	if (c->WriteHttp(PTP::Net::HTTP_OK, NULL, NULL, NULL, size))
		return -1;

	TransferContext ctx(c, fp);
	if ((key->flags & SFS_FLAGS_PLAINTEXT_XFER) == 0)
		k.Encrypt(HandleGetRead, HandleGetWrite, &ctx, SFS_READ_SIZE);
	else
	{
		PTP::Key::Transfer(HandleGetRead,
				   HandleGetWrite,
				   &ctx,
				   SFS_READ_SIZE);
	}
	fclose(fp);

	return 0;
}

static int
Share(PTP::Store *store,
      PTP::List *keys,
      PTP::Net::Ip localIp,
      PTP::Net::Port localPort,
      PTP::Collection *collect)
{
	PTP::Net::Connection serv(PTP::Net::Connection::HTTP, localPort);
	PTP::Authenticator auth(store);

	for (;;)
	{
                PTP::Net::Connection *client = serv.Accept(1);
                if (!client)
                        continue;

		char buf[64];
		int size = client->Read((BYTE*) buf, sizeof(buf));
		if (size <= 0)
		{
			delete client;
			continue;
		}
		client->Unget((BYTE*) buf, size);

		if (STRNCMP_CONST(buf, "GET ") == 0)
			HandleGet(collect, keys, client);
		else if (STRNCMP_CONST(buf, "PUT " SFS_SEARCH_URL)== 0)
			HandleSearch(collect, keys,localIp, localPort, client);
		else if (STRNCMP_CONST(buf, "PUT " SFS_AUTH_URL) == 0)
			HandleAuth(store, &auth, keys, client);
		else if (STRNCMP_CONST(buf, "PUT " SFS_RESP_URL) == 0)
			HandleResp(store, &auth, keys, client);
		delete client;
	}

	return 0;
}

static int
MatchOpt(char **args, const char *s2, int count)
{
	char *s1 = *args;
	if (s1[0] != '-' || s1[1] != '-')
		return 0;
	s1 += 2;
	if (strcmp(s1, s2))
		return 0;
	int cnt = 0;
	for (; args[cnt]; cnt++) ;
	return (cnt >= count);
}

static void
HandleOpts(PTP::Store *store, int argc, char **argv)
{
	PTP::Net::Ip localIp = PTP::Net::Lookup("localhost");
	PTP::Net::Port localPort = SFS_DEFAULT_PORT;
	PTP::Collection collect;
	PTP::List keys;
	PTP::List resps;

	int flags = 0;
	int i = 1;

	for (;;)
	{
		if (i >= argc)
			break;
		char **args = argv + i;
		i++;

		if (MatchOpt(args, "search", 3))
		{
			PTP::Net::Connection *c	= Connect(args[1]);
			Key *key = Auth(store, &keys, c, flags);
			if (!key)
				printf("%s: authorization failed.\n", prog);
			else if (Search(c, key, args[2], &resps))
			{
				printf("%s: search of `%s' failed.\n",
				       prog,
				       args[1]);
			}
			else
				ShowResponses(&resps, 0);
			delete c;
			i += 2;
		}
		else if (MatchOpt(args, "get", 3))
		{
			PTP::Net::Connection *c	= Connect(args[1]);
			Key *key = Auth(store, &keys, c, flags);
			if (!key)
				printf("%s: authorization failed.\n", prog);
			else if (Get(c, key, args[1], args[2], NULL))
			{
				printf("%s: get of `%s' failed.\n",
				       prog,
				       args[1]);
			}
			delete c;
			i++;
		}
		else if (MatchOpt(args, "share", 2))
		{
			collect.Add(args[1], NULL);
			i++;
		}
		else if (MatchOpt(args, "port", 2))
		{
			localPort = (PTP::Net::Port)
				strtoul(args[1], NULL, 10);
			i++;
		}
		else if (MatchOpt(args, "proxy", 2))
		{
			PTP::Net::Port port;
			PTP::Net::Ip ip	= PTP::Net::Lookup(args[1],
							   &port,
							   80);
                        if (!ip || !port)
			{
                                printf("%s: invalid proxy `%s'.\n",
				       prog,
				       args[1]);
			}
			else
				PTP::Net::Connection::SetProxy(ip, port);
			i++;
		}
		else
		{
			printf("Usage: %s [OPTIONS]\n", prog);
			printf("  --search URL STRING\n");
			printf("  --get URL PATH\n");
			printf("  --share DIR\n");
			printf("  --port PORT\n");
			printf("  --proxy URL\n");
			printf("  --help\n");
			DestroyKeys(&keys);
			return;
		}
	}
	
	collect.Rescan();
	if (collect.GetSize())
		Share(store, &keys, localIp, localPort, &collect);

	DestroyKeys(&keys);
	DestroyResponses(&resps);
}

static int
MatchCmd(char **args, const char *s2, int count)
{
	const char *s1 = *args;
	for (; *s1 && *s1 == *s2; s1++, s2++) ;
	if (*s1)
		return 0;
	int cnt = 0;
	for (; args[cnt]; cnt++) ;
	return (cnt == (count + 1));
}

#ifdef PERF_TEST
static unsigned long
GetTime()
{
#ifdef WIN32
	return GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((unsigned long) tv.tv_sec * 1000
		+ ((unsigned long) tv.tv_usec / 1000));
#endif
}
#endif // PERF_TEST

static void
HandleCmds(PTP::Store *store, FILE *in)
{
	PTP::Net::Ip localIp = PTP::Net::Lookup("localhost");
	PTP::Net::Port localPort = SFS_DEFAULT_PORT;
	PTP::Collection collect;
	PTP::List keys;
	PTP::List resps;

	int flags = 0;
	int quiet = 0;
#ifdef PERF_TEST
	unsigned long mark = 0;
#endif // PERF_TEST
	unsigned long total = 0;

	for (;;)
	{
		if (!quiet)
		{
			printf("(sfs) ");
			fflush(stdout);
		}

		char buffer[256];
		*buffer = '\0';
		if (!fgets(buffer, sizeof(buffer), in) || !*buffer)
			break;

		if (in != stdin && !quiet)
			printf("%s", buffer);
		
		int size = strlen(buffer);
		if (buffer[size - 1] == '\n')
			buffer[size - 1] = '\0';

		char *args[4];
		memset(args, 0, sizeof(args));
		char *arg = buffer;
		for (int j = 0; j < 3; j++)
		{
			arg += strspn(arg, " \t");
			args[j] = *arg ? arg:NULL;
			arg += strcspn(arg, " \t");
			if (!isspace(*arg))
				break;
			*arg++ = '\0';
		}

		if (!args[0])
			continue;

		if (MatchCmd(args, "search", 2))
		{
			PTP::Net::Connection *c	= Connect(args[1]);
			Key *key = Auth(store, &keys, c, flags);
			if (!key)
				printf("%s: authorization failed.\n", prog);
			else if (Search(c, key, args[2], &resps))
			{
				printf("%s: search of `%s' failed.\n",
				       prog,
				       args[1]);
			}
			else
				ShowResponses(&resps, 1);
			delete c;
		}
		else if (MatchCmd(args, "get", 2))
		{
			int id = (int) strtoul(args[1], NULL, 10);
			Response *resp = FindResponse(&resps, id);
			if (!resp)
				continue;

			PTP::Net::Connection *c	= Connect(resp->url);
			Key *key = Auth(store, &keys, c, flags);
			if (!key)
				printf("%s: authorization failed.\n", prog);
			else
			{
				int size = 0;
				if (Get(c, key, resp->url, args[2], &size))
				{
					printf("%s: get of `%s' "
					       "failed.\n",
					       prog,
					       resp->url);
				}
				else
					total += (unsigned long) size;
			}
			delete c;
		}
		else if (MatchCmd(args, "share", 1))
		{
			collect.Add(args[1], NULL);
		}
		else if (MatchCmd(args, "wait", 0))
		{
			collect.Rescan();
			if (collect.GetSize())
			{
				Share(store,
				      &keys,
				      localIp, localPort,
				      &collect);
			}
		}
		else if (MatchCmd(args, "port", 1))
		{
			localPort = (PTP::Net::Port)
				strtoul(args[1], NULL, 10);
		}
		else if (MatchCmd(args, "proxy", 1))
		{
			PTP::Net::Port port;
			PTP::Net::Ip ip = PTP::Net::Lookup(args[1], &port, 80);
                        if (!ip || !port)
			{
                                printf("%s: invalid proxy `%s'.\n",
				       prog,
				       args[1]);
			}
			else
				PTP::Net::Connection::SetProxy(ip, port);
		}
		else if (MatchCmd(args, "quit", 0))
		{
			break;
		}
#ifdef PERF_TEST
		else if (MatchCmd(args, "quiet", 1))
		{
			quiet = (int) strtoul(args[1], NULL, 10);
		}
		else if (MatchCmd(args, "mark", 0))
		{
			mark = GetTime();
			total = 0;
		}
		else if (MatchCmd(args, "time", 0))
		{
			unsigned long delta
				= GetTime() - mark;
			float mbps = ((float) total
				      / (float) delta
				      / (float) 1024.0
				      / (float) 1024.0
				      * (float) 1000.0);
			printf("%ld bytes received in %.02f secs"
			       " (%.04f MB/s)\n",
			       total,
			       (float) delta / 1000.0,
			       mbps);
		}
		else if (MatchCmd(args, "plain", 0))
		{
			flags |= SFS_FLAGS_PLAINTEXT_XFER;
		}
#endif // PERF_TEST
		else
		{
			printf("Commands:\n");
			printf("  search URL STRING\n");
			printf("  get ID PATH\n");
			printf("  share [DIR]\n");
			printf("  wait\n");
			printf("  port PORT\n");
			printf("  proxy URL\n");
			printf("  quit\n");
#ifdef PERF_TEST
			printf("\nPerformace testing:\n");
			printf("  quiet 0|1\n");
			printf("  mark\n");
			printf("  time\n");
			printf("  plain\n");
#endif // PERF_TEST
		}
	}

	DestroyKeys(&keys);
	DestroyResponses(&resps);
}

int
main(int argc, char **argv)
{
#ifdef WIN32
	prog = strrchr(argv[0], '\\');
#else
	prog = strrchr(argv[0], '/');
#endif
	prog = prog ? (prog + 1):argv[0];

	char *s = new char[23];
	memset(s, 0x34, 23);

#ifdef WIN32
        PTP::Store store(HKEY_CURRENT_USER, "Software\\PTL\\Cert", NULL, NULL);
#else
        char path[2048];
        sprintf(path, "%s/.ptl/cert", getenv("HOME"));
        PTP::Store store(path, NULL, NULL);
#endif

	if (store.Load() || !store.Find(NULL, 1))
        {
                printf("%s: cannot load certificates.\n", prog);
                return 1;
        }
	
	if (argc <= 2)
	{
		FILE *in = (argc > 1) ? fopen(argv[1], "rb"):stdin;
		if (in)
		{
			HandleCmds(&store, in);
			if (in != stdin)
				fclose(in);
		}
	}
	else
		HandleOpts(&store, argc, argv);

	return 0;
}
