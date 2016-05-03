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

#ifndef __PTP_DEBUG_H__
#define __PTP_DEBUG_H__

#if defined(_DEBUG) && !defined(WIN32)

#include <stdarg.h>
#include <string.h>
#include <ptp/ptp.h>

/**
 * PTP::Debug: Application debug support.
 * Synopsis: #define _DEBUG 1
 *           #include <ptp/debug.h>
 * Notes: %_DEBUG should be defined to enable debug support.
 *        Debug replaces the global &new and &delete functions.
 */
class PTP::Debug
{
public:
	class Heap;

	EXPORT static void Dump(const void *data, int size = -1);
	EXPORT static void Log(const char *fmt, ...);
	EXPORT static void Scan();
	EXPORT static void Error(void *ptr = NULL);

	static void *s_watch;
};

/*
 * Replace the global new and delete functions with debug versions.
 */
void *operator new[](size_t size, const char *file, unsigned lineno);
void *operator new(size_t size, const char *file, unsigned lineno);
void operator delete[](void *ptr);
void operator delete(void *ptr);

/*
 * Simple version of strdup that uses new instead of malloc.
 */
inline char *
__strdup(const char *s, const char *file, unsigned lineno)
{
        if (!s)
                return NULL;
        char *copy = new(file, lineno) char[strlen(s) + 1];
        strcpy(copy, s);
        return copy;
}
#define strdup(x) __strdup((x), __FILE__, __LINE__)

/*
 * Enhance new with file and line number information to ease debug.
 */
#define new new(__FILE__, __LINE__)

#endif // _DEBUG && !WIN32

#endif // __PTP_DEBUG_H__
