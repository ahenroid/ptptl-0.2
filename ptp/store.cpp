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

#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <assert.h>
#include <openssl/bio.h>
#include <openssl/pkcs12.h>
#include <openssl/pkcs7.h>
#include <openssl/pem.h>
#include <ptp/store.h>
#include <ptp/debug.h>

#define PTP_STORE_KEY_FRIENDLY ".KEYDATA."

/**
 * PTP::Store::Store: Create an in-memory store.
 */
PTP::Store::Store()
	:m_key(NULL), m_path(NULL), m_passwd(NULL), m_macpasswd(NULL)
{
}

/**
 * PTP::Store::Store: Create a file store.
 * @path: File pathname.
 * @passwd: Archive password.
 * @macpasswd: MAC password.
 * Example:
 *   char *passwd = ...;
 *   PTP::Store store("/home/johndoe/.ptl/certs", passwd, passwd);
 *   ...
 */
PTP::Store::Store(const char *path,
		  const char *passwd,
		  const char *macpasswd)
	:m_key(NULL)
{
	m_path = path ? strdup(path):NULL;
	m_passwd = passwd ? strdup(passwd):NULL;
	m_macpasswd = macpasswd ? strdup(macpasswd):NULL;
}

#ifdef WIN32

/**
 * PTP::Store::Store: Create a registry store.
 * @key: Base registry key.
 * @name: Key name.
 * @passwd: Archive password.
 * @macpasswd: MAC password.
 * Notes: This method is only available on the Win32 platform.
 * Example:
 *   char *passwd = ...;
 *   PTP::Store store(HKEY_CURRENT_USER,
 *                    "Software\\PTL\\Certs",
 *                    passwd,
 *                    passwd);
 *   ...
 */
PTP::Store::Store(HKEY key,
		  const char *name,
		  const char *passwd,
		  const char *macpasswd)
	:m_key(key)
{
	m_path = name ? strdup(name):NULL;
	m_passwd = passwd ? strdup(passwd):NULL;
	m_macpasswd = macpasswd ? strdup(macpasswd):NULL;
}

#endif // WIN32

#ifdef PTPTL_DLL

/*
 * PTP::Store::Store: Copy constructor.
 * @store: Source Store.
 */
PTP::Store::Store(const Store& store)
{
	assert(0);
}

/*
 * PTP::Store::operator=: Copy constructor.
 * @store: Source Store.
 */
PTP::Store&
PTP::Store::operator=(const Store& store)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL


/**
 * PTP::Store::~Store: Class destructor.
 * Notes: The destructor does not save the archive to the storage
 *        medium. &Save must be called before destruction to save
 *        the contents of the archive.
 */
PTP::Store::~Store()
{
	Destroy(&m_entries);
	delete [] m_macpasswd;
	delete [] m_passwd;
	delete [] m_path;
}

/**
 * PTP::Store::Load: Load archive from the storage medium.
 * Returns: 0 on success or -1 on error (the archive failed to open or
 *          the archive is invalid).
 */
int
PTP::Store::Load()
{
	if (!m_path)
		return -1;

	Destroy(&m_entries);

	BYTE *buffer = NULL;
	BIO *bio = NULL;
	if (!m_key)
		bio = BIO_new_file(m_path, "rb");
	else
	{
#ifdef WIN32
		HKEY key;
		if (RegOpenKeyEx(m_key, m_path, 0, KEY_ALL_ACCESS, &key)
		    == ERROR_SUCCESS)
		{
			DWORD size = 0;
			if (RegQueryValueEx(key, NULL, 0, NULL, NULL, &size)
			    == ERROR_SUCCESS)
			{
				buffer = new BYTE[size];
				RegQueryValueEx(key, NULL, 0,
						NULL, buffer,
						&size);
			}
			RegCloseKey(key);
			bio = BIO_new_mem_buf((void*) buffer, size);
		}
#endif
	}

	int status = -1;
	if (bio)
	{
		status = Import(bio, m_passwd, m_macpasswd, &m_entries);
		BIO_free(bio);
	}
	delete [] buffer;
	return status;
}

/**
 * PTP::Store::Save: Save archive to the storage medium.
 * Returns: 0 on success or -1 on error.
 */
int
PTP::Store::Save()
{
	if (!m_path)
		return -1;

	BIO *bio = m_key ? BIO_new(BIO_s_mem()):BIO_new_file(m_path, "wb");;
	if (!bio)
		return -1;

	int status = Export(&m_entries, m_passwd, m_macpasswd, bio);
#ifdef WIN32
	if (!status && m_key)
	{
		RegDeleteKey(m_key, m_path);
		
		HKEY key;
		if (RegCreateKeyEx(m_key,
				   m_path,
				   0,
				   NULL,
				   REG_OPTION_NON_VOLATILE,
				   KEY_ALL_ACCESS,
				   NULL,
				   &key,
				   NULL) == ERROR_SUCCESS)
		{
			BUF_MEM *buf;
			BIO_get_mem_ptr(bio, &buf);
			RegSetValueEx(key, NULL, 0, REG_BINARY,
				      (BYTE*) buf->data, buf->length);
			RegCloseKey(key);
		}
	}
#endif // WIN32
	BIO_free(bio);
	return status;
}

/**
 * PTP::Store::Reset: Clear entries and, optionally, remove the archive.
 * @remove: 1 to remove the archive from its storage medium.
 */
void
PTP::Store::Reset(int remove)
{
	Destroy(&m_entries);
	if (remove && m_path)
	{
		if (!m_key)
			unlink(m_path);
#ifdef WIN32
		else
			RegDeleteKey(m_key, m_path);
#endif
	}
}

/**
 * PTP::Store::Insert: Add a certificate to the archive.
 * @ident: Certificate.
 * @exportkey: 1 to export the private key with the certificate.
 * @friendly: Friendly name.
 * @id: Identifier value.
 * @idsize: Identifier size or -1 if @id is a string.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Store store;
 *   store.$Insert(new PTP::Identity("John Doe"),
 *                0,
 *                "John Doe",
 *                NULL,
 *                0);
 */
int
PTP::Store::Insert(const PTP::Identity *ident,
		   int exportkey,
		   const char *friendly,
		   const BYTE *id,
		   int idsize)
{
	if (!ident)
		return -1;

	PTP::Identity *copy = new PTP::Identity(*ident);
	if (!exportkey)
		copy->DestroyKey();
	Insert(&m_entries, copy, exportkey, friendly, id, idsize);
	return 0;
}

/**
 * PTP::Store::Remove: Remove a certificate from the archive.
 * @ident: Certificate.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Store store;
 *   ...
 *   PTP::Identity *id = store.Find("John Doe", 0, NULL, NULL);
 *   store.$Remove(id);
 */
int
PTP::Store::Remove(const PTP::Identity *ident)
{
	if (!ident)
		return -1;

	BYTE modulus[PTP::Identity::KEY_SIZE];
	ident->GetKey(modulus);

	Entry *entry = NULL;
	m_entries.Lock();
	PTP_LIST_FOREACH(Entry, entry, &m_entries)
	{
		if (entry->type == IDENTITY)
		{
			if (entry->ident.ident == ident)
				break;
			BYTE mod[PTP::Identity::KEY_SIZE];
			entry->ident.ident->GetKey(mod);
			if (memcmp(mod, modulus, sizeof(mod)) == 0)
				break;
		}
	}
	if (entry)
	{
		m_entries.Remove(entry, 0);
		delete entry->ident.ident;
		delete [] entry->friendly;
		delete [] entry->id;
		delete entry;
	}
	m_entries.Unlock();
	return entry ? 0:-1;
}

/**
 * PTP::Store::Insert: Add a key to the archive.
 * @key: Key.
 * @id: Identifier value.
 * @idsize: Identifier size or -1 if @id is a string.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Store store;
 *   store.$Insert(new PTP::Key, (const BYTE*) "Joe's key", -1);
 *   ...
 */
int
PTP::Store::Insert(const PTP::Key *key, const BYTE *id, int idsize)
{
	if (!key)
		return -1;
	Insert(&m_entries,
	       key->m_key,
	       sizeof(key->m_key),
	       PTP_STORE_KEY_FRIENDLY,
	       id,
	       idsize);
	return 0;
}

/**
 * PTP::Store::Remove: Remove a key from the archive.
 * @key: Key.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Key *key = ...;
 *   PTP::Store store;
 *   store.Insert(key, NULL, 0);
 *   store.$Remove(key);
 *   ...
 */
int
PTP::Store::Remove(const PTP::Key *key)
{
	if (!key)
		return -1;

	Entry *entry = NULL;
	m_entries.Lock();
	PTP_LIST_FOREACH(Entry, entry, &m_entries)
	{
		if (entry->type == KEY
		    && memcmp(key->m_key,
			      entry->key.data,
			      sizeof(key->m_key)) == 0)
		{
			m_entries.Remove(entry, 0);
			memset(entry->key.data, 0, sizeof(entry->key.data));
			delete [] entry->friendly;
			delete [] entry->id;
			delete entry;
			break;
		}
	}
	m_entries.Unlock();
	return entry ? 0:-1;
}

/**
 * PTP::Store::Insert: Add secret data to the archive.
 * @secret: Secret data.
 * @size: Data size or -1 if @secret is a string.
 * @friendly: Friendly name.
 * @id: Identifier value.
 * @idsize: Identifier size or -1 if @id is a string.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Store store;
 *   store.$Insert((const BYTE*) "This is my secret...",
 *                -1,
 *                "Secret",
 *                NULL,
 *                0);
 *   ...
 */
int
PTP::Store::Insert(const BYTE *secret,
		   int size,
		   const char *friendly,
		   const BYTE *id,
		   int idsize)
{
	if (!secret)
		return -1;
	Insert(&m_entries, secret, size, friendly, id, idsize);
	return 0;
}

/**
 * PTP::Store::Remove: Remove secret data from the archive.
 * @secret: Secret data.
 * @size: Data size or -1 if @secret is a string.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Store store;
 *   store.Insert((const BYTE*) "Secret", -1, NULL, NULL, 0);
 *   store.$Remove((const BYTE*) "Secret", -1);
 *   ...
 */
int
PTP::Store::Remove(const BYTE *secret, int size)
{
	if (size == -1)
		size = strlen((const char*) secret);

	Entry *entry = NULL;
	m_entries.Lock();
	PTP_LIST_FOREACH(Entry, entry, &m_entries)
	{
		if (entry->type == SECRET
		    && entry->secret.size == size
		    && memcmp(entry->secret.data, secret, size) == 0)
		{
			m_entries.Remove(entry, 0);
			memset(entry->secret.data, 0, entry->secret.size);
			delete [] entry->secret.data;
			delete [] entry->friendly;
			delete [] entry->id;
			delete entry;
			break;
		}
	}
	m_entries.Unlock();
	return entry ? 0:-1;
}

/**
 * PTP::Store::Find: Locate an archive entry.
 * @type: Entry type (%IDENTITY, %KEY, %SECRET, or %ALL).
 * @friendly: Friendly name or NULL to match any.
 * @id: Identifier value or NULL to match any.
 * @idsize: Identifier size or 0.
 * @from: Previous find result or NULL to begin at the start.
 * Returns: Matching entry or NULL if none found.
 * Example:
 *   PTP::Store store;
 *   ...
 *   PTP::Store::Entry *entry = NULL;
 *   for (;;)
 *   {
 *       entry = store.$Find(PTP::Store::IDENTITY,
 *                          "John Doe",
 *                          NULL,
 *                          0,
 *                          entry);
 *       if (!entry)
 *           break;
 *       printf("%s\n", entry->ident.ident->GetName());
 *   }
 */
const PTP::Store::Entry *
PTP::Store::Find(Type type,
		 const char *friendly,
		 const BYTE *id,
		 int idsize,
		 const Entry *from)
{
	if (id && idsize == -1)
		idsize = strlen((const char*) id);

	Entry *entry = NULL;
	m_entries.Lock();
	PTP_LIST_FOREACH(Entry, entry, &m_entries)
	{
		if (!from
		    && (type == ALL || type == entry->type)
		    && (!friendly
			|| (entry->friendly
			    && strcmp(friendly, entry->friendly) == 0))
		    && (!id
			|| (entry->idsize == idsize
			    && memcmp(id, entry->id, idsize) == 0)))
			break;
		else if (entry == from)
			from = NULL;
	}
	m_entries.Unlock();
	return entry;
}

/**
 * PTP::Store::Find: Locate a certificate.
 * @name: Certificate name or NULL to match any.
 * @haskey: 1 to only match certificates with a private key.
 * @modulus: Public key modulus or NULL to match any.
 * @from: Previous find result or NULL to begin at the start.
 * Returns: Matching certificate or NULL if none found.
 * Example:
 *   PTP::Store store;
 *   ...
 *   PTP::Identity *id = NULL;
 *   for (;;)
 *   {
 *       id = store.$Find(NULL, 1, NULL, id);
 *       if (!id)
 *           break;
 *       printf("%s\n", id->GetName());
 *   }
 */
PTP::Identity *
PTP::Store::Find(const char *name,
		 int haskey,
		 const BYTE *modulus,
		 PTP::Identity *from)
{
	Entry *entry = NULL;
	m_entries.Lock();
	PTP_LIST_FOREACH(Entry, entry, &m_entries)
	{
		if (entry->type == IDENTITY)
		{
			if (!from
			    && (!name
				|| strcmp(name,
					  entry->ident.ident->GetName()) == 0)
			    && (!haskey || entry->ident.ident->m_key))
			{
				if (!modulus)
					break;
				BYTE mod[PTP::Identity::KEY_SIZE];
				entry->ident.ident->GetKey(mod);
				if (memcmp(mod, modulus, sizeof(mod)) == 0)
					break;
			}
			else if (entry->ident.ident == from)
				from = NULL;
		}
	}
	PTP::Identity *ident = entry ? entry->ident.ident:NULL;
	m_entries.Unlock();
	return ident;
}

/**
 * PTP::Store::Import: Import a certificate in PKCS#12 format.
 * Type: static
 * @data: Certificate data (PKCS#12).
 * @size: Data size.
 * @passwd: Archive password or NULL.
 * @macpasswd: MAC password or NULL.
 * Returns: Certificate on success or NULL on error.
 * Notes: &Import imports the first certificate and private key found
 *        found in the PKCS#12 data.  It does not process
 *        nested (ie. SafeContents) bags.  The certificate should be
 *        in a top-level CertBag and a private key can be in either a
 *        PKCS#8 ShroudedKeyBag (if @passwd is non-NULL) or in a
 *        KeyBag (if @passwd is NULL).
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE data[...];
 *   int size = PTP::Store::Export(id, 1, passwd, passwd, data);
 *   PTP::Identity *copy = $PTP::Store::Import(data, size, passwd, passwd);
 *   ...
 */
PTP::Identity *
PTP::Store::Import(BYTE *data,
		   int size,
		   const char *passwd,
		   const char *macpasswd)
{
	BIO *bio = BIO_new_mem_buf((void*) data, size);
	if (!bio)
		return NULL;

	PTP::List list(0);
	int status = Import(bio, passwd, macpasswd, &list);
	BIO_free(bio);
	
	PTP::Identity *ident = NULL;
	if (!status)
	{
		Entry *entry = (Entry*) list.GetHead();
		if (entry)
		{
			ident = entry->ident.ident;
			entry->ident.ident = NULL;
		}
	}
	Destroy(&list);

	return ident;
}

/**
 * PTP::Store::Export: Export certificate data in PKCS#12 format.
 * Type: static
 * @ident: Certificate.
 * @exportkey: 1 to also export the private key.
 * @passwd: Archive password or NULL.
 * @macpasswd: MAC password or NULL.
 * @data: [$OUT] Certificate data (PKCS#12) or NULL.
 * Returns: Data size on success or -1 on error.
 * Notes: The certificate is exported as a PKCS#12 CertBag and the
 *        key, if it is exported, is exported as either a PKCS#8
 *        ShroudedKeyBag (if @passwd is non-NULL) or as a unshrouded
 *        KeyBag (if @passwd is NULL).  All of the PKCS#12 data is
 *        enclosed with a message authentication code based on
 *        @macpasswd.
 * Example:
 *   PTP::Identity *id = ...;
 *   int size = $PTP::Store::Export(id, 1, passwd, passwd, NULL);
 *   if (size <= 0)
 *       return -1;
 *   BYTE *data = new BYTE[size];
 *   $PTP::Store::Export(id, 1, passwd, passwd, data);
 *   ...
 */
int
PTP::Store::Export(const PTP::Identity *ident,
		   int exportkey,
		   const char *passwd,
		   const char *macpasswd,
		   BYTE *data)
{
	if (!ident)
		return -1;

	BIO *bio = BIO_new(BIO_s_mem());
	if (!bio)
		return -1;

	PTP::Identity id(*ident);
	PTP::List list(0);
	Insert(&list, &id, exportkey, NULL, NULL, 0);

	Export(&list, passwd, macpasswd, bio);
	BUF_MEM *buf;
	BIO_get_mem_ptr(bio, &buf);
	int size = buf->length;
	if (size > 0 && data)
		memcpy(data, buf->data, size);
	BIO_free(bio);
		
	Entry *entry = (Entry*) list.GetHead();
	if (entry && entry->type == IDENTITY)
		entry->ident.ident = NULL;
	Destroy(&list);

	return ((size > 0) ? size:-1);
}

/**
 * PTP::Store::ImportPEM: Import a certificate in PEM format.
 * Type: static
 * @data: Certificate data (PEM).
 * @size: Data size.
 * Returns: Certificate on success or NULL on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE data[...];
 *   int size = PTP::Store::ExportPEM(id, data);
 *   PTP::Identity *copy = $PTP::Store::ImportPEM(data, size);
 *   ...
 */
PTP::Identity *
PTP::Store::ImportPEM(BYTE *data, int size)
{
	BIO *bio = BIO_new_mem_buf((void*) data, size);
	if (!bio)
		return NULL;
	X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (!cert)
		return NULL;
	return new PTP::Identity(cert);
}

/**
 * PTP::Store::ExportPEM: Export certificate data in PEM format.
 * Type: static
 * @ident: Certificate.
 * @data: [$OUT] Certificate data (PEM) or NULL.
 * Returns: Data size on success or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   int size = $PTP::Store::ExportPEM(id, NULL);
 *   if (size <= 0)
 *       return -1;
 *   BYTE *data = new BYTE[size];
 *   $PTP::Store::ExportPEM(id, data);
 *   ...
 */
int
PTP::Store::ExportPEM(const PTP::Identity *ident, BYTE *data)
{
	if (!ident)
		return -1;

	BIO *bio = BIO_new(BIO_s_mem());
	if (!bio)
		return -1;

	PEM_write_bio_X509(bio, ident->m_cert);
	BUF_MEM *buf;
	BIO_get_mem_ptr(bio, &buf);
	int size = buf->length;
	if (size > 0 && data)
		memcpy(data, buf->data, size);
	BIO_free(bio);

	return ((size > 0) ? size:-1);
}

/**
 * PTP::Store::ImportEnvelope: Import PKCS#7 enveloped data.
 * Type: static
 * @envelope: Envelope (PKCS#7).
 * @size: Envelope size.
 * @data: [$OUT] Enveloped data or NULL.
 * @recipient: Recipient identity.
 * @signer: Signer identity or NULL.
 * Returns: Enveloped data size or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   char *secret = "This is a secret...";
 *   BYTE envelope[...];
 *   BYTE data[..];
 *   int size
 *       = PTP::Store::ExportEnvelope((BYTE*) secret, -1, envelope, id, NULL);
 *   int datasize
 *       = $PTP::Store::ImportEnvelope(envelope, size, data, id, NULL);
 *   if (datasize <= 0)
 *       return -1;
 */
int
PTP::Store::ImportEnvelope(
	const BYTE *envelope,
	int size,
	BYTE *data,
	const PTP::Identity *recipient,
	const PTP::Identity *signer)
{
	if (!envelope || size <= 0 || !recipient || !recipient->m_key)
		return -1;

	BIO *bio = BIO_new_mem_buf((void*) envelope, size);
	if (!bio)
		return -1;

	PKCS7 *pkcs7 = d2i_PKCS7_bio(bio, NULL);
	BIO_free(bio);
	if (!pkcs7)
		return -1;

	bio = PKCS7_dataDecode(pkcs7,
			       recipient->m_key,
			       NULL,
			       recipient->m_cert);
	BIO *bio2 = BIO_new(BIO_s_mem());
	if (!bio || !bio2)
	{
		BIO_free(bio2);
		BIO_free(bio);
		PKCS7_free(pkcs7);
		return -1;
	}

	for (;;)
	{
		char buffer[256];
		int s = BIO_read(bio, buffer, sizeof(buffer));
		if (s <= 0)
			break;
		BIO_write(bio2, buffer, s);
	}

	if (signer)
	{
		STACK_OF(PKCS7_SIGNER_INFO) *info
			= PKCS7_get_signer_info(pkcs7);
		if (info)
		{
			PKCS7_SIGNER_INFO *si
				= sk_PKCS7_SIGNER_INFO_value(info, 0);
			if (!si  || PKCS7_signatureVerify(bio,
							  pkcs7,
							  si,
							  signer->m_cert) <= 0)
				size = -1;
		}
		else
			size = -1;
	}
	BIO_free(bio);
	PKCS7_free(pkcs7);

	if (size > 0)
	{
		BUF_MEM *buf;
		BIO_get_mem_ptr(bio2, &buf);
		size = buf->length;
		if (size > 0 && data)
			memcpy(data, buf->data, size);
		BIO_free(bio2);
	}

	return ((size > 0) ? size:-1);
}

/**
 * PTP::Store::ExportEnvelope: Export PKCS#7 enveloped data.
 * Type: static
 * @data: Enveloped data.
 * @size: Data size.
 * @envelope: [$OUT] Envelope (PKCS#7) or NULL.
 * @recipient: Recipient identity.
 * @signer: Signer identity or NULL.
 * Returns: Envelope size or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   char *secret = "This is a secret.";
 *   int size
 *       = $PTP::Store::ExportEnvelope((BYTE*) secret, -1, NULL, id, NULL);
 *   if (size <= 0)
 *       return -1;
 *   BYTE *envelope = new BYTE[size];
 *   $PTP::Store::ExportEnvelope((BYTE*) secret, -1, envelope, id, NULL);
 */
int
PTP::Store::ExportEnvelope(
	const BYTE *data,
	int size,
	BYTE *envelope,
	const PTP::Identity *recipient,
	const PTP::Identity *signer)
{
	if (!data || (signer && !signer->m_key))
		return -1;
	if (size == -1)
		size = strlen((const char*) data);

	PKCS7 *pkcs7 = PKCS7_new();
	if (!pkcs7)
		return -1;
	if (signer)
	{
		PKCS7_set_type(pkcs7, NID_pkcs7_signedAndEnveloped);
		PKCS7_add_signature(pkcs7,
				    signer->m_cert,
				    signer->m_key,
				    PTP_DIGEST);
	}
	else
		PKCS7_set_type(pkcs7, NID_pkcs7_enveloped);
	PKCS7_set_cipher(pkcs7, EVP_des_ede3_cbc());
	PKCS7_add_recipient(pkcs7, recipient->m_cert);

	BIO *bio = PKCS7_dataInit(pkcs7, NULL);
	if (!bio)
	{
		PKCS7_free(pkcs7);
		return -1;
	}
	BIO_write(bio, data, size);
	BIO_flush(bio);
	PKCS7_dataFinal(pkcs7, bio);
	BIO_free(bio);

	bio = BIO_new(BIO_s_mem());
	if (!bio)
	{
		PKCS7_free(pkcs7);
		return -1;
	}
	i2d_PKCS7_bio(bio, pkcs7);
	PKCS7_free(pkcs7);

	BUF_MEM *buf;
	BIO_get_mem_ptr(bio, &buf);
	size = buf->length;
	if (size > 0 && envelope)
		memcpy(envelope, buf->data, size);
	BIO_free(bio);

	return ((size > 0) ? size:-1);
}

/*
 * PTP::Store::Import: Import certificates from I/O source in PKCS#12 format.
 * Type: static
 * @bio: I/O source.
 * @passwd: Archive password or NULL.
 * @macpasswd: MAC password or NULL.
 * @list: [$OUT] List of certificates.
 * Returns: 0 on success or -1 on error.
 */
int
PTP::Store::Import(BIO *bio,
		   const char *passwd,
		   const char *macpasswd,
		   PTP::List *list)
{
	if (!bio)
		return -1;
	PKCS12 *pkcs12 = d2i_PKCS12_bio(bio, NULL);
	if (!pkcs12)
		return -1;
	
	// check MAC
	if ((macpasswd && !PKCS12_verify_mac(pkcs12, macpasswd, -1))
	    || (!macpasswd && !PKCS12_verify_mac(pkcs12, NULL, 0)))
	{
		PKCS12_free(pkcs12);
		return -1;
	}

	STACK_OF(PKCS7) *safes = M_PKCS12_unpack_authsafes(pkcs12);
	for (int i = 0; i < sk_PKCS7_num(safes); i++)
	{
		// fetch auth safe
		PKCS7 *safe = sk_PKCS7_value(safes, i);
		STACK_OF(PKCS12_SAFEBAG) *bags = NULL;
		switch (OBJ_obj2nid(safe->type))
		{
		case NID_pkcs7_data:
			bags = M_PKCS12_unpack_p7data(safe);
			break;
		case NID_pkcs7_encrypted:
			bags = M_PKCS12_unpack_p7encdata(safe, passwd, -1);
			break;
		}
		if (!bags)
			continue;

		X509 *cert = NULL;
		const char *friendly = NULL;
		ASN1_OCTET_STRING *id = NULL;

		// fetch certificate bags, key bags, and secret bags
		for (int j = 0; j < sk_PKCS12_SAFEBAG_num(bags); j++)
		{
			PKCS12_SAFEBAG *bag = sk_PKCS12_SAFEBAG_value(bags, j);
			if (!bag)
				break;

			EVP_PKEY *key = NULL;
			PKCS8_PRIV_KEY_INFO *pkcs8;
			ASN1_TYPE *attrib;
			switch (M_PKCS12_bag_type(bag))
			{
			case NID_keyBag:
				key = EVP_PKCS82PKEY(bag->value.keybag);
				break;
			case NID_pkcs8ShroudedKeyBag:
				pkcs8 = M_PKCS12_decrypt_skey(bag, passwd, -1);
				if (!pkcs8)
					break;
				key = EVP_PKCS82PKEY(pkcs8);
				PKCS8_PRIV_KEY_INFO_free(pkcs8);
				break;
			default:
				friendly = PKCS12_get_friendlyname(bag);
				attrib = PKCS12_get_attr(bag, NID_localKeyID);
				id = attrib ? attrib->value.octet_string:NULL;
				break;
			}

			if (cert)
			{
				Insert(list,
				       new PTP::Identity(cert, key),
				       (key != NULL),
				       friendly,
				       id ? id->data:NULL,
				       id ? id->length:0);
				cert = NULL;
				key = NULL;
			}
			EVP_PKEY_free(key);
			key = NULL;
		       
			ASN1_OCTET_STRING *secret;
			switch (M_PKCS12_bag_type(bag))
			{
			case NID_certBag:
				cert = M_PKCS12_certbag2x509(bag);
				break;
			case NID_secretBag:
				secret = bag->value.bag->value.other
					->value.octet_string;
				if (secret)
				{
					Insert(list,
					       secret->data,
					       secret->length,
					       friendly,
					       id ? id->data:NULL,
					       id ? id->length:0);
				}
				break;
			}
		}

		if (cert)
		{
			Insert(list,
			       new PTP::Identity(cert),
			       0,
			       friendly,
			       id ? id->data:NULL,
			       id ? id->length:0);
		}

		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
	}

	sk_PKCS7_pop_free(safes, PKCS7_free);
	PKCS12_free(pkcs12);

	return 0;
}

/*
 * PTP::Store::Export: Export certificates to I/O sink in PKCS#12 format.
 * Type: static
 * @list: Certificates.
 * @passwd: Archive password or NULL.
 * @macpasswd: MAC password or NULL.
 * @bio: [$OUT] I/O sink.
 * Returns: 0 on sucess or -1 on error.
 */
int
PTP::Store::Export(PTP::List *list,
		   const char *passwd,
		   const char *macpasswd,
		   BIO *bio)
{
	STACK_OF(PKCS12_SAFEBAG) *bags = sk_PKCS12_SAFEBAG_new_null();

	// build safe bags
	Entry *entry = NULL;
	list->Lock();
	PTP_LIST_FOREACH(Entry, entry, list)
	{
		PKCS12_SAFEBAG *bag = NULL;
		switch (entry->type)
		{
		case IDENTITY:
#define i2d_X509 ((int (*)()) i2d_X509)
			bag = M_PKCS12_x5092certbag(
				entry->ident.ident->m_cert);
#undef i2d_X509
			break;
		case KEY:
			bag = PKCS12_SAFEBAG_new();
			bag->type = OBJ_nid2obj(NID_secretBag);
			bag->value.bag = PKCS12_BAGS_new();
			bag->value.bag->type = OBJ_nid2obj(NID_secretBag);
			bag->value.bag->value.other = ASN1_TYPE_new();
			ASN1_TYPE_set_octetstring(
				bag->value.bag->value.other,
				entry->key.data,
				sizeof(entry->key.data));
			break;
		case SECRET:
			bag = PKCS12_SAFEBAG_new();
			bag->type = OBJ_nid2obj(NID_secretBag);
			bag->value.bag = PKCS12_BAGS_new();
			bag->value.bag->type = OBJ_nid2obj(NID_secretBag);
			bag->value.bag->value.other = ASN1_TYPE_new();
			ASN1_TYPE_set_octetstring(
				bag->value.bag->value.other,
				entry->secret.data,
				entry->secret.size);
			break;
		case ALL:
			break;
		}

		if (!bag)
			continue;

		if (entry->friendly)
			PKCS12_add_friendlyname(bag, entry->friendly, -1);
		if (entry->id)
		{
			PKCS12_add_localkeyid(bag,
					      entry->id,
					      entry->idsize);
		}
		sk_PKCS12_SAFEBAG_push(bags, bag);

		if (entry->type == IDENTITY
		    && entry->ident.exportkey
		    && entry->ident.ident->m_key)
		{
			PKCS8_PRIV_KEY_INFO *pkcs8
				= EVP_PKEY2PKCS8(entry->ident.ident->m_key);
			bag = PKCS12_MAKE_KEYBAG(pkcs8);
			sk_PKCS12_SAFEBAG_push(bags, bag);
		}
	}
	list->Unlock();

	// build auth safe
	STACK_OF(PKCS7) *safes = sk_PKCS7_new_null();
	PKCS7 *safe = NULL;
	if (passwd)
	{
		safe = PKCS12_pack_p7encdata(
			NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
			passwd,	-1,
			NULL, 0,
			PKCS12_DEFAULT_ITER,
			bags);
	}
	else
		safe = PKCS12_pack_p7data(bags);
	sk_PKCS7_push(safes, safe);
	sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);

	// build PKCS12 data
	PKCS12 *pkcs12 = PKCS12_init(NID_pkcs7_data);
#define i2d_PKCS7 ((int (*)()) i2d_PKCS7)
	M_PKCS12_pack_authsafes(pkcs12, safes);
#undef i2d_PKCS7
	sk_PKCS7_pop_free(safes, PKCS7_free);

	// add MAC
	PKCS12_set_mac(pkcs12,
		       macpasswd, macpasswd ? -1:0,
		       NULL, 0,
		       PKCS12_DEFAULT_ITER,
		       PTP_DIGEST);

	i2d_PKCS12_bio(bio, pkcs12);
	PKCS12_free(pkcs12);

	return 0;
}

/*
 * PTP::Store::Insert: Insert a certificate into a list.
 * Type: static
 * @list: [$OUT] List.
 * @ident: Certificate.
 * @exportkey: 1 to export private key.
 * @friendly: Friendly name.
 * @id: Identifier value.
 * @idsize: Identifier size.
 */
void
PTP::Store::Insert(PTP::List *list,
		   PTP::Identity *ident,
		   int exportkey,
		   const char *friendly,
		   const BYTE *id,
		   int idsize)
{
	Entry *entry = new Entry;
	memset(entry, 0, sizeof(*entry));
	entry->type = IDENTITY;
	entry->ident.ident = ident;
	entry->ident.exportkey = exportkey;
	if (friendly)
		entry->friendly = strdup(friendly);
	if (id && idsize)
	{
		if (idsize == -1)
			idsize = strlen((const char*) id);
		entry->id = new BYTE[idsize + 1];
		memcpy(entry->id, id, idsize);
		entry->id[idsize] = '\0';
		entry->idsize = idsize;
	}
	list->Append(entry);
}

/*
 * PTP::Store::Insert: Insert a key or secret data into list.
 * Type: static
 * @list: [$OUT] List.
 * @data: Key or secret data.
 * @size: Data size.
 * @friendly: Friendly name.
 * @id: Identifier value.
 * @idsize: Identifier size.
 */
void
PTP::Store::Insert(PTP::List *list,
		   const BYTE *data,
		   int size,
		   const char *friendly,
		   const BYTE *id,
		   int idsize)
{
	Entry *entry = new Entry;
	memset(entry, 0, sizeof(*entry));

	if (friendly && strcmp(friendly, PTP_STORE_KEY_FRIENDLY) == 0)
	{
		entry->type = KEY;
		memset(entry->key.data, 0, sizeof(entry->key.data));
		if (size > (int) sizeof(entry->key.data))
			size = sizeof(entry->key.data);
		memcpy(entry->key.data, data, size);
	}
	else
	{
		entry->type = SECRET;
		if (size == -1)
			size = strlen((const char*) data);
		entry->secret.data = new BYTE[size + 1];
		memcpy(entry->secret.data, data, size);
		entry->secret.data[size] = '\0';
		entry->secret.size = size;
	}

	if (friendly)
		entry->friendly = strdup(friendly);
	if (id && idsize)
	{
		if (idsize == -1)
			idsize = strlen((const char*) id);
		entry->id = new BYTE[idsize + 1];
		memcpy(entry->id, id, idsize);
		entry->id[idsize] = '\0';
		entry->idsize = idsize;
	}
	list->Append(entry);
}

/*
 * PTP::Store::Destroy: Destroy a list of certificates.
 * Type: static
 * @list: Certificates.
 */
void
PTP::Store::Destroy(PTP::List *list)
{
	Entry *entry = NULL;
	list->Lock();
	PTP_LIST_FOREACH(Entry, entry, list)
	{
		list->Remove(entry, 0);
		switch (entry->type)
		{
		case IDENTITY:
			delete entry->ident.ident;
			break;
		case KEY:
			memset(entry->key.data, 0, sizeof(entry->key.data));
			break;
		case SECRET:
			memset(entry->secret.data, 0, entry->secret.size);
			delete [] entry->secret.data;
			break;
		case ALL:
			break;
		}
		delete [] entry->friendly;
		delete [] entry->id;
		delete entry;
	}
	list->Unlock();
}
