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
#include <ctype.h>
#include <ptp/list.h>
#include <ptp/debug.h>

#if defined(_DEBUG) && !defined(WIN32)

/*
 * PTP::Debug::Heap: Debug dynamic memory management.
 */
class PTP::Debug::Heap
{
public:
	Heap();
	~Heap();

	void *Allocate(unsigned size, const char *file, unsigned lineno);
	void Free(void *ptr);
	void Scan();

protected:
	/*
	 * PTP::Debug::Heap::Header: Memory allocation header.
	 */
	struct Header:public PTP::List::Entry
	{
		const char *file;
		unsigned lineno;
		unsigned long *end;
		unsigned long magic;
	};

        enum {MAGIC = 0xdeadbeef};

	void ShowLeaks();

	PTP::List m_alloc;
};

void *PTP::Debug::s_watch = NULL;
static PTP::Debug::Heap s_heap;

/**
 * PTP::Debug::Dump: Print contents of buffer in a user-readable form.
 * Type: static
 * @data: Data buffer.
 * @size: Data size or -1 if data is a string (default).
 * Notes: The data is output in both hex and ASCII (for printable characters).
 */
void
PTP::Debug::Dump(const void *data, int size)
{
	if (size == -1 && data)
		size = strlen((const char*) data);

	const BYTE *d = (const BYTE*) data;
	int i, j;
	for (i = 0; i < size; i += 16)
	{
		printf("  %04x: ", i);
		for (j = 0; j < 16 && (i + j) < size; j++)
			printf("%02x ", d[i + j]);
		for (; j < 16; j++)
			printf("   ");
		printf("  ");
		for (j = 0; j < 16 && (i + j) < size; j++)
		{
			int ch = d[i + j];
			printf("%c", isprint(ch) ? ch:'.');
		}
		printf("\n");
	}
}

/**
 * PTP::Debug::Log: Write a message to the debug log.
 * Type: static
 * @fmt: &printf style format string.
 * @...: Additional arguments if any.
 */
void
PTP::Debug::Log(const char *fmt, ...)
{
	static FILE *log = NULL;
	if (!log)
		log = fopen("ptptl_debug.log", "wb");

	va_list args;
	va_start(args, fmt);
	vfprintf(log, fmt, args);
	va_end(args);
}

/**
 * PTP::Debug::Scan: Scan the heap for memory errors.
 */
void
PTP::Debug::Scan()
{
	s_heap.Scan();
}

/**
 * PTP::Debug::Error: &Error is called when a memory error occurs.
 * Type: static
 * @ptr: Memory location.
 * Notes: This function is commonly used as a debugger hook.
 */
void
PTP::Debug::Error(void *ptr)
{
}

/*
 * PTP::Debug::Heap::Heap: Class constructor.
 */
PTP::Debug::Heap::Heap()
{
}

/*
 * PTP::Debug::Heap::Heap: Class destructor.  Outputs memory leak information.
 */
PTP::Debug::Heap::~Heap()
{
	ShowLeaks();
}

/*
 * PTP::Debug::Heap::Allocate: Allocate dynamic memory.
 * @size: Allocation size.
 * @file: Source file pathname or NULL.
 * @lineno: Source line number or 0.
 * Notes: &Allocate will terminate when out of memory and will not return NULL.
 */
void *
PTP::Debug::Heap::Allocate(unsigned size, const char *file, unsigned lineno)
{
	size_t total = (sizeof(Header)
			+ size
			+ sizeof(unsigned long));
	Header *hdr = (Header*) malloc(total);
	if (!hdr)
	{
		printf("OUT of memory at %s:%u.\n", file, lineno);
		Error(NULL);
		exit(1);
	}
	memset(hdr, 0, total);
	hdr->file = file;
	hdr->lineno = lineno;
	hdr->magic = MAGIC;
	hdr->end = (unsigned long*)((BYTE*)(hdr + 1) + size);
	*hdr->end = MAGIC;
	m_alloc.Insert(hdr);

	if ((void*)(hdr + 1) == s_watch)
	{
		printf("Allocated %p.\n", s_watch);
		Error(s_watch);
	}

	return (void*)(hdr + 1);
}

/*
 * PTP::Debug::Heap::Free: Release dynamic memory.
 * @ptr: Allocated dynamic memory.
 */
void
PTP::Debug::Heap::Free(void *ptr)
{
	if (!ptr)
		return;

	if (ptr == s_watch)
	{
		printf("Freeing %p.\n", s_watch);
		Error(s_watch);
	}

	Header *hdr = (Header*) ptr - 1;
	if (hdr->magic != MAGIC)
	{
		printf("Invalid heap start magic at %s:%u (%p).\n",
		       hdr->file, hdr->lineno, hdr + 1);
		Error(ptr);
		return;
	}
	if (*hdr->end != MAGIC)
	{
		printf("Invalid heap magic at %s:%u (%p).\n",
		       hdr->file, hdr->lineno, hdr + 1);
		Error(ptr);
		return;
	}
	m_alloc.Remove(hdr);
	hdr->magic = 0;
	memset(hdr, 0, (BYTE*) hdr->end - (BYTE*) hdr);
	free(hdr);
}

/*
 * PTP::Debug::Heap::Scan: Scan heap for memory errors.
 */
void
PTP::Debug::Heap::Scan()
{
	Header *hdr;
	m_alloc.Lock();
	PTP_LIST_FOREACH(Header, hdr, &m_alloc)
	{
		if (hdr->magic != MAGIC)
		{
			printf("Invalid heap start magic at %s:%u (%p).\n",
			       hdr->file, hdr->lineno, hdr + 1);
			Error(hdr + 1);
			return;
		}
		if (*hdr->end != MAGIC)
		{
			printf("Invalid heap magic at %s:%u (%p).\n",
			       hdr->file, hdr->lineno, hdr + 1);
			Error(hdr + 1);
			return;
		}
	}
	m_alloc.Unlock();
}

/*
 * PTP::Debug::Heap::ShowLeaks: Show location of memory leaks if any.
 */
void
PTP::Debug::Heap::ShowLeaks()
{
	Header *hdr;
	m_alloc.Lock();
	PTP_LIST_FOREACH(Header, hdr, &m_alloc)
	{
		unsigned size = ((BYTE*) hdr->end
				 - (BYTE*)(hdr + 1));
		printf("Memory leak at %s:%u (%p)\n",
		       hdr->file,
		       hdr->lineno,
		       hdr + 1);
		PTP::Debug::Dump(hdr + 1, size);
		Error(hdr + 1);
	}
	m_alloc.Unlock();
}

/*
 * Replacements for global new and delete functions
 */

#ifdef new
#undef new
#endif

void *
operator new[](size_t size, const char *file, unsigned lineno)
{
	return s_heap.Allocate(size, file, lineno);
}

void *
operator new(size_t size, const char *file, unsigned lineno)
{
	return s_heap.Allocate(size, file, lineno);
}

void *
operator new[](size_t size)
{
	return s_heap.Allocate(size, "<unknown>", 0);
}

void *
operator new(size_t size)
{
	return s_heap.Allocate(size, "<unknown>", 0);
}

void
operator delete[](void *ptr)
{
	s_heap.Free(ptr);
}

void
operator delete(void *ptr)
{
	s_heap.Free(ptr);
}

#endif // _DEBUG && !WIN32
