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

#include <string.h>
#include <ptp/encode.h>
#include <ptp/debug.h>

/**
 * PTP::Encoding::EncodeBase64: Convert raw data to Base64-encoded data.
 * Type: static
 * @src: Raw data.
 * @size: Data size or -1 if @src is a string.
 * @dst: [$OUT] Base64 data ((@size * 2 + 4) bytes) or NULL.
 * @bpl: Bytes of raw data per output line (default 36).
 * Returns: Base64 data size.
 * Example:
 *   BYTE src[] = {0x01, 0x02, 0x03, 0x04};
 *   char dst[sizeof(src) * 2 + 2];
 *   $PTP::Encoding::EncodeBase64(src, sizeof(src), dst);
 *   // dst[] now contains "AAECAw=="
 */
int
PTP::Encoding::EncodeBase64(const BYTE *src, int size, char *dst, int bpl)
{
	static char table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	if (!src || bpl <= 0)
		return -1;
	if (size == -1)
		size = strlen((const char*) src);

	int total = ((size + 2) / 3) * 4;
	if (size > bpl)
		total += ((size - 1) / bpl);
	if (!dst)
		return total;

	const BYTE *s = src + size - 1;
	char *d = dst + total;
	int prev = *s--;
	int bits = (2 * size) % 6;
	int cnt = (size % bpl);

	*d-- = '\0';
	if (cnt <= 0)
	{
		if (size % bpl)
			*d-- = '\n';
		cnt = bpl;
	}

	switch (bits)
	{
	case 2:
		*d-- = '=';
		*d-- = '=';
		*d-- = table[((prev & 0x3) << 4)];
		break;
	case 4:
		*d-- = '=';
		*d-- = table[((prev & 0xf) << 2)];
		break;
	}

	for (; s >= src;)
	{
		if (cnt > 0)
			cnt--;
		else
		{
			*d-- = '\n';
			cnt = bpl - 1;
		}

		int x = *s--;
		bits = (bits + 4) % 6;
		switch (bits)
		{
		case 0:
			*d-- = table[(prev >> 2)];
			break;
		case 2:
			*d-- = table[((x & 0x3) << 4) | (prev >> 4)];
			break;
		case 4:
			*d-- = table[(prev & 0x3f)];
			*d-- = table[((x & 0xf) << 2) | (prev >> 6)];
			break;
		}
		prev = x;
	}

	if (bits == 2)
		*d-- = table[(prev >> 2)];

	return total;
}

/**
 * PTP::Encoding::DecodeBase64: Convert Base64-encoded data to raw data.
 * Type: static
 * @src: Base64 data.
 * @size: Base64 data size or -1 if @src is a string.
 * @dst: [$OUT] Raw data ((@size / 2) bytes) or NULL.
 * Returns: Raw data size.
 * Example:
 *   char *src = "AAECAw==";
 *   BYTE dst[sizeof(src) / 2];
 *   $PTP::Encoding::DecodeBase64(src, -1, dst);
 *   // dst[] now contains {0x01, 0x02, 0x03, 0x04}
 */
int
PTP::Encoding::DecodeBase64(const char *src, int size, BYTE *dst)
{
#define INVL 0xff
	static BYTE table[] =
	{
		0x3e, INVL, INVL, INVL, 0x3f, 0x34, 0x35, 0x36,
		0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, INVL,
		INVL, INVL, INVL, INVL, INVL, INVL, 0x00, 0x01,
		0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
		INVL, INVL, INVL, INVL, INVL, INVL, 0x1a, 0x1b,
		0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
		0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
		0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
	};

	if (!src)
		return -1;
	if (size == -1)
		size = strlen(src);

	const char *s = src;
	BYTE *d = dst;
	int prev = 0;
	int bits = 0;

	for (; size > 0; size--)
	{
		int i = *s++ - '+';
		if (i < 0 || i >= (int) sizeof(table))
			continue;
		int val = table[i];
		if (val == INVL)
			continue;
		bits = (bits + 6) & 0x7;
		if (bits != 6)
		{
			if (dst)
				*d = ((prev << (6 - bits))
				      | (val >> bits)) & 0xff;
			d++;
		}
		prev = val;
	}

	return (d - dst);
}
