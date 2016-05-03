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

#ifndef __GNUTELLA_H__
#define __GNUTELLA_H__

#include <ptp/ptp.h>

#define GNUTELLA_CONNECT_REQUEST "GNUTELLA CONNECT/0.4\n\n"
#define GNUTELLA_CONNECT_RESPONSE "GNUTELLA OK\n\n"

/**
 * Gnutella: Gnutella protocol.
 */
class Gnutella
{
public:
	enum Type
	{
		PING = 0x00,
		PONG = 0x01,
		PUSH = 0x40,
		SEARCH_REQUEST = 0x80,
		SEARCH_RESPONSE = 0x81
	};

	enum
	{
		GUID_SIZE = 16,
		DEFAULT_TTL = 7
	};

	struct Packet
	{
		BYTE guid[GUID_SIZE];
		BYTE type;
		BYTE ttl;
		BYTE hops;
		BYTE size[4];
	};

	struct SearchRqst
	{
		BYTE guid[GUID_SIZE];
		BYTE type;
		BYTE ttl;
		BYTE hops;
		BYTE size[4];
		
		BYTE speed[2];
		char search[1];
	};
	
	struct SearchResp
	{
		BYTE guid[GUID_SIZE];
		BYTE type;
		BYTE ttl;
		BYTE hops;
		BYTE size[4];
		
		BYTE count;
		BYTE port[2];
		BYTE ip[4];
		BYTE speed[4];
	};
	
	struct SearchEntry
	{
		BYTE ref[4];
		BYTE size[4];
		char name[2];

		SearchEntry *GetNext() const
		{
			return ((SearchEntry*)
				(ref + sizeof(*this) + strlen(name)));
		}
	};
	
	struct SearchTrailer
	{
		BYTE guid[GUID_SIZE];
	};

	static UINT16 Get16(const BYTE *field)
	{
		return *(UINT16*) field;
	}

	static UINT32 Get32(const BYTE *field)
	{
		return *(UINT32*) field;
	}

	static void Set16(BYTE *field, UINT16 value)
	{
		*(UINT16*) field = value;
	}

	static void Set32(BYTE *field, UINT32 value)
	{
		*(UINT32*) field = value;
	}
};

#endif // __GNUTELLA_H__
