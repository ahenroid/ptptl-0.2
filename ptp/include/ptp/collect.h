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

#ifndef __PTP_COLLECT_H__
#define __PTP_COLLECT_H__

#include <ptp/ptp.h>
#include <ptp/list.h>

/**
 * PTP::Collection: Simple file and data collection.
 * Synopsis: #include <ptp/collect.h>
 */
class EXPORT PTP::Collection
{
public:
        /**
	 * PTP::Collection::Entry: File or data entry.
	 */
	class EXPORT Entry:public PTP::List::Entry
	{
	public:
		Entry(const char *path,
		      unsigned long size,
		      void *context = NULL);
		Entry(const char *name,
		      const BYTE *data,
		      unsigned long size,
		      void *context = NULL);

		const char *GetName() const;
		unsigned long GetId() const;

		const char *GetPath() const;
		unsigned long GetSize() const;
		const BYTE *GetData() const;

		void *GetContext() const;

	protected:
		friend class PTP::Collection;

		Entry(const Entry& entry);
		Entry& operator=(const Entry& entry);
		virtual ~Entry();
		
		char *m_path;
		unsigned long m_size;
		char *m_name;
		BYTE *m_data;
		unsigned long m_id;
		void *m_context;
		int m_rescanned;
	};

	Collection();
	~Collection();

	void Add(Entry *entry);
	void Destroy(Entry *entry);

	Entry *Find(const char *pat, Entry *from = NULL);
	Entry *Find(unsigned long id);

	void Add(const char *dir, const char *ext, void *context = NULL);
	void Remove(const char *dir);
	void Rescan();

	const int GetSize() const;

protected:
	struct Dir:public PTP::List::Entry
	{
		Dir(const char *path, const char *ext, void *context);
		~Dir();

		char *m_path;
		char *m_ext;
		void *m_context;
	};

	Collection(const Collection& collect);
	Collection& operator=(const Collection& collect);

	void Scan(Dir *parent);
	void ScanDir(Dir *d, Dir *parent);

	int CmpPat(const char *str, const char *pat);
	int CmpExt(const char *name, const char *ext);

	PTP::List m_dirs;
	PTP::List m_entries;
	unsigned long m_id;
	int m_size;
};

#endif // __PTP_COLLECT_H__

