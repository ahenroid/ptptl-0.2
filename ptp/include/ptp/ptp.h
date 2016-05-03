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

#ifndef __PTP_H__
#define __PTP_H__

#include <openssl/evp.h>
#include <openssl/sha.h>

// use SHA-1 for message digests
#define PTP_DIGEST EVP_sha1()
#define PTP_DIGEST_SIZE SHA_DIGEST_LENGTH

// use Blowfish with OFB encoding for symmetric encryption
#define PTP_SESSION_CIPHER EVP_bf_ofb()
#define PTP_SESSION_KEY_SIZE 16
#define PTP_SESSION_IV_SIZE 8

// use 1024-bit RSA keys for asymmetric encryption
#define PTP_PUBLIC_KEY_SIZE 128

// prefix HTTP tunneled data with "/tunnel"
#define PTP_NET_TUNNEL_PREFIX "tunnel"

// export DLL interfaces
#undef EXPORT
#ifdef PTPTL_DLL
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// useful data types
typedef unsigned char BYTE;
typedef unsigned short UINT16;
#ifndef WIN32
typedef unsigned long UINT32;
#endif

/**
 * PTP: Peer-to-peer support.
 * Synopsis: #include <ptp/ptp.h>
 */
class PTP
{
public:
	// authentication/encryption
	class Identity;
	class Store;
	class Authenticator;
	class Key;
	class Random;

	// utility
	class List;
	class Thread;
	class Mutex;

	// misc.
	class Net;
	class Collection;
	class Debug;
	class Encoding;
};

#endif // __PTP_H__
