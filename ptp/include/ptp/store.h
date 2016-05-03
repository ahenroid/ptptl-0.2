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

#ifndef __PTP_STORE_H__
#define __PTP_STORE_H__

#ifdef WIN32
#define __WINCRYPT_H__
#include <windows.h>
#endif
#include <openssl/bio.h>
#include <ptp/ptp.h>
#include <ptp/list.h>
#include <ptp/id.h>
#include <ptp/key.h>

#ifndef WIN32
typedef void *HKEY;
#endif

/**
 * PTP::Store: Secure storage (PKCS#12, PKCS#7, and PEM support).
 * Synopsis: #include <ptp/store.h>
 */
class EXPORT PTP::Store
{
public:
	enum Type
	{
		/**
		 * PTP::Store::ALL: Any archive entry.
		 */
		ALL,

		/**
		 * PTP::Store::IDENTITY: A certificate.
		 */
		IDENTITY,

		/**
		 * PTP::Store::KEY: A symmetric key.
		 */
		KEY,

		/**
		 * PTP::Store::SECRET: Secret data.
		 */
		SECRET
	};
	
	struct Entry:public PTP::List::Entry
	{
		Type type;
		union
		{
			struct
			{
				PTP::Identity *ident;
				int exportkey;
			} ident;
			struct
			{
				BYTE data[PTP::Key::KEY_SIZE];
			} key;
			struct
			{
				BYTE *data;
				int size;
			} secret;
		};
		char *friendly;
		BYTE *id;
		int idsize;
	};

	Store();
	Store(const char *path,
	      const char *passwd,
	      const char *macpasswd);
#ifdef WIN32
	Store(HKEY key,
	      const char *path,
	      const char *passwd,
	      const char *macpasswd);
#endif // WIN32
	~Store();

	int Load();
	int Save();
	void Reset(int remove = 1);

	int Insert(const PTP::Identity *ident,
		   int exportkey,
		   const char *friendly = NULL,
		   const BYTE *id = NULL,
		   int idsize = 0);
	int Remove(const PTP::Identity *ident);

	int Insert(const PTP::Key *key, const BYTE *id = NULL, int idsize = 0);
	int Remove(const PTP::Key *key);

	int Insert(const BYTE *secret,
		   int size,
		   const char *friendly = NULL,
		   const BYTE *id = NULL,
		   int idsize = 0);
	int Remove(const BYTE *secret, int size);

	const Entry *Find(Type type = ALL,
			  const char *friendly = NULL,
			  const BYTE *id = NULL,
			  int idsize = 0,
			  const Entry *from = NULL);
	PTP::Identity *Find(const char *name = NULL,
			    int haskey = 0,
			    const BYTE *modulus = NULL,
			    PTP::Identity *from = NULL);
	
	static PTP::Identity *Import(
		BYTE *data,
		int size,
		const char *passwd,
		const char *macpasswd);
	static int Export(
		const PTP::Identity *id,
		int exportkey,
		const char *passwd,
		const char *macpasswd,
		BYTE *data);

	static PTP::Identity *ImportPEM(BYTE *data, int size);
	static int ExportPEM(const PTP::Identity *ident, BYTE *data);

	static int ImportEnvelope(
		const BYTE *envelope,
		int size,
		BYTE *data,
		const PTP::Identity *recipient,
		const PTP::Identity *signer);
	static int ExportEnvelope(
		const BYTE *data,
		int size,
		BYTE *envelope,
		const PTP::Identity *recipient,
		const PTP::Identity *signer);

protected:
	Store(const Store& store);
	Store &operator=(const Store& store);

	static int Import(BIO *bio,
			  const char *passwd,
			  const char *macpasswd,
			  PTP::List *list);
	static int Export(PTP::List *list,
			  const char *passwd,
			  const char *macpasswd,
			  BIO *bio);

	static void Insert(PTP::List *list,
			   PTP::Identity *ident,
			   int exportkey,
			   const char *friendly,
			   const BYTE *id,
			   int idsize);
	static void Insert(PTP::List *list,
			   const BYTE *data,
			   int size,
			   const char *friendly,
			   const BYTE *id,
			   int idsize);
	static void Destroy(PTP::List *list);

	HKEY m_key;
	char *m_path;
	char *m_passwd;
	char *m_macpasswd;
	PTP::List m_entries;
};

#endif // __PTP_STORE_H__
