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

#include <openssl/evp.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <ptp/auth.h>
#include <ptp/rand.h>
#include <ptp/key.h>
#include <ptp/debug.h>

/**
 * PTP::Key::Key: Create a new randomly-generated key.
 */
PTP::Key::Key():PTP::List::Entry()
{
	PTP::Random::Fill(m_key, sizeof(m_key));
}

/**
 * PTP::Key::Key: Create a key from data.
 * @data: Key data (%KEY_SIZE bytes).
 * Example:
 *   BYTE data[PTP::Key::KEY_SIZE];
 *   PTP::Random::Fill(data, sizeof(data));
 *   PTP::Key key(data);
 */
PTP::Key::Key(const BYTE *data):PTP::List::Entry()
{
	memcpy(m_key, data, sizeof(m_key));
}

/**
 * PTP::Key::Key: Create a key from a password using the
 *                PKCS#5 PBKDF2 algorithm.
 * @passwd: Private password string.
 * @salt: Well-known salt data.
 * @saltsize: Salt data size or -1 if @salt is a string.
 * Notes: Longer values of @passwd and @salt, obviously, result in
 *        more secure keys.
 * Example:
 *   BYTE salt[256];
 *   PTP::Random::Fill(salt, sizeof(salt));
 *   PTP::Key key("SecretPassword", salt, sizeof(salt));
 */
PTP::Key::Key(const char *passwd, const BYTE *salt, int saltsize)
{
	if (saltsize == -1)
		saltsize = strlen((const char*) salt);
	PKCS5_PBKDF2_HMAC_SHA1(passwd,
			       strlen(passwd),
			       (BYTE*) salt,
			       saltsize,
			       PKCS5_DEFAULT_ITER,
			       sizeof(m_key),
			       m_key);
}

#ifdef PTPTL_DLL

/*
 * PTP::Key::Key: Copy constructor.
 * @key: Source Key.
 */
PTP::Key::Key(const Key& key)
{
	assert(0);
}

/*
 * PTP::Key::operator=: Copy constructor.
 * @key: Source Key.
 */
PTP::Key&
PTP::Key::operator=(const Key& key)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::Key::~Key: Class destructor.
 */
PTP::Key::~Key()
{
	memset(m_key, 0, sizeof(m_key));
}

/**
 * PTP::Key::Encrypt: Encrypt data from @plain into @cipher.
 * @plain: Plaintext data.
 * @size: Plaintext size.
 * @cipher: [$OUT] Ciphertext data or NULL.
 * @iv: 1 to prepend a randomly-generate IV (default).
 * @digest: 1 to append a message digest (default).
 * Returns: Ciphertext size on success or -1 on error.
 * Example:
 *   BYTE plain[] = ...;
 *   int size = $Encrypt(plain, sizeof(plain), NULL);
 *   BYTE *cipher = new BYTE[size];
 *   $Encrypt(plain, sizeof(plain), cipher);
 */
int
PTP::Key::Encrypt(
	const BYTE *plain,
	int size,
	BYTE *cipher,
	int iv,
	int digest) const
{
	// check arguments
	int total = size;
	if (iv)
		total += IV_SIZE;
	if (digest)
		total += PTP_DIGEST_SIZE;
	if (!cipher && size > 0)
		return total;
	if (!plain || size <= 0)
		return -1;

	// calculate digest
	BYTE digestData[PTP_DIGEST_SIZE];
	if (digest)
	{
		EVP_MD_CTX digestCtx;
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
		EVP_DigestUpdate(&digestCtx, plain, size);
		EVP_DigestFinal(&digestCtx, digestData, NULL);
	}

	// allocate a temporary buffer
	BYTE *buffer = new BYTE[total];
	BYTE *dst = buffer;

	// create a random IV and append
	BYTE ivData[IV_SIZE];
	if (iv)
	{
		PTP::Random::Fill(ivData, sizeof(ivData));
		memcpy(dst, ivData, sizeof(ivData));
		dst += sizeof(ivData);
	}
	else
		memset(ivData, 0, sizeof(ivData));

	// encrypt data and digest and append
	EVP_CIPHER_CTX ctx;
	EVP_EncryptInit(&ctx, PTP_SESSION_CIPHER, (BYTE*) m_key, ivData);
	EVP_EncryptUpdate(&ctx, dst, &size, (BYTE*) plain, size);
	dst += size;
	if (digest)
	{
		EVP_EncryptUpdate(&ctx,
				  dst,
				  &size,
				  digestData,
				  sizeof(digestData));
		dst += size;
	}
	EVP_EncryptFinal(&ctx, dst, &size);
	dst += size;

	// copy out data
	total = dst - buffer;
	memcpy(cipher, buffer, total);
	delete [] buffer;
	return total;
}

/**
 * PTP::Key::Decrypt: Decrypt data from @cipher into @plain.
 * @cipher: Ciphertext data.
 * @size: Ciphertext size.
 * @plain: [$OUT] Plaintext data or NULL.
 * @iv: 1 to fetch prepended IV (default).
 * @digest: 1 to verify appended message digest (default).
 * Returns: Plaintext size on success or -1 on error or invalid
 *          message digest.
 * Example:
 *   BYTE cipher[] = ...;
 *   int size = $Decrypt(cipher, sizeof(cipher), NULL);
 *   BYTE *plain = new BYTE[size];
 *   size = $Decrypt(cipher, sizeof(cipher), plain);
 *   if (size <= 0)
 *       return -1;
 */
int
PTP::Key::Decrypt(
	const BYTE *cipher,
	int size,
	BYTE *plain,
	int iv,
	int digest) const
{
	// check arguments
	int total = size;
	if (iv)
		total -= IV_SIZE;
	if (digest)
		total -= PTP_DIGEST_SIZE;
	if (!plain && total > 0)
		return total;
	if (!cipher || total <= 0)
		return -1;

	const BYTE *src = cipher;

	// fetch IV
	BYTE ivData[IV_SIZE];
	if (iv)
	{
		memcpy(ivData, src, sizeof(ivData));
		src += sizeof(ivData);
		size -= sizeof(ivData);
	}
	else
		memset(ivData, 0, sizeof(ivData));

	// allocate a temporary buffer
	BYTE *buffer = new BYTE[size];
	BYTE *dst = buffer;

	// fetch and decrypt data and digest
	EVP_CIPHER_CTX ctx;
	EVP_DecryptInit(&ctx, PTP_SESSION_CIPHER, (BYTE*) m_key, ivData);
	EVP_DecryptUpdate(&ctx, dst, &size, (BYTE*) src, size);
	dst += size;
	EVP_DecryptFinal(&ctx, dst, &size);
	dst += size;

	if (digest)
		dst -= PTP_DIGEST_SIZE;
	total = dst - buffer;

	// calculate digest
	if (digest)
	{
		EVP_MD_CTX digestCtx;
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
		EVP_DigestUpdate(&digestCtx, buffer, total);
		BYTE digestData[PTP_DIGEST_SIZE];
		EVP_DigestFinal(&digestCtx, digestData, NULL);
		
		// check digest
		if (memcmp(digestData,
			   buffer + total,
			   sizeof(digestData)) != 0)
			total = -1;
	}

	// copy out data
	if (total > 0)
		memcpy(plain, buffer, total);
	delete [] buffer;
	return total;
}

/**
 * PTP::Key::Encrypt: Encrypt data from @read and send to @write.
 * @read: Data read function.
 * @write: Data write function or NULL.
 * @context: Context for @read and @write.
 * @readsize: Size of read buffer (default: 1024 bytes).
 * Returns: Total ciphertext size on success or -1 on error.
 * Notes: &Encrypt prepends a random IV and appends a message digest
 *        so the ciphertext will necessarily be larger than the plaintext.
 * Example:
 *   // encrypt from stdin to stdout
 *   int Read(BYTE *buffer, int size, void *context)
 *       {return fread(buffer, 1, size, stdin);}
 *   int Write(const BYTE *buffer, int size, void *context)
 *       {return fwrite(buffer, 1, size, stdout);}
 *   ...
 *   $Encrypt(Read, Write, NULL, 2048, 1, 1);
 */
int
PTP::Key::Encrypt(
	Read read,
	Write write,
	void *context,
	int iv,
	int digest,
	int readsize) const
{
	// check arguments
	if (!read)
		return -1;
	int total = 0;

	// create and send a random IV
	BYTE ivData[IV_SIZE];
	if (iv)
	{
		PTP::Random::Fill(ivData, sizeof(ivData));
		if (WriteAll(write, ivData, sizeof(ivData), context, &total)
		    != sizeof(ivData))
			return -1;
	}
	else
		memset(ivData, 0, sizeof(ivData));

	EVP_CIPHER_CTX ctx;
	EVP_EncryptInit(&ctx, PTP_SESSION_CIPHER, (BYTE*) m_key, ivData);
	EVP_MD_CTX digestCtx;
	if (digest)
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
	BYTE *buffer = new BYTE[readsize + EVP_CIPHER_CTX_block_size(&ctx)];

	int size = 0;
	for (;;)
	{
		// fill buffer
		size = ReadAll(read, buffer, readsize, context);
		if (size <= 0)
		{
			if (size < 0)
				total = -1;
			break;
		}

		// encrypt and send buffer
		if (digest)
			EVP_DigestUpdate(&digestCtx, buffer, size);
		EVP_EncryptUpdate(&ctx,
				  buffer,
				  &size,
				  buffer,
				  size);
		if (WriteAll(write, buffer, size, context, &total) != size)
		{
			total = -1;
			break;
		}
	}

	if (total >= 0 && digest)
	{
		// encrypt and send message digest
		EVP_DigestFinal(&digestCtx, buffer, NULL);
		EVP_EncryptUpdate(&ctx,
				  buffer,
				  &size,
				  buffer,
				  PTP_DIGEST_SIZE);
		if (WriteAll(write, buffer, size, context, &total) != size)
			total = -1;
	}

	if (total >= 0)
	{
		// encrypt and send any final data
		EVP_EncryptFinal(&ctx, buffer, &size);
		if (WriteAll(write, buffer, size, context, &total) != size)
			total = -1;
	}

	delete [] buffer;
	return total;
}

/**
 * PTP::Key::Decrypt: Decrypt data from @read and send to @write.
 * @read: Data read function.
 * @write: Data write function or NULL.
 * @context: Context for @read and @write.
 * @readsize: Size of read buffer (default: 1024 bytes).
 * Returns: Total plaintext size on success or -1 on error
 *          or invalid message digest.
 * Notes: &Decrypt fetches the prepended IV from the ciphertext and
 *        verifies that the appended message digest is valid.
 * Example:
 *   // decrypt from stdin to stdout
 *   int Read(BYTE *buffer, int size, void *context)
 *       {return fread(buffer, 1, size, stdin);}
 *   int Write(const BYTE *buffer, int size, void *context)
 *       {return fwrite(buffer, 1, size, stdout);}
 *   ...
 *   int size = $Decrypt(Read, Write, NULL, 2048, 1, 1);
 *   if (size <= 0)
 *       return -1;
 */
int
PTP::Key::Decrypt(
	Read read,
	Write write,
	void *context,
	int iv,
	int digest,
	int readsize) const
{
	// check arguments
	if (!read)
		return -1;
	int total = 0;

	// fetch IV
	BYTE ivData[IV_SIZE];
	if (iv)
	{
		if (ReadAll(read, ivData, sizeof(ivData), context)
		    != sizeof(ivData))
			return -1;
	}
	else
		memset(ivData, 0, sizeof(ivData));

	EVP_CIPHER_CTX ctx;
	EVP_DecryptInit(&ctx, PTP_SESSION_CIPHER, (BYTE*) m_key, ivData);
	EVP_MD_CTX digestCtx;
	if (digest)
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
	BYTE *buffer = new BYTE[readsize];

	int size = 0;
	for (;;)
	{
		// fill remainder of buffer
		int s = ReadAll(read,
				buffer + size,
				readsize - size,
				context);
		if (s < 0)
		{
			total = -1;
			break;
		}
		size += s;

		if (digest)
		{
			// decrypt and send buffer (without padding)
			size -= DIGEST_PADDED_SIZE;
			if (size > 0)
			{
				EVP_DecryptUpdate(&ctx,
						  buffer,
						  &size,
						  buffer,
						  size);
				EVP_DigestUpdate(&digestCtx, buffer, size);
				if (WriteAll(write,
					     buffer,
					     size,
					     context,
					     &total) != size)
				{
					total = -1;
					break;
				}
				memmove(buffer,
					buffer + size,
					DIGEST_PADDED_SIZE);
				size = 0;
			}
			size += DIGEST_PADDED_SIZE;
		}
		else if (size > 0)
		{
			// decrypt and send entire buffer
			EVP_DecryptUpdate(&ctx,
					  buffer,
					  &size,
					  buffer,
					  size);
			if (WriteAll(write, buffer, size, context, &total)
			    != size)
			{
				total = -1;
				break;
			}
			size = 0;
		}

		if (s <= 0 || total < 0)
			break;
	}

	if (total >= 0)
	{
		// decrypt and send final data and message digest
		if (size > 0)
			EVP_DecryptUpdate(&ctx, buffer, &size, buffer, size);
		int fsize = 0;
		EVP_DecryptFinal(&ctx, buffer + size, &fsize);
		size += fsize;
		if (digest)
		{
			size -= PTP_DIGEST_SIZE;
			EVP_DigestUpdate(&digestCtx, buffer, size);
		}
		if (WriteAll(write, buffer, size, context, &total) != size)
			total = -1;
	}

	if (total >= 0 && digest)
	{
		// verify message digest
		BYTE digestData[PTP_DIGEST_SIZE];
		EVP_DigestFinal(&digestCtx, digestData, NULL);
		if (memcmp(buffer + size, digestData, PTP_DIGEST_SIZE) != 0)
			total = -1;
	}

	delete [] buffer;
	return total;
}

/**
 * PTP::Key::Transfer: Transfer data from @read to @write.
 * Type: static
 * @read: Data read function.
 * @write: Data write function or NULL.
 * @context: Context for @read and @write.
 * @readsize: Size of read buffer (default: 1024 bytes).
 * Returns: Total transfer size on success or -1 on error.
 * Notes: &Transfer performs no encryption or decryption on the data stream.
 * Example:
 *   // transfer data from stdin to stdout
 *   int Read(BYTE *buffer, int size, void *context)
 *       {return fread(buffer, 1, size, stdin);}
 *   int Write(const BYTE *buffer, int size, void *context)
 *       {return fwrite(buffer, 1, size, stdout);}
 *   ...
 *   $PTP::Key::Transfer(Read, Write, NULL, 2048);
 */
int
PTP::Key::Transfer(
	Read read,
	Write write,
	void *context,
	int readsize)
{
        BYTE *buffer = new BYTE[readsize];
        int size = 0;
        for (;;)
        {
                int s = (*read)(buffer, readsize, context);
                if (s <= 0)
                        break;
                if (write)
                        (*write)(buffer, s, context);
                size += s;
        }
        delete [] buffer;
        return size;
}

/**
 * PTP::Key::Export: Export key.
 * @data: [$OUT] Key data (%KEY_SIZE bytes) or NULL.
 * Returns: Data size (always %KEY_SIZE).
 * Example:
 *   PTP::Key *key = ...;
 *   BYTE data[PTP::Key::KEY_SIZE];
 *   key->Export(data);
 */
int
PTP::Key::Export(BYTE *data) const
{
	if (data)
		memcpy(data, m_key, KEY_SIZE);
	return KEY_SIZE;
}

/*
 * PTP::Key::ReadAll: Read data until entire buffer is full.
 * Type: static
 * @read: Data read function.
 * @buffer: [$OUT] Data buffer.
 * @size: Buffer size.
 * @context: Context for @read.
 * Returns: Number of bytes read or -1 on error.
 */
int
PTP::Key::ReadAll(PTP::Key::Read read,
		  BYTE *buffer,
		  int size,
		  void *context)
{
	int readsize = 0;
	while (readsize < size)
	{
		int s = (*read)(buffer + readsize, size - readsize, context);
		if (s < 0)
			return -1;
		else if (s == 0)
			break;
		readsize += s;
	}
	return readsize;
}

/*
 * PTP::Key::WriteAll: Write data until entire buffer is sent.
 * Type: static
 * @write: Data write function or NULL.
 * @buffer: Data buffer.
 * @size: Buffer size.
 * @context: Context for @write.
 * @total: [$OUT] Total size written.
 * Returns: Number of bytes written or -1 on error.
 */
int
PTP::Key::WriteAll(PTP::Key::Write write,
		   const BYTE *buffer,
		   int size,
		   void *context,
		   int *total)
{
	if (!write)
	{
		*total += size;
		return size;
	}
	int writesize = 0;
	while (writesize < size)
	{
		int s = (*write)(buffer + writesize,
				 size - writesize,
				 context);
		if (s < 0)
			return -1;
		else if (s == 0)
			break;
		writesize += s;
	}
	*total += writesize;
	return writesize;
}
