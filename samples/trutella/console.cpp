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

/*
 * console: Console Trutella interface.
 */

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ptp/list.h>
#include <ptp/thread.h>
#include <ptp/auth.h>
#include <ptp/debug.h>
#include "trut.h"

// Uncomment to build for performance testing
//#define PERF_TEST 1

// enable/suppress output
static int trutella_quiet = 0;

/*
 * SearchResponse: Search response data.
 */
struct SearchResponse:public PTP::List::Entry
{
	Trut::File *file;
	int id;
};

static Trut::Group *s_group = NULL;
static PTP::List s_resp;
static int s_respCount = 0;

/*
 * ClearResponses: Clear search responses.
 * @trut: Trutella client.
 */
static void
ClearResponses(Trut *trut)
{
	s_resp.Lock();
	SearchResponse *resp;
	PTP_LIST_FOREACH(SearchResponse, resp, &s_resp)
	{
		s_resp.Remove(resp, 0);
		delete resp->file;
		delete resp;
	}
	s_resp.Unlock();
	s_respCount = 0;
}

/*
 * FindResponse: Locate a search response.
 * @id: Response identifier.
 * Returns: Search response or NULL if none found.
 */
static SearchResponse *
FindResponse(int id)
{
	SearchResponse *resp;
	s_resp.Lock();
	PTP_LIST_FOREACH(SearchResponse, resp, &s_resp)
	{
		if (resp->id == id)
			break;
	}
	s_resp.Unlock();
	return resp;
}

/*
 * FindFile: Search response callback.
 * @file: File.
 * @context: Search request context.
 */
static void
FindFile(Trut::File *file, void *context)
{
	SearchResponse *resp = new SearchResponse;
	resp->file = file;
	resp->id = ++s_respCount;

	s_resp.Insert(resp);

	const Trut::Group *group = file->GetGroup();
	const char *groupName = group ? group->GetName():"GnutellaNet";

	if (trutella_quiet)
		return;

	printf("  %d) %s (%lu bytes)"
	       " @ %lu.%lu.%lu.%lu:%u (%s)\n",
	       resp->id,
	       file->GetName(),
	       file->GetSize(),
	       (file->GetIp() >> 24) & 0xff,
	       (file->GetIp() >> 16) & 0xff,
	       (file->GetIp() >> 8) & 0xff,
	       file->GetIp() & 0xff,
	       file->GetPort(),
	       groupName);
}

/*
 * GetFile: File get callback.
 * @file: File.
 * @status: Get status.
 * @data: Unused.
 * @size: Unused.
 * @context: Get request context.
 */
static void
GetFile(Trut::File *file,
	Trut::GetStatus status,
	BYTE *data,
	unsigned long size,
	void *context)
{
	if (trutella_quiet)
		return;

	switch (status)
	{
	case Trut::GET_OK:
		printf("#");
		fflush(stdout);
		break;
	case Trut::GET_DONE:
		printf("\nDownloaded %s (%lu bytes)\n", file->GetName(), size);
		break;
	case Trut::GET_ERROR:
		printf(" failed\n");
		break;
	}
}

/*
 * Joined: Group join callback.
 * @group: Secure group.
 * @status: Join status.
 * @context: Group context.
 */
static void
Joined(Trut::Group *group, Trut::JoinStatus status, void *context)
{
	s_group = group;

	if (trutella_quiet)
		return;

	switch (status)
	{
	case Trut::JOIN_OK:
		printf("Joined %s\n", group->GetName());
		break;
	case Trut::JOIN_CREATED:
		printf("Created %s\n", group->GetName());
		break;
	case Trut::JOIN_ERROR:
		printf("Join %s failed\n", group->GetName());
		break;
	default:
		break;
	}
}

/*
 * Accept: Group accept callback.
 * @group: Secure group.
 * @id: New group member name.
 * @context: Group context.
 * Returns: 1, accept new member.
 */
static int
Accept(Trut::Group *group, const char *id, void *context)
{
	if (!trutella_quiet)
		printf("Accepting %s into %s..\n", id, group->GetName());
	return 1;
}

/*
 * MatchCmd: Match a command string.
 * @s1: Command string.
 * @s2: Matching string.
 * Returns: 1 on match or 0 for failed match.
 */
static int
MatchCmd(const char *s1, const char *s2)
{
	for (; *s1 && *s1 == *s2; s1++, s2++) ;
	return (*s1 ? 0:1);
}

/*
 * ShowHost: Enumeration connected client.
 * @host: Connected host.
 */
static void
ShowHost(Trut::Host *host)
{
	printf("  %s (%lu.%lu.%lu.%lu:%u)\n",
	       host->GetUrl(),
	       (host->GetIp() >> 24) & 0xff,
	       (host->GetIp() >> 16) & 0xff,
	       (host->GetIp() >> 8) & 0xff,
	       (host->GetIp() & 0xff),
	       host->GetPort());
}

/*
 * ShowOpen: Show newly connected client.
 */
static void
ShowOpen(Trut::Host *host, void **context)
{
	if (trutella_quiet)
		return;
	printf("Connected to %s\n", host->GetUrl());
}

/*
 * ShowClose: Show disconnected client.
 */
static void
ShowClose(Trut::Host *host, void *context)
{
	if (trutella_quiet)
		return;
	printf("Connection to %s lost\n", host->GetUrl());
}

#ifdef PERF_TEST
/*
 * GetTime: Get current system time.
 * Returns: Time in milliseconds.
 */
static unsigned long
GetTime()
{
#ifdef WIN32
	return GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((unsigned long) tv.tv_sec * 1000
		+ ((unsigned long) tv.tv_usec / 1000));
#endif
}
#endif // PERF_TEST

/*
 * HandleCmds: Handle user/script commands.
 * @trut: Trutella client.
 * @prompt: Prompt string.
 */
static void
HandleCmds(Trut *trut, const char *prompt, FILE *in)
{
	trut->SetOpenCallback(ShowOpen);
	trut->SetCloseCallback(ShowClose);

	Trut::Port *port = trut->AddPort(0);

	PTP::Net::Ip ip = trut->GetIp();
	printf("Local %lu.%lu.%lu.%lu:%lu\n",
	       (ip >> 24) & 0xff,
	       (ip >> 16) & 0xff,
	       (ip >> 8) & 0xff,
	       ip & 0xff,
	       port->GetPort());

#ifdef PERF_TEST
	unsigned long mark = 0;
#endif
	unsigned long total = 0;

	for (;;)
	{
		if (!trutella_quiet)
		{
			printf(prompt);
			fflush(stdout);
		}

		char buffer[256];
		*buffer = '\0';
		if (!fgets(buffer, sizeof(buffer), in) || !*buffer)
			return;

		if (!trutella_quiet && in != stdin)
			printf("%s", buffer);

		int size = strlen(buffer);
		if (buffer[size - 1] == '\n')
			buffer[size - 1] = '\0';

		char *cmd = buffer + strspn(buffer, " ");
		char *arg = cmd;
		for (; *arg && !isspace(*arg); arg++) ;
		if (*arg)
		{
			*arg = '\0';
			arg++;
		}
		for (; isspace(*arg); arg++) ;
		if (!*arg)
			arg = NULL;

		if (MatchCmd(cmd, "open"))
		{
			if (!arg)
				continue;
			trut->AddHost(arg);
		}
		else if (MatchCmd(cmd, "search"))
		{
			if (!arg)
				continue;
			trut->SearchStop(NULL);
			ClearResponses(trut);
			trut->Search(arg, FindFile, NULL, s_group);
			PTP::Thread::Sleep(2);
		}
		else if (MatchCmd(cmd, "get"))
		{
			if (!arg)
				continue;

			int id = (int) strtoul(arg, NULL, 10);
			SearchResponse *resp = FindResponse(id);
			if (resp)
			{
				if (!trutella_quiet)
				{
					printf("Getting %s:",
					       resp->file->GetName());
					fflush(stdout);
				}
				trut->Get(resp->file,
					  resp->file->GetName(),
					  GetFile,
					  resp);
				trut->GetWait(resp->file);
				total += resp->file->GetSize();
			}
		}
		else if (MatchCmd(cmd, "join"))
		{
			if (!arg)
				continue;
			
			s_group = NULL;
			trut->JoinGroup(arg, 1, Joined, Accept,	arg);
		}
		else if (MatchCmd(cmd, "leave"))
		{
			trut->LeaveGroup(s_group);
			s_group = NULL;
		}
		else if (MatchCmd(cmd, "share"))
		{
			if (!arg)
				continue;
			trut->AddShared(arg, NULL, s_group);
		}
		else if (MatchCmd(cmd, "wait"))
		{
			for (;;)
				PTP::Thread::Sleep(60);
		}
		else if (MatchCmd(cmd, "show"))
		{
			printf("Hosts\n");
			trut->GetHosts(ShowHost);
		}
		else if (MatchCmd(cmd, "quit"))
		{
			return;
		}
#ifdef PERF_TEST
		else if (MatchCmd(cmd, "quiet"))
		{
			trutella_quiet = arg ? strtoul(arg, NULL, 10):1;
		}
		else if (MatchCmd(cmd, "mark"))
		{
			mark = GetTime();
			total = 0;
		}
		else if (MatchCmd(cmd, "time"))
		{
			if (!trutella_quiet)
			{
				unsigned long delta = GetTime() - mark;
				float mbps = ((float) total
					      / (float) delta
					      / 1024.0
					      / 1024.0
					      * 1000.0);
				printf("%ld bytes received in %.02f secs"
				       " (%.04f MB/s)\n",
				       total,
				       delta / 1000.0,
				       mbps);
			}
		}
		else if (MatchCmd(cmd, "sleep"))
		{
			if (!arg)
				continue;
			unsigned long sec = strtoul(arg, NULL, 10);
			PTP::Thread::Sleep(sec);
		}
#endif // PERF_TEST
		else if (MatchCmd(cmd, "help"))
		{
			printf("Commands:\n");
			printf("  open HOST[:PORT]  Open connection\n");
			printf("  search STRING     Search GnutellaNet\n");
			printf("  get INDEX         Fetch file\n");
			printf("  join GROUP        Join group\n");
			printf("  leave GROUP       Leave group\n");
			printf("  share DIR         Share directory files\n");
			printf("  wait              Wait for clients\n");
			printf("  show              Show hosts\n");
			printf("  quit              Quit\n");
			printf("  help              Help text\n");
#ifdef PERF_TEST
			printf("\nPerformance testing:\n");
			printf("  quiet [0|1]       Suppress normal output\n");
			printf("  mark              Mark current time\n");
			printf("  time              Print delta since mark\n");
			printf("  sleep SEC         Delay\n");
#endif // PERF_TEST
		}
		else
		{
			printf("Undefined command: \"%s\". "
			       "Try \"help\".\n",
			       cmd);
		}
	}
}

int
main(int argc, char **argv)
{
#ifdef WIN32
	PTP::Store store(HKEY_CURRENT_USER, "Software\\PTL\\Cert", NULL, NULL);
#else
	char path[2048];
	sprintf(path, "%s/.ptl/cert", getenv("HOME"));
	PTP::Store store(path, NULL, NULL);
#endif

	if (store.Load() || !store.Find(NULL, 1))
	{
		printf("Trutella: cannot load certificates.\n");
		return 0;
	}

	Trut trut(&store);

	const PTP::Identity *local = store.Find(NULL, 1);
	char prompt[64];
	sprintf(prompt, "(trutella:%s) ", local->GetName());

	if (argc <= 1)
		HandleCmds(&trut, prompt, stdin);
	else
	{
		int i;
		for (i = 1; i < argc; i++)
		{
			FILE *in = fopen(argv[i], "rb");
			if (in)
			{
				HandleCmds(&trut, prompt, in);
				fclose(in);
			}
		}
	}
	
	ClearResponses(&trut);

	return 0;
}
