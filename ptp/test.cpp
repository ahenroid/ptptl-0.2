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

#include <stdio.h>
#include <string.h>
#include <ptp/id.h>
#include <ptp/store.h>
#include <ptp/auth.h>
#include <ptp/rand.h>
#include <ptp/key.h>
#include <ptp/thread.h>
#include <ptp/console.h>
#include <ptp/debug.h>

#define CHECK(arg) \
    ((void)((arg) \
     || !(printf("test: failure at %s:%d\n", __FILE__, __LINE__))))

static void
TestIdentity()
{
	PTP::Identity empty((const char*) NULL);
	const char *name = "John Doe";
	PTP::Identity id(name);
	CHECK(id.GetName() && !strcmp(id.GetName(), name));
	CHECK(id.GetIssuerName() && !strcmp(id.GetIssuerName(), name));
	const char *email = "john@doe.org";
	id.SetName(PTP::Identity::EMAIL_ADDRESS, email);
	char *value = id.GetName(PTP::Identity::EMAIL_ADDRESS);
	CHECK(value && !strcmp(value, email));
	delete [] value;
	email = "jane@doe.org";
	id.SetIssuerName(PTP::Identity::EMAIL_ADDRESS, email);
	value = id.GetIssuerName(PTP::Identity::EMAIL_ADDRESS);
	CHECK(value && !strcmp(value, email));
	delete [] value;
	BYTE data[PTP::Identity::KEY_SIZE];
	memset(data, 0, sizeof(data));
	id.GetKey(data);
	CHECK(data[0] != 0);
	BYTE plain[PTP::Identity::PLAINTEXT_SIZE];
	BYTE cipher[PTP::Identity::CIPHERTEXT_SIZE];
	memset(plain, 0, sizeof(plain));
	CHECK(id.Encrypt(NULL, sizeof(plain), NULL)
	      == PTP::Identity::CIPHERTEXT_SIZE);
	id.Encrypt(plain, sizeof(plain), cipher);
	CHECK(id.Decrypt(cipher, NULL) == sizeof(plain));
	id.Decrypt(cipher, plain);
	BYTE sign[PTP::Identity::SIGNATURE_SIZE];
	CHECK(id.Sign(plain, sizeof(plain), NULL) == sizeof(sign));
	id.Sign(plain, sizeof(plain), sign);
	CHECK(id.Verify(NULL, 0, NULL) == -1);
	CHECK(!id.Verify(plain, sizeof(plain), sign));
	CHECK(!id.Sign(&id, 60));
	CHECK(!id.Verify(&id));
	PTP::Key key;
	BYTE keydata[PTP::Identity::CIPHERTEXT_SIZE];
	CHECK(id.ExportKey(NULL, NULL) == sizeof(keydata));
	id.ExportKey(&key, keydata);
	PTP::Key *key2 = id.ImportKey(keydata, sizeof(keydata));
	BYTE k1[PTP::Key::KEY_SIZE], k2[PTP::Key::KEY_SIZE];
	key.Export(k1);
	key2->Export(k2);
	CHECK(!memcmp(k1, k2, sizeof(k1)));
	delete key2;
}

static void
TestStore()
{
	const char *passwd = "Passwd";
	const char *macpasswd = "MacPasswd";
	PTP::Store store("test.store", passwd, macpasswd);

	PTP::Identity id("John Doe");
	store.Insert(&id, 1, id.GetName(), (const BYTE*) "1234", -1);
	CHECK(!store.Remove(&id));
	CHECK(store.Remove(&id) == -1);
	PTP::Key key;
	store.Insert(&key, (const BYTE*) "5678", -1);
	CHECK(!store.Remove(&key));
	CHECK(store.Remove(&key) == -1);
	store.Insert((const BYTE*) "John", -1, "Doe", NULL, 0);
	CHECK(!store.Remove((const BYTE*) "John", -1));
	CHECK(store.Remove((const BYTE*) "John", -1) == -1);

	store.Insert(&id, 1, id.GetName(), (const BYTE*) "1234", -1);
	PTP::Identity *id2 = store.Find(id.GetName(), 1, NULL, NULL);
	CHECK(id2 && !strcmp(id.GetName(), id2->GetName()));
	BYTE mod[PTP::Identity::KEY_SIZE];
	id.GetKey(mod);
	id2 = store.Find(NULL, 0, mod, NULL);
	CHECK(id2 && !strcmp(id.GetName(), id2->GetName()));
	store.Remove(&id);
	CHECK(!store.Find(NULL, 0, mod, NULL));

	store.Insert(&id, 1, id.GetName(), NULL, 0);
	store.Save();
	store.Load();
	id2 = store.Find(NULL, 0, mod, NULL);
	CHECK(id2 && !strcmp(id.GetName(), id2->GetName()));
	store.Remove(&id);

	store.Insert(&id, 1, id.GetName(), NULL, 0);
	const PTP::Store::Entry *entry
		= store.Find(PTP::Store::IDENTITY,
			     id.GetName(),
			     NULL,
			     0,
			     NULL);
	CHECK(entry && !strcmp(id.GetName(), entry->ident.ident->GetName()));
	store.Reset(1);

	int size = PTP::Store::Export(&id, 1, passwd, macpasswd, NULL);
	CHECK(size > 0);
	BYTE *data = new BYTE[size];
	PTP::Store::Export(&id, 1, passwd, macpasswd, data);
	CHECK(!PTP::Store::Import(data, size, NULL, NULL));
	CHECK(!PTP::Store::Import(data, size, passwd, NULL));
	CHECK(!PTP::Store::Import(data, size, NULL, macpasswd));
	id2 = PTP::Store::Import(data, size, passwd, macpasswd);
	CHECK(id2 && !strcmp(id.GetName(), id2->GetName()));
	delete id2;
	delete [] data;

	size = PTP::Store::ExportPEM(&id, NULL);
	CHECK(size > 0);
	data = new BYTE[size];
	PTP::Store::ExportPEM(&id, data);
	id2 = PTP::Store::ImportPEM(data, size);
	CHECK(id2 && !strcmp(id.GetName(), id2->GetName()));
	delete id2;
	delete [] data;

	BYTE info[512];
	memset(info, 6, sizeof(info));
	size = PTP::Store::ExportEnvelope(info, sizeof(info), NULL, &id, &id);
	CHECK(size > 0);
	data = new BYTE[size];
	PTP::Store::ExportEnvelope(info, sizeof(info), data, &id, &id);
	size = PTP::Store::ImportEnvelope(data, size, data, &id, &id);
	CHECK(size == sizeof(info) && !memcmp(data, info, sizeof(info)));
	delete [] data;
}

static void
TestAuth()
{
	PTP::Store store;
	PTP::Identity id("John Doe");
	store.Insert(&id, 1, NULL, NULL, 0);
	PTP::Authenticator auth(&store);
	BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
	CHECK(auth.Challenge(&id, 60, (void*) 1, NULL) == sizeof(chal));
	auth.Challenge(&id, 60, (void*) 2, chal);
	BYTE resp[PTP::Authenticator::RESPONSE_SIZE];
	CHECK(auth.Respond(chal, NULL) == sizeof(resp));
	auth.Respond(chal, resp);
	CHECK(!auth.Verify(NULL));
	CHECK(auth.Verify(resp) == (void*) 2);
	CHECK(!auth.Verify(resp));
	PTP::Random::Fill(resp, sizeof(resp));
	CHECK(!auth.Verify(resp));
}

struct KeyContext
{
	const BYTE *read;
	const BYTE *readend;
	BYTE *write;
};

static int
KeyRead(BYTE *data, int size, void *context)
{
	KeyContext *ctx = (KeyContext*) context;
	int s = ctx->readend - ctx->read;
	if (s > size)
		s = size;
	if (s > 7)
		s = 7;
	if (s > 0)
		memcpy(data, ctx->read, s);
	ctx->read += s;
	return s;
}

static int
KeyWrite(const BYTE *data, int size, void *context)
{
	KeyContext *ctx = (KeyContext*) context;
	int s = size;
	if (s > 7)
		s = 7;
	memcpy(ctx->write, data, s);
	ctx->write += s;
	return s;
}

static void
TestKey()
{
	BYTE data[PTP::Key::KEY_SIZE];
	PTP::Random::Fill(data, sizeof(data));
	PTP::Key key(data);
	BYTE data2[PTP::Key::KEY_SIZE];
	CHECK(key.Export(data2) == PTP::Key::KEY_SIZE);
	CHECK(!memcmp(data, data2, sizeof(data)));
	
	BYTE plain[258];
	BYTE cipher[1024];
	PTP::Random::Fill(plain, sizeof(plain));

	int psize;
	for (psize = sizeof(plain) - 5;
	     psize <= (int) sizeof(plain); psize++)
	{
		int size = key.Encrypt(plain, psize, cipher);
		CHECK(size > (int) psize);
		size = key.Decrypt(cipher, size, cipher);
		CHECK(size == psize && !memcmp(plain, cipher, size));
	}

	for (psize = sizeof(plain) - 5;
	     psize <= (int) sizeof(plain); psize++)
	{
		int size = key.Encrypt(plain, psize, cipher, 0, 0);
		CHECK(size == (int) psize);
		size = key.Decrypt(cipher, size, cipher, 0, 0);
		CHECK(size == psize && !memcmp(plain, cipher, size));
	}

	for (psize = sizeof(plain) - 5;
	     psize <= (int) sizeof(plain); psize++)
	{
		KeyContext ctx;
		ctx.read = plain;
		ctx.readend = plain + psize;
		ctx.write = cipher;

		int size = key.Encrypt(KeyRead, KeyWrite, &ctx);
		CHECK(size > (int) psize);
		
		ctx.read = cipher;
		ctx.readend = cipher + size;
		ctx.write = cipher;

		size = key.Decrypt(KeyRead, KeyWrite, &ctx);
		CHECK(size == psize && !memcmp(plain, cipher, size));
	}

	for (psize = sizeof(plain) - 5;
	     psize <= (int) sizeof(plain); psize++)
	{
		KeyContext ctx;
		ctx.read = plain;
		ctx.readend = plain + psize;
		ctx.write = cipher;

		int size = key.Encrypt(KeyRead, KeyWrite, &ctx, 0, 0);
		CHECK(size == (int) psize);
		
		ctx.read = cipher;
		ctx.readend = cipher + size;
		ctx.write = cipher;

		size = key.Decrypt(KeyRead, KeyWrite, &ctx, 0, 0);
		CHECK(size == psize && !memcmp(plain, cipher, size));
	}
}

static void *
RunThread(void *context)
{
	int *value = (int*) context;
	(*value)++;
	PTP::Thread::Exit(3);
	return NULL;
}

static void *
RunThread2(void *context)
{
	int *value = (int*) context;
	(*value)++;
	return (void*) 4;
}

static void *
RunThread3(void *context)
{
	PTP::Thread::Sleep(10);
	return NULL;
}

static void
TestThread()
{
	PTP::Thread thread;
	int value = 1;
	CHECK(!thread.Start(RunThread, &value));
	CHECK(thread.Wait() == 3);
	CHECK(value == 2);
	CHECK(!thread.Start(RunThread2, &value));
	CHECK(thread.Wait() == 4);
	CHECK(value == 3);
	CHECK(!thread.Start(RunThread3, &value));
	CHECK(!thread.Kill());
	CHECK(thread.Wait());
}

struct MutexContext
{
	PTP::Mutex mutex;
	int value;
};

static void *
MutexThread(void *context)
{
	MutexContext *ctx = (MutexContext*) context;
	ctx->mutex.Lock();
	ctx->value /= 2;
	ctx->mutex.Unlock();
	return NULL;
}

static void
TestMutex()
{
	PTP::Mutex mutex;
	mutex.Lock();
	mutex.Unlock();
	CHECK(!mutex.TryLock());
	CHECK(!mutex.TryLock());
	mutex.Unlock();
	mutex.Unlock();

	PTP::Thread thread;
	MutexContext ctx;
	ctx.value = 3;
	ctx.mutex.Lock();
	CHECK(!thread.Start(MutexThread, &ctx));
	PTP::Thread::Sleep(2);
	ctx.value++;
	ctx.mutex.Unlock();
	CHECK(!thread.Wait());
	CHECK(ctx.value == 2);
}

struct ListContext:public PTP::List::Entry
{
	int value;
};

static void
TestList()
{
	ListContext entry;
	entry.value = 1;
	PTP::List list;
	list.Append(&entry);
	ListContext entry2;
	entry2.value = 2;
	list.Insert(&entry2);
	CHECK(list.GetHead() &&  ((ListContext*) list.GetHead())->value == 2);
	CHECK(list.GetTail() &&  ((ListContext*) list.GetTail())->value == 1);
	list.Remove(&entry2);
	list.Remove(&entry2);
	list.Remove(&entry2);
	CHECK(list.GetHead() &&  ((ListContext*) list.GetHead())->value == 1);
	list.Remove(&entry);
	CHECK(!list.GetHead() && !list.GetTail());

	ListContext *x;
	int count = 0;
	PTP_LIST_FOREACH(ListContext, x, &list)
		count++;
	CHECK(count == 0);
	list.Insert(&entry);
	list.Insert(&entry2);
	ListContext *y;
	PTP_LIST_FOREACH(ListContext, y, &list)
		count++;
	CHECK(count == 2);
}

int
main(int argc, char **argv)
{
	TestIdentity();
	TestStore();
	TestAuth();
	TestKey();

	TestThread();
	TestMutex();
	TestList();

	return 0;
}
