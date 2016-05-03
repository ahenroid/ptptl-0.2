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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <ptp/collect.h>
#include <ptp/debug.h>

#ifndef WIN32
#define MAX_PATH 1024
#endif

#ifdef WIN32
/*
 * strncasecmp: Compare two strings (case insensitive).
 * @s1: First string.
 * @s2: Second string.
 * @size: Comparison length.
 * Returns: -1, 0, or 1 if s1 < s2, s1 == s2, or s1 > s2 respectively.
 */
static int
strncasecmp(const char *s1, const char *s2, int size)
{
	while (size-- > 0)
	{
		if (toupper(*s1) != toupper(*s2))
			return -1;
		s1++;
		s2++;
	}
	return 0;
}
#endif

/**
 * PTP::Collection::Entry::Entry: File entry constructor.
 * @path: File pathname.
 * @size: File size.
 * @context: Context data to be returned from &GetContext.
 */
PTP::Collection::Entry::Entry(const char *path,
			      unsigned long size,
			      void *context)
	:PTP::List::Entry(), m_path(NULL), m_size(size),
	 m_name(NULL), m_data(NULL), m_id(0), m_context(context),
	 m_rescanned(0)
{
	if (path)
	{
		m_path = strdup(path);
#ifdef WIN32
		char *name = strrchr(m_path, '\\');
		if (!name)
			name = strrchr(m_path, '/');
#else
		char *name = strrchr(m_path, '/');
#endif
		name = name ? (name + 1):m_path;
		m_name = strdup(name);
	}
}

/**
 * PTP::Collection::Entry::Entry: Data entry constructor.
 * @name: Data name.
 * @data: Data buffer.
 * @size: Data size.
 * @context: Context data to be returned from &GetContext.
 */
PTP::Collection::Entry::Entry(const char *name,
			      const BYTE *data,
			      unsigned long size,
			      void *context)
	:PTP::List::Entry(), m_path(NULL), m_size(size),
	 m_name(NULL), m_data(NULL), m_id(0), m_context(context),
         m_rescanned(0)
{
	m_name = name ? strdup(name):NULL;
	if (data)
	{
		m_data = new BYTE[size];
		memcpy(m_data, data, size);
	}
}

#ifdef PTPTL_DLL

/*
 * PTP::Collection::Entry: Copy constructor.
 * @entry: Source Entry.
 */
PTP::Collection::Entry::Entry(const Entry& entry)
{
	assert(0);
}

/*
 * PTP::Collection::Entry::operator=: Copy constructor.
 * @entry: Source Entry.
 */
PTP::Collection::Entry&
PTP::Collection::Entry::operator=(const Entry& entry)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/*
 * PTP::Collection::Entry::~Entry: Class destructor.
 */
PTP::Collection::Entry::~Entry()
{
	delete [] m_data;
	delete [] m_name;
	delete [] m_path;
}

/**
 * PTP::Collection::Entry::GetName
 * Returns: Data name or base of file pathname.
 */
const char *
PTP::Collection::Entry::GetName() const 
{
	return m_name;
}

/**
 * PTP::Collection::Entry::GetId
 * Returns: Unique entry number.
 */
unsigned long
PTP::Collection::Entry::GetId() const
{
	return m_id;
}

/**
 * PTP::Collection::Entry::GetPath
 * Returns: File pathname or NULL for a data entry.
 */
const char *
PTP::Collection::Entry::GetPath() const
{
	return m_path;
}

/**
 * PTP::Collection::Entry::GetSize
 * Returns: File or data size.
 */
unsigned long
PTP::Collection::Entry::GetSize() const
{
	return m_size;
}

/**
 * PTP::Collection::Entry::GetData
 * Returns: Data buffer or NULL for a file entry.
 */
const BYTE *
PTP::Collection::Entry::GetData() const
{
	return m_data;
}

/**
 * PTP::Collection::Entry::GetContext
 * Returns: Context data previously passed to &PTP::Collection::Add.
 */
void *
PTP::Collection::Entry::GetContext() const
{
	return m_context;
}

/**
 * PTP::Collection::GetSize
 * Returns: Number of entries in the collection.
 */
const int
PTP::Collection::GetSize() const
{
	return m_size;
}

/*
 * PTP::Collection::Dir::Dir: Class constructor.
 * @path: Directory pathname.
 * @ext: File extension list.
 */
PTP::Collection::Dir::Dir(const char *path, const char *ext, void *context)
	:PTP::List::Entry(), m_context(context)
{
	m_path = path ? strdup(path):NULL;
	m_ext = ext ? strdup(ext):NULL;
}

/*
 * PTP::Collection::Dir::~Dir: Class destructor.
 */
PTP::Collection::Dir::~Dir()
{
	delete [] m_path;
	delete [] m_ext;
}

/**
 * PTP::Collection::Collection: Class constructor.
 */
PTP::Collection::Collection()
	:m_id(0), m_size(0)
{
}

#ifdef PTPTL_DLL

/*
 * PTP::Collection::Collection: Copy constructor.
 * @collect: Source Collection.
 */
PTP::Collection::Collection(const Collection& collect)
{
	assert(0);
}

/*
 * PTP::Collection::operator=: Copy constructor.
 * @collect: Source Collection.
 */
PTP::Collection&
PTP::Collection::operator=(const Collection& collect)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::Collection::~Collection: Class destructor.
 */
PTP::Collection::~Collection()
{
	m_entries.Lock();
	{
		Entry *i = NULL;
		PTP_LIST_FOREACH(Entry, i, &m_entries)
		{
			m_entries.Remove(i, 0);
			delete i;
		}
	}
	m_entries.Unlock();

	m_dirs.Lock();
	{
		Dir *i = NULL;
		PTP_LIST_FOREACH(Dir, i, &m_dirs)

		{
			m_dirs.Remove(i, 0);
			delete i;
		}
	}
	m_dirs.Unlock();
}

/**
 * PTP::Collection::Add: Add an entry to the collection.
 * @entry: New entry.
 */
void
PTP::Collection::Add(Entry *entry)
{
	if (entry)
	{
		unsigned long id = m_id++;
		entry->m_id = id;
		m_entries.Lock();
		m_entries.Insert(entry, 0);
		m_size++;
		m_entries.Unlock();
	}
}

/**
 * PTP::Collection::Destroy: Remove and destroy an entry from the collection.
 * @entry: Existing collection entry.
 */
void
PTP::Collection::Destroy(Entry *entry)
{
	if (entry)
	{
		m_entries.Lock();
		m_entries.Remove(entry, 0);
		m_size--;
		m_entries.Unlock();
		delete entry;
	}
}

/*
 * PTP::Collection::CmpPat: Look for a pattern match (case-insensitive).
 * @str: String.
 * @pat: Pattern.
 * Returns: 0 if matches or -1 if no match.
 */
int
PTP::Collection::CmpPat(const char *str, const char *pat)
{
	const char *s = str;
	const char *p = pat;
	for (; *p; p++)
	{
		switch (*p)
		{
		case '?':
			if (!*s)
				return -1;
			s++;
			break;
		case '*':
			for (p++; *p == '*' || *p == '?'; p++)
			{
				if (*p == '?')
				{
					if (!*s)
						return -1;
					s++;
				}
			}
			if (!*p)
				return 0;
			for (; *s; s++)
			{
				if (CmpPat(s, p) == 0)
					return 0;
			}
			p--;
			break;
		case '\\':
			p++;
			if (!*p)
				return -1;
			/* fall-through */
		default:
			if (toupper(*s) != toupper(*p))
				return -1;
			s++;
			break;
		}
	}

	return ((*s || *p) ? -1:0);
}

/**
 * PTP::Collection::Find: Search for an entry with a matching name.
 * @pat: Pattern to match or NULL to match all.
 * @from: Previous find result or NULL to begin at the start.
 * Returns: Next matching entry or NULL if none.
 * Example:
 *   PTP::Collection collect;
 *   ...
 *   PTP::Collection::Entry *x = NULL;
 *   while (x = collect.Find("*.jpg", x))
 *   {
 *       ...
 *   }
 */
PTP::Collection::Entry *
PTP::Collection::Find(const char *pat, Entry *from)
{
	Entry *i = NULL;
	m_dirs.Lock();
	{
		int start = from ? 0:1;
		PTP_LIST_FOREACH(Entry, i, &m_entries)
		{
			if (start)
			{
				if (!pat || !CmpPat(i->GetName(), pat))
					break;
			}
			else if (i == from)
				start = 1;
		}
	}
	m_dirs.Unlock();
	return i;
}

/**
 * PTP::Collection::Find: Search for an entry with a matching entry number.
 * @id: Entry number to match.
 * Returns: Matching entry or NULL if none.
 */
PTP::Collection::Entry *
PTP::Collection::Find(unsigned long id)
{
	Entry *i = NULL;
	m_dirs.Lock();
	{
		PTP_LIST_FOREACH(Entry, i, &m_entries)
		{
			if (id == i->m_id)

				break;
		}
	}
	m_dirs.Unlock();
	return i;
}

/**
 * PTP::Collection::Add: Add all matching files in all subdirectories.
 * @path: Top-level directory.
 * @ext: List of extensions (separated by ';') or NULL to match all.
 * @context: Context data to be returned from
 *           &PTP::Collection::Entry::GetContext
 * Notes: The user must call &Rescan before the new files are actually added.
 * Example:
 *   PTP::Collection collect;
 *   collect.Add("/tmp/files", ".gz;.tar", NULL);
 *   collect.Rescan();
 */
void
PTP::Collection::Add(const char *path, const char *ext, void *context)
{
	Dir *d = new Dir(path, ext, context);
	m_dirs.Insert(d);
}

/**
 * PTP::Collection::Remove: Remove all entries for files in subdirectories.
 * @path: Top-level directory.
 */
void
PTP::Collection::Remove(const char *path)
{
	m_dirs.Lock();
	{
		Dir *i = NULL;
		PTP_LIST_FOREACH(Dir, i, &m_dirs)
		{
			if (strcmp(path, i->m_path) == 0)
			{
				m_dirs.Remove(i, 0);
				delete i;
			}
		}
	}
	m_dirs.Unlock();
}

/**
 * PTP::Collection::Rescan: Rescan all subdirectories for matching files.
 */
void
PTP::Collection::Rescan()
{
	m_entries.Lock();
	m_size = 0;
	Entry *x;
	PTP_LIST_FOREACH(Entry, x, &m_entries)
	{
		if (x->m_rescanned)
		{
			m_entries.Remove(x, 0);
			delete x;
		}
		else
			m_size++;
	}
	m_entries.Unlock();

	m_dirs.Lock();
	Dir *d;
	PTP_LIST_FOREACH(Dir, d, &m_dirs)
	{
		Scan(d);
	}
	m_dirs.Unlock();
}

/*
 * PTP::Collection::Scan: Scan a directory and its subdirectories.
 * @parent: Top-level directory.
 */
void
PTP::Collection::Scan(Dir *parent)
{
	if (!parent->m_path)
		return;
	
	List scan(0);
	List done(0);

	Dir *d = new Dir(parent->m_path, parent->m_ext, parent->m_context);
	scan.Insert(d);

	for (;;)
	{
		d = (Dir*) scan.GetHead();
		if (!d)
			break;
		scan.Remove(d);

		char *path = new char[strlen(d->m_path) + MAX_PATH + 2];
#ifdef WIN32
		sprintf(path, "%s\\*.*", d->m_path);
		
		WIN32_FIND_DATA info;
		HANDLE h = FindFirstFile(path, &info);
		if (h == INVALID_HANDLE_VALUE)
		{
			delete d;
			delete [] path;
			continue;
		}
		done.Append(d);
		
		for (;;)
		{
			const char *name = info.cFileName;
			sprintf(path, "%s\\%s", d->m_path, name);
			
			if (strcmp(name, ".") != 0
			    && strcmp(name, "..") != 0
			    && (GetFileAttributes(path)
				& FILE_ATTRIBUTE_DIRECTORY))
			{
				Dir *next = new Dir(path,
						    NULL,
						    parent->m_context);
				scan.Insert(next);
			}

			if (!FindNextFile(h, &info))
				break;
		}
		CloseHandle(h);
#else
		DIR *dir = opendir(d->m_path);
		if (!dir)
		{
			delete d;
			delete [] path;
			continue;
		}
		done.Append(d);

		for (;;)
		{
			struct dirent *ent = readdir(dir);
			if (!ent)
				break;
			sprintf(path, "%s/%s", d->m_path, ent->d_name);

			struct stat info;
			if (strcmp(ent->d_name, ".") != 0
			    && strcmp(ent->d_name, "..") != 0
			    && stat(path, &info) == 0
			    && S_ISDIR(info.st_mode))
			{
				Dir *next = new Dir(path,
						    NULL,
						    parent->m_context);
				scan.Insert(next);
			}
		}
#endif
		
		delete [] path;
	}

	for (;;)
	{
		d = (Dir*) done.GetHead();
		if (!d)
			break;
		done.Remove(d);
		ScanDir(d, parent);
		delete d;
	}
}

/*
 * PTP::Collection::CmpExt: Compare entry name with file extension.
 * @name: File or data name.
 * @ext: List of extensions (items separated by ';').
 * Returns: 0 if matches or non-zero if no match.
 */
int
PTP::Collection::CmpExt(const char *name, const char *ext)
{
	if (!ext || !*ext)
		return 0;

	int nameSize = strlen(name);
	const char *start = ext;
	while (*start)
	{
		const char *end = start;
		for (; *end && *end != ';'; end++) ;
		int extSize = end - start;
		if (extSize < nameSize
		    && name[nameSize - extSize - 1] == '.'
		    && strncasecmp(name + nameSize - extSize,
				   start,
				   extSize) == 0)
			return 0;
		for (; *end == ';'; end++) ;
		start = end;
	}
	
	return 1;
}

/*
 * PTP::Collection::ScanDir: Scan a single directory.
 * @d: Directory.
 * @parent: Parent of directory.
 */
void
PTP::Collection::ScanDir(Dir *d, Dir *parent)
{
	if (!d->m_path)
		return;

	char *path = new char[strlen(d->m_path) + MAX_PATH + 2];

#ifdef WIN32
	sprintf(path, "%s\\*.*", d->m_path);

	WIN32_FIND_DATA info;
	HANDLE h = FindFirstFile(path, &info);
	if (h == INVALID_HANDLE_VALUE)
	{
		delete [] path;
		return;
	}

	for (;;)
	{
		const char *name = info.cFileName;
		sprintf(path, "%s\\%s", d->m_path, name);

		if (!(GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY)
		    && !CmpExt(name, d->m_ext))
		{
			Entry *entry = new Entry(path,
						 info.nFileSizeLow,
						 d->m_context);
			entry->m_rescanned = 1;
			Add(entry);
		}

		if (!FindNextFile(h, &info))
			break;
	}
	CloseHandle(h);
#else
	DIR *dir = opendir(d->m_path);
	if (!dir)
	{
		delete [] path;
		return;
	}

	for (;;)
	{
		struct dirent *ent = readdir(dir);
		if (!ent)
			break;

		if (strcmp(ent->d_name, ".") == 0
		    || strcmp(ent->d_name, "..") == 0
		    || CmpExt(ent->d_name, parent->m_ext))
			continue;
		
		sprintf(path, "%s/%s", d->m_path, ent->d_name);
		
		struct stat info;
		if (!stat(path, &info) && !S_ISDIR(info.st_mode))
		{
			Entry *entry = new Entry(path,
						 info.st_size,
						 d->m_context);
			entry->m_rescanned = 1;
			Add(entry);
		}
	}
#endif

	delete [] path;
}
