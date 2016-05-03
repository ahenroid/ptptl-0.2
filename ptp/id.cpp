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

#include <openssl/rsa.h>
#include <openssl/pkcs12.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>
#include <time.h>
#include <ptp/id.h>
#include <ptp/debug.h>

/*
 * CryptoInit: Initialize the OpenSSL library.
 */
class CryptoInit
{
public:
	CryptoInit();
	~CryptoInit();
};

static CryptoInit s_crypto;

/*
 * CryptoInit::CryptoInit: Initialize OpenSSL.
 */
CryptoInit::CryptoInit()
{
	CRYPTO_malloc_init();
	OpenSSL_add_all_algorithms();
}

/*
 * CryptoInit::~CryptoInit: Deinitialize OpenSSL.
 */
CryptoInit::~CryptoInit()
{
	ERR_remove_state(0);
	EVP_cleanup();
	ERR_free_strings();
}

/**
 * PTP::Identity::Identity: Class constructor.
 * @name: Subject common name or NULL.
 * Example:
 *   PTP::Identity john("John Doe");
 */
PTP::Identity::Identity(const char *name)
	:PTP::List::Entry(),
	 m_cert(NULL), m_key(NULL),
	 m_name(NULL), m_issuerName(NULL)
{
	if (!name)
		name = "*Unknown*";

	X509 *cert = X509_new();
	if (!cert)
		return;

	X509_set_version(cert, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(cert), 25L);

	EVP_PKEY *key = EVP_PKEY_new();
	if (!key)
	{
		X509_free(cert);
		return;
	}
	RSA *rsa = RSA_generate_key(KEY_SIZE << 3, KEY_EXPONENT, NULL, NULL);
	if (!rsa)
	{
		EVP_PKEY_free(key);
		X509_free(cert);
		return;
	}
	EVP_PKEY_assign_RSA(key, rsa);
	X509_set_pubkey(cert, key);

	m_cert = cert;
	m_key = key;
	SetName(COMMON_NAME, name);
	m_name = GetName(COMMON_NAME);

	// self-sign certificate
	Sign(this, EXPIRE_DEFAULT);
}

/*
 * PTP::Identity::Identity: Default copy constructor.
 * @ident: Source Identity.
 */
PTP::Identity::Identity(const Identity& ident)
	:PTP::List::Entry(),
	 m_cert(NULL), m_key(NULL),
	 m_name(NULL), m_issuerName(NULL)
{
	*this = ident;
}

/*
 * PTP::Identity::operator=: Copy constructor.
 * @ident: Source Identity.
 */
PTP::Identity&
PTP::Identity::operator=(const Identity& ident)
{
	delete [] m_issuerName;
	delete [] m_name;
	EVP_PKEY_free(m_key);
	X509_free(m_cert);

	m_cert = X509_dup(ident.m_cert);
	if (ident.m_key)
	{
		RSA *rsa = EVP_PKEY_get1_RSA(ident.m_key);
		m_key = EVP_PKEY_new();
		EVP_PKEY_assign_RSA(m_key, rsa);
	}
	m_name = GetName(COMMON_NAME);
	m_issuerName = GetIssuerName(COMMON_NAME);
	
	return *this;
}

/*
 * PTP::Identity::Identity: Class constructor.
 */
PTP::Identity::Identity(X509 *cert, EVP_PKEY *key)
	:PTP::List::Entry(), m_cert(cert), m_key(key)
{
	m_name = GetName(COMMON_NAME);
	m_issuerName = GetIssuerName(COMMON_NAME);
}

/**
 * PTP::Identity::~Identity: Class destructor.
 */
PTP::Identity::~Identity()
{
	delete [] m_issuerName;
	delete [] m_name;
	EVP_PKEY_free(m_key);
	X509_free(m_cert);
}

/**
 * PTP::Identity::GetName
 * Returns: Common name.
 */
const char *
PTP::Identity::GetName() const
{
	return m_name;
}

/**
 * PTP::Identity::GetIssuerName
 * Returns: Certificate issuer's common name.
 */
const char *
PTP::Identity::GetIssuerName() const
{
	return m_issuerName;
}

/**
 * PTP::Identity::GetName: Get subject name element.
 * @nid: Element identifier (%COMMON_NAME, %EMAIL_ADDRESS, ...).
 * Returns: Allocated name string or NULL on error.
 * Notes: It is the caller's responsibility to free the returned string.
 * Example:
 *   PTP::Identity *id = ...;
 *   char *org = id->$GetName(PTP::Identity::ORGANIZATION_NAME);
 *   if (org)
 *       printf("Organization: %s\n", org);
 *   delete [] org;
 */
char *
PTP::Identity::GetName(int nid) const
{
	if (!m_cert)
		return NULL;
	X509_NAME *name = X509_get_subject_name(m_cert);
	if (!name)
		return NULL;
	int size = X509_NAME_get_text_by_NID(name, nid, NULL, 0);
	if (size <= 0)
		return NULL;

	char *str = new char[size + 1];
	X509_NAME_get_text_by_NID(name, nid, str, size + 1);
	return str;
}

/**
 * PTP::Identity::SetName: Set subject name element.
 * @nid: Element identifier (%COMMON_NAME, %EMAIL_ADDRESS, ...).
 * Example:
 *   PTP::Identity *id = ...;
 *   id->$SetName(PTP::Identity::COUNTRY_NAME, "Canada");
 */
void
PTP::Identity::SetName(int nid, const char *value)
{
	if (!m_cert || !value)
		return;
	X509_NAME *name = X509_get_subject_name(m_cert);
	if (!name)
		return;
	X509_NAME_add_entry_by_NID(
		name, nid, MBSTRING_ASC, (BYTE*) value, -1, -1, 0);
}

/**
 * PTP::Identity::GetKey: Retrieve public key (RSA) modulus.
 * @data: [$OUT] Key modulus (%KEY_SIZE bytes).
 * Returns: %KEY_SIZE on success or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE modulus[PTP::Identity::KEY_SIZE];
 *   id->$GetKey(modulus);
 */
int
PTP::Identity::GetKey(BYTE *data) const
{
	if (!m_cert)
		return -1;
	RSA *key = EVP_PKEY_get1_RSA(X509_get_pubkey(m_cert));
	if (!key)
		return -1;
	if (data)
		BN_bn2bin(key->n, data);
	return KEY_SIZE;
}

/**
 * PTP::Identity::GetIssuerName: Get certificate issuer name element.
 * @nid: Element identifier (%COMMON_NAME, %EMAIL_ADDRESS, ...).
 * Returns: Allocated name string or NULL on error.
 * Notes: It is the caller's responsibility to free the returned string.
 * Example:
 *   PTP::Identity *id = ...;
 *   char *prov = id->$GetIssuerName(PTP::Identity::STATE_OR_PROVINCE_NAME);
 *   if (prov)
 *       printf("State/Province: %s\n", prov);
 *   delete [] prov;
 */
char *
PTP::Identity::GetIssuerName(int nid) const
{
	if (!m_cert)
		return NULL;
	X509_NAME *name = X509_get_issuer_name(m_cert);
	if (!name)
		return NULL;
	int size = X509_NAME_get_text_by_NID(name, nid, NULL, 0);
	if (size <= 0)
		return NULL;

	char *str = new char[size + 1];
	X509_NAME_get_text_by_NID(name, nid, str, size + 1);
	return str;
}

/**
 * PTP::Identity::SetIssuerName: Set certificate issuer name element.
 * @nid: Element identifier (%COMMON_NAME, %EMAIL_ADDRESS, ...).
 * Example:
 *   PTP::Identity *id = ...;
 *   id->$SetIssuerName(PTP::Identity::EMAIL_ADDRESS, "john\@doe.org");
 */
void
PTP::Identity::SetIssuerName(int nid, const char *value)
{
	if (!m_cert || !value)
		return;
	X509_NAME *name = X509_get_issuer_name(m_cert);
	if (!name)
		return;
	X509_NAME_add_entry_by_NID(
		name, nid, MBSTRING_ASC, (BYTE*) value, -1, -1, 0);
}

/**
 * PTP::Identity::GetExpiration: Get certificate expiration time.
 * Returns: Expiration time string or NULL on error.
 * Notes: It is the caller's responsibility to free the returned string.
 * Example:
 *   PTP::Identity *id = ...;
 *   char *expire = id->$GetExpiration();
 *   if (expire)
 *       printf("Expiration: %s\n", expire);
 *   delete [] expire;
 */
char *
PTP::Identity::GetExpiration() const
{
	BIO *bio = BIO_new(BIO_s_mem());
	if (!bio)
		return NULL;
	if (!ASN1_TIME_print(bio, X509_get_notAfter(m_cert)))
	{
		BIO_free(bio);
		return NULL;
	}
	BUF_MEM *buf;
	BIO_get_mem_ptr(bio, &buf);
	char *expire = new char[buf->length + 1];
	memcpy(expire, buf->data, buf->length);
	expire[buf->length] = '\0';
	BIO_free(bio);
	return expire;
}

/**
 * PTP::Identity::Encrypt: Encrypt data with public key.
 * @plain: Plaintext data.
 * @size: Plaintext size (%PLAINTEXT_SIZE bytes or less).
 * @cipher: [$OUT] Ciphertext data (%CIPHERTEXT_SIZE bytes) or NULL.
 * Returns: %CIPHERTEXT_SIZE on success or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE plain[] = ...;
 *   BYTE cipher[PTP::Identity::CIPHERTEXT_SIZE];
 *   int ok = (id->$Encrypt(plain, sizeof(plain), cipher) > 0);
 */
int
PTP::Identity::Encrypt(const BYTE *plain, int size, BYTE *cipher) const
{
	if (!m_cert)
		return -1;
	RSA *key = EVP_PKEY_get1_RSA(X509_get_pubkey(m_cert));
	if (!key || size <= 0 || size > PLAINTEXT_SIZE)
		return -1;

	if (plain && cipher)
	{
		// use a temporary buffer in case plain == cipher
		BYTE *buffer = new BYTE[size];
		memcpy(buffer, plain, size);

		int ciphersize = RSA_public_encrypt(
			size, buffer, cipher, key, RSA_PKCS1_PADDING);
		delete [] buffer;

		if (ciphersize <= 0)
			return -1;
	}
	return CIPHERTEXT_SIZE;
}

/**
 * PTP::Identity::Decrypt: Decrypt data with $private key.
 * @cipher: Ciphertext data (%CIPHERTEXT_SIZE bytes).
 * @plain: [$OUT] Plaintext (%PLAINTEXT_SIZE bytes or less) or NULL.
 * Returns: Plaintext size on success or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE cipher[PTP::Identity::CIPHERTEXT_SIZE] = ...;
 *   BYTE plain[PTP::Identity::PLAINTEXT_SIZE];
 *   int ok = (id->$Decrypt(cipher, plain) > 0);
 */
int
PTP::Identity::Decrypt(const BYTE *cipher, BYTE *plain) const
{
	if (!m_cert || !m_key)
		return -1;
	RSA *key = EVP_PKEY_get1_RSA(m_key);
	if (!key || !cipher)
		return -1;

	int plainsize = PLAINTEXT_SIZE;
	if (plain)
	{
		// use a temporary buffer in case plain == cipher
		BYTE *buffer = new BYTE[CIPHERTEXT_SIZE];
		memcpy(buffer, cipher, CIPHERTEXT_SIZE);

		plainsize = RSA_private_decrypt(
			CIPHERTEXT_SIZE, buffer, plain, key,
			RSA_PKCS1_PADDING);
		delete [] buffer;

		if (plainsize <= 0)
			plainsize = -1;
	}
	return plainsize;
}

/**
 * PTP::Identity::Verify: Check digitally signed data.
 * @data: Signed data.
 * @size: Data size (!not including the signature).
 * @sign: Signature data (%SIGNATURE_SIZE bytes).
 * Returns: 0 on valid signature or -1 if invalid.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE dataAndSignature[] = ...;
 *   int datasize = ...;
 *   int valid = (id->$Verify(dataAndSignature,
 *                           datasize,
 *                           dataAndSignature + datasize) == 0);
 */
int
PTP::Identity::Verify(const BYTE *data, int size, const BYTE *sign) const
{
	if (!m_cert)
		return -1;
	RSA *key = EVP_PKEY_get1_RSA(X509_get_pubkey(m_cert));
	if (!key || !data || size <= 0 || !sign)
		return -1;

	EVP_MD_CTX digestCtx;
	EVP_DigestInit(&digestCtx, PTP_DIGEST);
	EVP_DigestUpdate(&digestCtx, data, size);
	BYTE digest[PTP_DIGEST_SIZE];
	EVP_DigestFinal(&digestCtx, digest, 0);

	if (RSA_verify(EVP_MD_type(PTP_DIGEST),
		       digest,
		       sizeof(digest),
		       (BYTE*) sign,
		       SIGNATURE_SIZE,
		       key) != 1)
		return -1;
	return 0;
}

/**
 * PTP::Identity::Sign: Digitally sign data with $private key.
 * @data: Data buffer.
 * @size: Buffer size.
 * @sign: [$OUT] Signature data (%SIGNATURE_SIZE bytes).
 * Returns: %SIGNATURE_SIZE on success or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE plain[] = ...;
 *   BYTE cipher[PTP::Identity::CIPHERTEXT_SIZE];
 *   BYTE sign[PTP::Identity::SIGNATURE_SIZE];
 *   id->Encrypt(plain, sizeof(plain), cipher);
 *   id->$Sign(cipher, sizeof(cipher), sign);
 */
int
PTP::Identity::Sign(const BYTE *data, int size, BYTE *sign) const
{
	if (!m_cert || !m_key)
		return -1;
	RSA *key = EVP_PKEY_get1_RSA(m_key);
	if (!key || !data || size <= 0)
		return -1;

	int signsize = SIGNATURE_SIZE;
	if (sign)
	{
		EVP_MD_CTX digestCtx;
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
		EVP_DigestUpdate(&digestCtx, data, size);
		BYTE digest[PTP_DIGEST_SIZE];
		EVP_DigestFinal(&digestCtx, digest, 0);
		
		unsigned size = SIGNATURE_SIZE;
		if (RSA_sign(EVP_MD_type(PTP_DIGEST),
			     digest,
			     sizeof(digest),
			     sign,
			     &size,
			     key) != 1)
			signsize = -1;
	}
	return signsize;
}

/**
 * PTP::Identity::Verify: Check a digitally signed certificate.
 * @subj: Identity and certificate.
 * Returns: 0 on valid signature or -1 if invalid.
 * Notes: A certificate can be invalid because it has a bad signature
 *        or it can be invalid because the signature has expired.
 * Example:
 *   PTP::Identity *id = ...;
 *   printf("%s Certificate\n", id->$Verify(id) ? "Invalid":"Valid");
 */
int
PTP::Identity::Verify(Identity *subj) const
{
	if (!m_cert)
		return -1;

	EVP_PKEY *key = X509_get_pubkey(m_cert);
	if (!key || X509_cmp_time(X509_get_notAfter(m_cert), NULL) <= 0)
		return -1;

	return ((X509_verify(subj->m_cert, key) == 1) ? 0:-1);
}

/**
 * PTP::Identity::Sign: Digitally sign a certificate with $private key.
 * @subj: Identity and certificate to sign.
 * @expire: Time (in seconds) until signature expires.
 * Returns: 0 on success or -1 on error.
 * Example:
 *   PTP::Identity *issuer = ...;
 *   PTP::Identity *subject = ...;
 *   issuer->$Sign(subject, 30 * 24 * 60 * 60); // sign for 30 days
 */
int
PTP::Identity::Sign(Identity *subj, unsigned expire) const
{
	if (!m_cert || !m_key)
		return -1;

	// remove any existing signature
	while(X509_delete_ext(subj->m_cert, 0)) /* empty */ ;

	X509_gmtime_adj(X509_get_notBefore(subj->m_cert), 0);
	X509_gmtime_adj(X509_get_notAfter(subj->m_cert), expire);

	// set issuer identity
	X509_set_issuer_name(subj->m_cert, X509_get_subject_name(m_cert));
	delete [] subj->m_issuerName;
	subj->m_issuerName = subj->GetIssuerName(COMMON_NAME);
	
	// add signature extensions and sign
	X509V3_CTX ctx;
	X509V3_set_ctx(&ctx, subj->m_cert, subj->m_cert, NULL, NULL, 0);
	struct ExtEntry
	{
		const char *name;
		const char *value;
	} ext[] =
	{
		{"subjectKeyIdentifier", "hash"},
		{"authorityKeyIdentifier", "keyid:always,issuer:always"},
		{"basicConstraints", "CA:true"},
		{NULL, NULL}
	};

	for (ExtEntry *i = ext; i->name; i++)
	{
		X509_EXTENSION *ext = X509V3_EXT_conf(NULL,
						      &ctx,
						      (char*) i->name,
						      (char*) i->value);
		X509_add_ext(subj->m_cert, ext, -1);
		X509_EXTENSION_free(ext);
	}

	X509_sign(subj->m_cert, m_key, PTP_DIGEST);

	return 0;
}

/**
 * PTP::Identity::ExportKey: Encrypt and export a session key.
 * @key: Session key.
 * @data: [$OUT] Exported key data (%CIPHERTEXT_SIZE bytes).
 * Returns: %CIPHERTEXT_SIZE on success or -1 on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   PTP::Key *key = ...;
 *   BYTE data[PTP::Identity::CIPHERTEXT_SIZE];
 *   id->$ExportKey(key, data);
 *   ...
 */
int
PTP::Identity::ExportKey(PTP::Key *key, BYTE *data)
{
	if (key && data)
	{
		BYTE keydata[PTP::Key::KEY_SIZE];
		key->Export(keydata);
		Encrypt(keydata, sizeof(keydata), data);
		memset(keydata, 0, sizeof(keydata));
	}
	return CIPHERTEXT_SIZE;
}

/**
 * PTP::Identity::ImportKey: Decrypt and import a session key.
 * @data: Encrypted session key data.
 * @size: Data size.
 * Returns: Session key on success or NULL on error.
 * Example:
 *   PTP::Identity *id = ...;
 *   BYTE data[PTP::Identity::CIPHERTEXT_SIZE];
 *   // receive data[]
 *   PTP::Key *key = id->$ImportKey(data, sizeof(data));
 */
PTP::Key *
PTP::Identity::ImportKey(BYTE *data, int size)
{
	if (!data || size != CIPHERTEXT_SIZE)
		return NULL;
	BYTE keydata[PTP::Key::KEY_SIZE];
	Decrypt(data, keydata);
	PTP::Key *key = new Key(keydata);
	memset(keydata, 0, sizeof(keydata));
	return key;
}

/*
 * PTP::Identity::Show: Display full contents of the X.509
 *                      certificate to stdout.
 */
void
PTP::Identity::Show() const
{
	if (!m_cert)
		return;

	BIO *out = BIO_new(BIO_s_file());
	BIO_set_fp(out, stdout, BIO_NOCLOSE);
	X509_print(out, m_cert);
	BIO_free(out);
}

/*
 * PTP::Identity::DestroyKey: Destroy the private key.
 */
void
PTP::Identity::DestroyKey()
{
	EVP_PKEY_free(m_key);
	m_key = NULL;
}
