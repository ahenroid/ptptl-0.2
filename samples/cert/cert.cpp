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
 * cert - simple certificate management.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <ptp/id.h>
#include <ptp/store.h>
#include <ptp/debug.h>
#ifdef WIN32
#include <windowsx.h>
#include "resource.h"
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define EXPIRE_DEFAULT (30 * 24 * 60 * 60)

/*
 * Error: Print an error message.
 * @fmt: printf-style format string.
 * ...: Additional arguments.
 */
static void
Error(const char *fmt, ...)
{
	char *buffer = new char[strlen(fmt) + 1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buffer, fmt, args);
	va_end(args);
	buffer[0] = tolower(buffer[0]);
	printf("cert: %s", buffer);
	delete [] buffer;
}

/*
 * Load: Load a certificate from a file.
 * @path: File pathname.
 * @passwd: Certificate store password or NULL.
 * Returns: Certificate on success or NULL on error.
 */
static PTP::Identity *
Load(const char *path, const char *passwd)
{
	PTP::Identity *id = NULL;
	PTP::Store store(path, passwd, passwd);
	if (store.Load() == 0)
	{
		const PTP::Store::Entry *entry
			= store.Find(PTP::Store::IDENTITY);
		if (entry)
			id = new PTP::Identity(*entry->ident.ident);
	}
	else
		Error("Cannot load `%s'.\n", path);
	return id;
}

/*
 * Save: Save a certificate to a file.
 * @ident: Certificate.
 * @exportkey: 1 to export private key or 0 to not.
 * @path: File pathname.
 * @passwd: Certificate store password or NULL.
 * Returns: 0 on success or -1 on error.
 */
static int
Save(PTP::Identity *ident, const char *path, const char *passwd)
{
	if (!ident)
		return -1;
	PTP::Store store(path, passwd, passwd);
	store.Insert(ident, 0);
	if (store.Save())
		Error("Cannot save `%s'.\n", path);
	return 0;
}

/*
 * Verify: Check validity of the certificate.
 * @store: Certificate store.
 * @ident: Certificate.
 * Returns: 0 on valid certificate or -1 for an invalid certificate.
 */
static int
Verify(PTP::Store *store, PTP::Identity *ident)
{
	PTP::Identity *issuer = ident;
	if (ident && strcmp(ident->GetName(), ident->GetIssuerName()) != 0)
		issuer = store->Find(ident->GetIssuerName());
	return (issuer && issuer->Verify(ident) == 0) ? 0:-1;
}

/*
 * Insert: Insert certificate into store if valid.
 * @store: Certificate store.
 * @ident: Certificate.
 * @exportkey: 1 to export private key or 0 to not.
 * Returns: 0 on sucess or -1 on error.
 */
static int
Insert(PTP::Store *store, PTP::Identity *ident, int exportkey)
{
	if (!ident)
		return -1;
	if (Verify(store, ident))
	{
		Error("Invalid certificate `%s'.\n", ident->GetName());
		return -1;
	}

	PTP::Identity *local = store->Find(NULL, 1);
	if (local && strcmp(local->GetName(), ident->GetName()) == 0)
		return 0;

	PTP::Identity *old = store->Find(ident->GetName(), 0);
	if (old)
		store->Remove(old);
	store->Insert(ident, exportkey);
	return 0;
}

/*
 * Find: Find certificate in store.
 * @store: Certificate store.
 * @name: Subject common name.
 * Returns: Certificate on success or NULL if not found.
 */
static PTP::Identity *
Find(PTP::Store *store, const char *name)
{
	PTP::Identity *ident = store->Find(name, 0);
	if (!ident)
		Error("Cannot find `%s'.\n", name);
	return ident;
}

/*
 * Remove: Remove certificate from store.
 * @store: Certificate store.
 * @name: Subject common name.
 * Returns: 0 on success or -1 on error.
 */
static int
Remove(PTP::Store *store, const char *name)
{
	const PTP::Identity *local = store->Find(NULL, 1);
	PTP::Identity *ident = Find(store, name);
	if (!ident)
		return -1;
	else if (ident == local)
	{
		Error("Cannot remove local certificate.\n", name);
		return -1;
	}
	store->Remove(ident);
	return 0;
}

/*
 * SaveAll: Save certificate store.
 * @store: Certificate store.
 */
static void
SaveAll(PTP::Store *store)
{
	if (store->Save())
		Error("Cannot save certificate store.\n");
}

/*
 * Sign: Sign certificate
 * @store: Certificate store.
 * @ident: Certificate.
 * Returns: 0 on success or -1 on error.
 */
static int
Sign(PTP::Store *store, PTP::Identity *ident, int expire)
{
	const PTP::Identity *local = store->Find(NULL, 1);
	if (!local)
	{
		Error("Cannot find local certificate.\n");
		return -1;
	}
	return local->Sign(ident, expire);
}

#ifdef WIN32

static HINSTANCE g_inst;

/*
 * DisplayContext: Context for &ListCallback and &ShowCallback.
 */
struct DisplayContext
{
	PTP::Store *store;
	const char *path;
	PTP::Identity *ident;
};

static void Show(PTP::Store *store,
		 PTP::Identity *ident,
		 const char *path = NULL);

/*
 * ListCallback: Handle certificate list property page.
 */
static int CALLBACK
ListCallback(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	HWND lwnd = GetDlgItem(wnd, IDC_CERTS);
	DisplayContext *ctx = (DisplayContext*)
		GetWindowLong(wnd, GWL_USERDATA);

	int refresh = 0;
	switch (msg)
	{
	case WM_INITDIALOG:
		ctx = (DisplayContext*)	(((PROPSHEETPAGE*) lparam)->lParam);
		SetWindowLong(wnd, GWL_USERDATA, (long) ctx);
		break;
	case WM_COMMAND:
		{
			int i = ListBox_GetCurSel(lwnd);
			PTP::Identity *ident = (PTP::Identity*)
				((i >= 0) ? ListBox_GetItemData(lwnd, i):NULL);
			if (!ident)
				break;

			switch (LOWORD(wparam))
			{
			case IDC_SIGN:
				Sign(ctx->store, ident, EXPIRE_DEFAULT);
				break;
			case IDC_REMOVE:
				Remove(ctx->store, ident->GetName());
				refresh = 1;
				break;
			case IDC_PROP:
				Show(ctx->store, ident);
				break;
			default:
				return FALSE;
			}
		}
		break;
	case WM_NOTIFY:
		{
			NMHDR *hdr = (NMHDR*) lparam;
			switch (hdr->code)
			{
			case PSN_APPLY:
				SaveAll(ctx->store);
				break;
			case PSN_SETACTIVE:
				refresh = 1;
				break;
			default:
				return FALSE;
			}
		}
		break;
	default:
		return FALSE;
	}

	if (refresh)
	{
		ListBox_ResetContent(lwnd);
		
		PTP::Identity *ident = NULL;
		for (;;)
		{
			ident = ctx->store->Find(NULL, 0, NULL, ident);
			if (!ident)
				break;
			int i = ListBox_AddString(lwnd, ident->GetName());
			ListBox_SetItemData(lwnd, i, ident);
		}
	}

	return TRUE;
}

/*
 * ShowCallback: Handle certificate information property page.
 */
static int CALLBACK
ShowCallback(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	DisplayContext *ctx = (DisplayContext*)
		GetWindowLong(wnd, GWL_USERDATA);

	int refresh = 0;
	switch (msg)
	{
	case WM_INITDIALOG:
		ctx = (DisplayContext*)	(((PROPSHEETPAGE*) lparam)->lParam);
		SetWindowLong(wnd, GWL_USERDATA, (long) ctx);
		break;
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_IMPORT:
			Insert(ctx->store, ctx->ident, 0);
			break;
		case IDC_REMOVE:
			Remove(ctx->store, ctx->ident->GetName());
			break;
		default:
			return FALSE;
		}
		break;
	case WM_NOTIFY:
		{
			NMHDR *hdr = (NMHDR*) lparam;
			switch (hdr->code)
			{
			case PSN_APPLY:
				if (ctx->path)
					SaveAll(ctx->store);
				break;
			case PSN_SETACTIVE:
				refresh = 1;
				break;
			default:
				return FALSE;
			}
		}
		break;
	default:
		return FALSE;
	}

	if (refresh)
	{
		const char *path = ctx->path;
		if (!path)
		{
			path = ctx->ident->GetName();
			HWND bwnd = GetDlgItem(wnd, IDC_IMPORT);
			Button_Enable(bwnd, FALSE);
			bwnd = GetDlgItem(wnd, IDC_REMOVE);
			Button_Enable(bwnd, FALSE);
		}
		
#ifdef WIN32
		const char *name = strrchr(path, '\\');
#else
		const char *name = strrchr(path, '/');
#endif
		name = name ? (name + 1):path;
		
		SetDlgItemText(wnd, IDC_NAME, name);
		SetDlgItemText(wnd, IDC_SUBJECT, ctx->ident->GetName());
		SetDlgItemText(wnd,
			       IDC_ISSUER,
			       ctx->ident->GetIssuerName());
		char *expires = ctx->ident->GetExpiration();
		SetDlgItemText(wnd, IDC_EXPIRES, expires);
		delete [] expires;
		SetDlgItemText(wnd,
			       IDC_STATUS,
			       Verify(ctx->store, ctx->ident)
			       ? "Invalid":"Valid");
	}

	return TRUE;
}
#endif // WIN32

/*
 * List: Display list of certificates.
 * @store: Certificate store.
 */
static void
List(PTP::Store *store)
{
#ifdef WIN32
	DisplayContext ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.store = store;
	
	PROPSHEETPAGE page;
	memset(&page, 0, sizeof(page));
	page.dwSize = sizeof(page);
	page.dwFlags = PSP_DEFAULT;
	page.hInstance = g_inst;
	page.pszTemplate = MAKEINTRESOURCE(IDD_CERTS);
	page.pfnDlgProc = ListCallback;
	page.lParam = (LPARAM) &ctx;
	
	PROPSHEETHEADER sheet;
	memset(&sheet, 0, sizeof(sheet));
	sheet.dwSize = sizeof(sheet);
	sheet.dwFlags = (PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW);
	sheet.hInstance = g_inst;
	sheet.pszCaption = __TEXT("Certificates");
	sheet.nPages = 1;
	sheet.nStartPage = 0;
	sheet.ppsp = &page;
	
	PropertySheet(&sheet);
#else // WIN32
	printf("Certificates:\n");
	PTP::Identity *ident = NULL;
	for (;;)
	{
		ident = store->Find(NULL, 0, NULL, ident);
		if (!ident)
			break;
		printf(" %s\n", ident->GetName());
	}
#endif WIN32
}

/*
 * Show: Show certificate information.
 * @store: Certificate store.
 * @ident: Certificate.
 * @path: Certificate file pathname or NULL if none.
 */
static void
Show(PTP::Store *store, PTP::Identity *ident, const char *path)
{
	if (!ident)
		return;

#ifdef WIN32
	DisplayContext ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.store = store;
	ctx.path = path;
	ctx.ident = ident;
	
	PROPSHEETPAGE page[2];
	memset(&page, 0, sizeof(page));

	page[0].dwSize = sizeof(page[0]);
	page[0].dwFlags = PSP_DEFAULT;
	page[0].hInstance = g_inst;
	page[0].pszTemplate = MAKEINTRESOURCE(IDD_GENERAL);
	page[0].pfnDlgProc = ShowCallback;
	page[0].lParam = (LPARAM) &ctx;

	page[1].dwSize = sizeof(page[1]);
	page[1].dwFlags = PSP_DEFAULT;
	page[1].hInstance = g_inst;
	page[1].pszTemplate = MAKEINTRESOURCE(IDD_CERTS);
	page[1].pfnDlgProc = ListCallback;
	page[1].lParam = (LPARAM) &ctx;
	
	PROPSHEETHEADER sheet;
	memset(&sheet, 0, sizeof(sheet));
	sheet.dwSize = sizeof(sheet);
	sheet.dwFlags = (PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW);
	sheet.hInstance = g_inst;
	sheet.pszCaption = __TEXT("Certificate");
	sheet.nPages = path ? 2:1;
	sheet.nStartPage = 0;
	sheet.ppsp = page;
	
	PropertySheet(&sheet);
#else
	const char *name = strrchr(path, '/');
	name = name ? (name + 1):path;

	char *expire = ident->GetExpiration();
	printf("File:    %s\n"
	       "Subject: %s\n"
	       "Issuer:  %s\n"
	       "Expires: %s\n"
	       "Status:  %s\n",
	       name,
	       ident->GetName(),
	       ident->GetIssuerName(),
	       expire ? expire:"",
	       Verify(store, ident) ? "Invalid":"Valid");
	delete [] expire;
	delete ident;
#endif
}

int
main(int argc, char **argv)
{
	const char *prog = strrchr(argv[0], '/');
	prog = prog ? (prog + 1):argv[0];

#ifdef WIN32
	PTP::Store store(HKEY_CURRENT_USER, "Software\\PTL\\Cert", NULL, NULL);
#else
	char path[2048];
	sprintf(path, "%s/.ptl", getenv("HOME"));
	mkdir(path, 0700);
	chmod(path, 0700);
	strcat(path, "/cert");
	PTP::Store store(path, NULL, NULL);
#endif
	store.Load();

	if (argc < 2)
		List(&store);
	else
	{
		const char *passwd = NULL;
		for (char **arg = argv + 1; arg < (argv + argc); arg++)
		{
			int remain = argc - (arg - argv) - 1;
			
			if (strcmp(*arg, "--import") == 0 && remain >= 1)
			{
				arg++;
				const char *path = *arg;

				PTP::Identity *ident = Load(path, passwd);
				if (ident && Insert(&store, ident, 0) == 0)
					SaveAll(&store);
				delete ident;
			}
			else if (strcmp(*arg, "--export") == 0 && remain >= 2)
			{
				arg++;
				const char *name = *arg;
				arg++;
				const char *path = *arg;

				if (name && !*name)
				{
					const PTP::Identity *local
						= store.Find(NULL, 1);
					if (local)
						name = local->GetName();
				}

				PTP::Identity *ident = Find(&store, name);
				if (ident)
					Save(ident, path, passwd);
			}
			else if (strcmp(*arg, "--sign") == 0 && remain >= 2)
			{
				arg++;
				int expire = strtoul(*arg, NULL, 10);
				expire *= 24 * 60 * 60;
				if (!**arg)
					expire = EXPIRE_DEFAULT;
				arg++;
				const char *path = *arg;

				PTP::Identity *ident = Load(path, NULL);
				if (ident && Sign(&store, ident, expire) == 0)
					Save(ident, path, passwd);
				delete ident;
			}
			else if (strcmp(*arg, "--remove") == 0 && remain >= 1)
			{
				arg++;
				if (Remove(&store, *arg) == 0)
					SaveAll(&store);
			}
			else if (strcmp(*arg, "--setup") == 0 && remain >= 1)
			{
				arg++;
				const char *name = **arg ? *arg:NULL;

				PTP::Identity *ident = new PTP::Identity(name);
				if (!ident)
					Error("Cannot create `%s'.\n");
				else
				{
					store.Reset();
					if (Insert(&store, ident, 1) == 0)
						SaveAll(&store);
				}
				delete ident;
			}
			else if (strcmp(*arg, "--load") == 0 && remain >= 1)
			{
				arg++;
				const char *path = *arg;

				PTP::Store store2(path, passwd, passwd);
				if (store2.Load())
					Error("Cannot load `%s'.\n", path);
				else
				{
					store.Reset();
					PTP::Identity *id = NULL;
					for (;;)
					{
						id = store2.Find(NULL,
								 0, NULL, id);
						if (!id)
							break;
						store.Insert(id, 1);
					}
					SaveAll(&store);
				}
			}
			else if (strcmp(*arg, "--save") == 0 && remain >= 1)
			{
				arg++;
				const char *path = *arg;
				
				PTP::Store store2(path, passwd, passwd);
				PTP::Identity *id = NULL;
				for (;;)
				{
					id = store.Find(NULL, 0, NULL, id);
					if (!id)
						break;
					store2.Insert(id, 1);
				}
				if (store2.Save())
					Error("Cannot save `%s'.\n", path);
			}
			else if (strcmp(*arg, "--passwd") == 0 && remain >= 1)
			{
				arg++;
				passwd = (*arg && **arg) ? *arg:NULL;
			}
			else if (**arg != '-')
			{
				const char *path = *arg;

				PTP::Identity *ident = Load(path, passwd);
				if (ident)
					Show(&store, ident, path);
			}
			else
			{
				Error("Invalid option or missing arguments.\n"
				      "Usage: %s [OPTION] [FILE]\n"
				      "  NONE                 "
				      "List all certificates.\n"
				      "  FILE                 "
				      "Display a certificate.\n"
				      "  --import FILE        "
				      "Import a certificate.\n"
				      "  --export NAME FILE   "
				      "Export a certificate. \n"
				      "  --sign DAYS FILE     "
				      "Sign a certificate.\n"
				      "  --remove NAME        "
				      "Remove a certificate.\n"
				      "  --setup NAME         "
				      "Setup the local certificate.\n"
				      "  --load FILE          "
				      "Load the entire store.\n"
				      "  --save FILE          "
				      "Save the entire store.\n"
				      "  --passwd PASSWD      "
				      "Set import/export password.\n",
				      prog);
				break;
			}
		}
	}

	return 0;
}
