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

#ifndef __PTP_ID_H__
#define __PTP_ID_H__

#ifdef WIN32
#define __WINCRYPT_H__
#endif
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <ptp/ptp.h>
#include <ptp/list.h>
#include <ptp/key.h>

/**
 * PTP::Identity: Identity and X509v3 certificate support.
 * Synopsis: #include <ptp/id.h>
 */
class EXPORT PTP::Identity:public PTP::List::Entry
{
public:
	enum
	{
		/**
		 * PTP::Identity::KEY_SIZE: Public key (RSA modulus) size.
		 */
		KEY_SIZE = PTP_PUBLIC_KEY_SIZE,

		/**
		 * PTP::Identity::KEY_EXPONENT: Public key (RSA) exponent.
		 */
		KEY_EXPONENT = 65537,

		/**
		 * PTP::Identity::PLAINTEXT_SIZE:
		 * Maximum plaintext size for &Encrypt and &Decrypt.
		 */
		PLAINTEXT_SIZE = KEY_SIZE - 11, // less PKCS padding
		
		/**
		 * PTP::Identity::CIPHERTEXT_SIZE:
		 * Ciphertext size for &Encrypt and &Decrypt.
		 */
		CIPHERTEXT_SIZE = KEY_SIZE,

		/**
		 * PTP::Identity::SIGNATURE_SIZE:
		 * Digital signature size for &Sign and &Verify.
		 */
		SIGNATURE_SIZE = CIPHERTEXT_SIZE,
	};

	/*
	 * NID values
	 */
	enum
	{
		COMMON_NAME = NID_commonName,
		COUNTRY_NAME = NID_countryName,
		LOCALITY_NAME = NID_localityName,
		STATE_OR_PROVINCE_NAME = NID_stateOrProvinceName,
		ORGANIZATION_NAME = NID_organizationName,
		ORGANIZATIONAL_UNIT = NID_organizationalUnitName,
		EMAIL_ADDRESS = NID_pkcs9_emailAddress,
	};

	Identity(const char *name);
	Identity(const Identity& ident);
	~Identity();

	const char *GetName() const;
	char *GetName(int nid) const;
	void SetName(int nid, const char *value);
	int GetKey(BYTE *data) const;

	const char *GetIssuerName() const;
	char *GetIssuerName(int nid) const;
	void SetIssuerName(int nid, const char *value);

	char *GetExpiration() const;

	int Encrypt(const BYTE *plain, int size, BYTE *cipher) const;
	int Decrypt(const BYTE *cipher, BYTE *plain) const;

	int Verify(const BYTE *data, int size, const BYTE *sign) const;
	int Verify(Identity *subj) const;

	int Sign(const BYTE *data, int size, BYTE *sign) const;
	int Sign(Identity *subj, unsigned expire) const;

	int ExportKey(PTP::Key *key, BYTE *data);
	PTP::Key *ImportKey(BYTE *data, int size);

	void Show() const;

protected:
	enum {EXPIRE_DEFAULT = 30 * 24 * 60 * 60};

	friend class PTP::Store;

	Identity& operator=(const Identity& ident);
	Identity(X509 *cert, EVP_PKEY *key = NULL);
	void DestroyKey();

	X509 *m_cert;
	EVP_PKEY *m_key;
	char *m_name;
	char *m_issuerName;
};

#endif // __PTP_ID_H__
