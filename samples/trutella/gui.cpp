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
 * gui: Win32 GUI Trutella interface.
 */
#include "gui.h"

CTrutellaApp g_app;

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,		// status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_wndStatusBar.Create(this)
	    || !m_wndStatusBar.SetIndicators(
		    indicators,
		    sizeof(indicators) / sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;
	}

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	return CFrameWnd::PreCreateWindow(cs);
}

void CMainFrame::SetStatusText(CString s)
{
	m_wndStatusBar.SetPaneText(0, s);
}

IMPLEMENT_DYNCREATE(CPrefConnectionPage, CPropertyPage)

CPrefConnectionPage::CPrefConnectionPage()
	: CPropertyPage(CPrefConnectionPage::IDD)
{
	//{{AFX_DATA_INIT(CPrefConnectionPage)
	m_Host = _T("");
	//}}AFX_DATA_INIT
}

CPrefConnectionPage::~CPrefConnectionPage()
{
}

void CPrefConnectionPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrefConnectionPage)
	DDX_Control(pDX, IDC_HOSTS_LIST, m_ctlHostList);
	DDX_Text(pDX, IDC_HOST, m_Host);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrefConnectionPage, CPropertyPage)
	//{{AFX_MSG_MAP(CPrefConnectionPage)
	ON_BN_CLICKED(IDC_ADDHOST, OnAddhost)
	ON_BN_CLICKED(IDC_REMOVEHOST, OnRemovehost)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CPrefConnectionPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	DWORD style = m_ctlHostList.GetExtendedStyle() | LVS_EX_FULLROWSELECT;
	m_ctlHostList.SetExtendedStyle(style);
	m_ctlHostList.InsertColumn(0, "Host", LVCFMT_LEFT, 167);
	m_ctlHostList.InsertColumn(1, "Address", LVCFMT_LEFT, 100);
	m_ctlHostList.InsertColumn(2, "Port", LVCFMT_LEFT, 50);
	
	return TRUE;
}

void CPrefConnectionPage::OnAddhost() 
{
	UpdateData(TRUE);
	if(m_Host == "")
		return;

	CWaitCursor c;
	Trut::Host *host = g_app.trut->AddHost(m_Host);
	if(!host)
	{
		MessageBox("Cannot connect to " + m_Host + ".");
		return;
	}
	AddListItem(host);
}

void CPrefConnectionPage::OnRemovehost() 
{
	POSITION pos = m_ctlHostList.GetFirstSelectedItemPosition();
	if(!pos)
		return;

	UINT nIndex = m_ctlHostList.GetNextSelectedItem(pos);
	Trut::Host *host = (Trut::Host *)m_ctlHostList.GetItemData(nIndex);
	if (!host)
		return;

	g_app.trut->RemoveHost(host);
	m_ctlHostList.DeleteItem(nIndex);
}

void CPrefConnectionPage::AddListItem(Trut::Host *host)
{
	if(!host)
		return;

	CString ip;
	g_app.FormatIPAddr(host->GetIp(), ip);
	CString port;
	port.Format("%d", host->GetPort());
	UINT item = m_ctlHostList.InsertItem(
		LVIF_TEXT | LVIF_PARAM, 
		m_ctlHostList.GetItemCount(),
		host->GetUrl(),
		0, 0, 0,
		(LPARAM)host);
	m_ctlHostList.SetItem(item, 1, LVIF_TEXT, ip, 0, 0, 0, 0);
	m_ctlHostList.SetItem(item, 2, LVIF_TEXT, port, 0, 0, 0, 0);
}

BOOL CPrefConnectionPage::OnSetActive()
{
	m_ctlHostList.DeleteAllItems();
	CTrutellaApp::HostList::iterator i = g_app.hosts.begin();
	for (i = g_app.hosts.begin();
		 i != g_app.hosts.end();
		 i++)
	{
		Trut::Host *host = *i;
		AddListItem(host);
	}
	return CPropertyPage::OnSetActive();
}

IMPLEMENT_DYNCREATE(CPrefGroups, CPropertyPage)

CPrefGroups::CPrefGroups() : CPropertyPage(CPrefGroups::IDD)
{
	//{{AFX_DATA_INIT(CPrefGroups)
	m_GroupName = _T("");
	//}}AFX_DATA_INIT
}

CPrefGroups::~CPrefGroups()
{
}

void CPrefGroups::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrefGroups)
	DDX_Control(pDX, IDC_GROUPLIST, m_ctlGroupList);
	DDX_Text(pDX, IDC_GROUPNAME, m_GroupName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPrefGroups, CPropertyPage)
	//{{AFX_MSG_MAP(CPrefGroups)
	ON_BN_CLICKED(IDC_LEAVEGROUP, OnLeavegroup)
	ON_BN_CLICKED(IDC_JOINGROUP, OnJoingroup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CPrefGroups::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	DWORD style = m_ctlGroupList.GetExtendedStyle() | LVS_EX_FULLROWSELECT;
	m_ctlGroupList.SetExtendedStyle(style);
	m_ctlGroupList.InsertColumn(0, "Name", LVCFMT_LEFT, 317);
	
	return TRUE;
}

struct JoinContext
{
	CPrefGroups *pref;
	CEvent event;
};

void CPrefGroups::OnJoingroup() 
{
	UpdateData(TRUE);
	if(m_GroupName == "")
		return;
	CWaitCursor c;

	JoinContext context;
	context.pref = this;
	g_app.trut->JoinGroup(m_GroupName,
				   2,
			       JoinCallback,
			       AcceptCallback,
			       &context);
	CSingleLock lock(&context.event);
	lock.Lock();
}

void CPrefGroups::JoinCallback(Trut::Group *group,
			       Trut::JoinStatus status,
			       void *context)
{
	JoinContext *joinctx = (JoinContext*) context;
	if (!joinctx)
		return;
	joinctx->event.SetEvent();

	CPrefGroups *pref = joinctx->pref;
	switch(status)
	{
	case Trut::JOIN_CREATED:
		pref->MessageBox("No active group members could be found, "
			   "so a new group has been created.");
		g_app.groups.push_back(group);
		pref->m_ctlGroupList.InsertItem(
			LVIF_TEXT | LVIF_PARAM, 
			pref->m_ctlGroupList.GetItemCount(),
			group->GetName(),
			0, 0, 0,
			(LPARAM) group);
		break;

	case Trut::JOIN_OK:
		g_app.groups.push_back(group);
		pref->m_ctlGroupList.InsertItem(
			LVIF_TEXT | LVIF_PARAM, 
			pref->m_ctlGroupList.GetItemCount(),
			group->GetName(),
			0, 0, 0,
			(LPARAM) group);
		break;

	default:
		pref->MessageBox("You were denied access to the group.");
		break;
	}
}

int CPrefGroups::AcceptCallback(Trut::Group *group,
				const char *id,
				void *context)
{
	HWND hwnd = g_app.m_pMainWnd ? g_app.m_pMainWnd->m_hWnd:NULL; 
	return (::MessageBox(hwnd,
			     (CString) id
			     + " is trying to join the group "
			     + (CString)group->GetName()
			     +  ".  Will you allow this person to "
			     "join your group?",
			     "Trutella",
			     MB_YESNO) == IDYES);
}

void CPrefGroups::OnLeavegroup() 
{
	POSITION pos = m_ctlGroupList.GetFirstSelectedItemPosition();
	if(!pos)
		return;

	UINT item = m_ctlGroupList.GetNextSelectedItem(pos);
	Trut::Group *group = (Trut::Group*) m_ctlGroupList.GetItemData(item);
	if (!group)
		return;
	m_ctlGroupList.DeleteItem(item);

	CTrutellaApp::ShareList::iterator i = g_app.shares.begin();
	while (i != g_app.shares.end())
	{
		Trut::Shared *shared = *i;
		i++;
		if (shared && shared->GetGroup() == group)
			g_app.shares.remove(shared);
	}

	g_app.groups.remove(group);
	g_app.trut->LeaveGroup(group);
}

BOOL CPrefGroups::OnSetActive() 
{
	m_ctlGroupList.DeleteAllItems();
	CTrutellaApp::GroupList::iterator i;
	for (i = g_app.groups.begin();
		 i != g_app.groups.end();
		 i++)
	{
		Trut::Group *group = *i;
		if (group)
			m_ctlGroupList.InsertItem(
				LVIF_TEXT | LVIF_PARAM, 
				m_ctlGroupList.GetItemCount(),
				group->GetName(),
				0, 0, 0,
				(LPARAM)group);
	}
	return CPropertyPage::OnSetActive();
}

IMPLEMENT_DYNCREATE(CPrefSharing, CPropertyPage)

CPrefSharing::CPrefSharing() : CPropertyPage(CPrefSharing::IDD)
{
	//{{AFX_DATA_INIT(CPrefSharing)
	m_ext = _T("");
	m_groupItem = -1;
	//}}AFX_DATA_INIT
	m_current = NULL;
}

CPrefSharing::~CPrefSharing()
{
}

void CPrefSharing::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrefSharing)
	DDX_Control(pDX, IDC_RESCANSHARE, m_ctlRescanBtn);
	DDX_Control(pDX, IDC_REMOVESHARE, m_ctlRemoveBtn);
	DDX_Control(pDX, IDC_ADDSHARE, m_ctlAddBtn);
	DDX_Control(pDX, IDC_SHAREEXT, m_ctlExtensions);
	DDX_Control(pDX, IDC_SHAREGROUP, m_ctlGroup);
	DDX_Control(pDX, IDC_SHARELIST, m_ctlShareList);
	DDX_Text(pDX, IDC_SHAREEXT, m_ext);
	DDX_CBIndex(pDX, IDC_SHAREGROUP, m_groupItem);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrefSharing, CPropertyPage)
	//{{AFX_MSG_MAP(CPrefSharing)
	ON_BN_CLICKED(IDC_ADDSHARE, OnAddshare)
	ON_BN_CLICKED(IDC_REMOVESHARE, OnRemoveshare)
	ON_WM_DESTROY()
	ON_NOTIFY(NM_CLICK, IDC_SHARELIST, OnClickSharelist)
	ON_BN_CLICKED(IDC_RESCANSHARE, OnRescanshare)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CPrefSharing::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	DWORD style = m_ctlShareList.GetExtendedStyle() | LVS_EX_FULLROWSELECT;
	m_ctlShareList.SetExtendedStyle(style);
	m_ctlShareList.InsertColumn(0, "Path", LVCFMT_LEFT, 317);

	return TRUE;
}

void CPrefSharing::OnClickSharelist(NMHDR* pNMHDR, LRESULT* pResult)
{
	SaveCurrent();

	POSITION pos = m_ctlShareList.GetFirstSelectedItemPosition();
	if (!pos)
		return;
	UINT selected = m_ctlShareList.GetNextSelectedItem(pos);
	Trut::Shared *shared = (Trut::Shared*)
		m_ctlShareList.GetItemData(selected);
	if (!shared)
		return;

	m_current = shared;
	m_ext = shared->GetExt();
	m_groupItem = m_ctlGroup.GetCount() - 1;
	UpdateData(FALSE);
	if (shared->GetGroup())
		m_ctlGroup.SelectString(-1, shared->GetGroup()->GetName());

	m_ctlGroup.EnableWindow();
	m_ctlExtensions.EnableWindow();
	m_ctlAddBtn.EnableWindow();
	m_ctlRescanBtn.EnableWindow();
	m_ctlRemoveBtn.EnableWindow();
	
	*pResult = 0;
}

void CPrefSharing::OnAddshare() 
{
	SaveCurrent();

	char *path = GetPath();
	if (!path)
		return;

	CString ext;
	ext.LoadString(IDS_SHAREEXT_DEFAULTS);
	Trut::Shared *shared = g_app.trut->AddShared(path, ext, NULL);
	if (!shared)
		return;

	g_app.shares.push_back(shared);
	m_ctlShareList.InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_STATE,
				  m_ctlShareList.GetItemCount(),
				  shared->GetPath(),
				  LVIS_SELECTED, LVIS_SELECTED, 0,
				  (LPARAM) shared);

	m_current = shared;
	m_ext = shared->GetExt();
	m_groupItem = m_ctlGroup.GetCount() - 1;
	UpdateData(FALSE);

	m_ctlGroup.EnableWindow();
	m_ctlExtensions.EnableWindow();
	m_ctlRemoveBtn.EnableWindow();
}

void CPrefSharing::OnRemoveshare()
{
	SaveCurrent();

	POSITION pos = m_ctlShareList.GetFirstSelectedItemPosition();
	if (!pos)
		return;
	UINT item = m_ctlShareList.GetNextSelectedItem(pos);
	Trut::Shared *shared = (Trut::Shared*)
		m_ctlShareList.GetItemData(item);
	if (!shared)
		return;

	m_ctlShareList.DeleteItem(item);
	g_app.shares.remove(shared);
	g_app.trut->RemoveShared(shared);

	if (m_ctlShareList.GetItemCount() <= 0)
	{
		m_ctlGroup.EnableWindow(FALSE);
		m_ctlExtensions.EnableWindow(FALSE);
		m_ctlRemoveBtn.EnableWindow(FALSE);
	}
}

void CPrefSharing::OnDestroy() 
{
	g_app.trut->RescanAllShared();
	CPropertyPage::OnDestroy();
}


void CPrefSharing::OnRescanshare() 
{
	SaveCurrent();
	g_app.trut->RescanAllShared();
}

BOOL CPrefSharing::OnSetActive() 
{
	m_ctlShareList.DeleteAllItems();
	CTrutellaApp::ShareList::iterator i;
	for(i = g_app.shares.begin();
	    i != g_app.shares.end();
	    i++)
	{
		Trut::Shared *shared = *i;
		if (shared)
			m_ctlShareList.InsertItem(LVIF_TEXT | LVIF_PARAM, 
						  m_ctlShareList.GetItemCount(),
						  shared->GetPath(),
						  0, 0, 0,
						  (LPARAM) shared);
	}
	
	m_ctlGroup.ResetContent();
	m_ctlGroup.AddString("All of GnutellaNet");
	m_ctlGroup.SetItemDataPtr(0, NULL);

	CTrutellaApp::GroupList::iterator j;
	for(j = g_app.groups.begin();
	    j != g_app.groups.end();
	    j++)
	{
			Trut::Group *group = *j;
			int item = m_ctlGroup.AddString(group->GetName());
			m_ctlGroup.SetItemDataPtr(item, group);
	}

	m_ext = _T("");
	m_groupItem = -1;
	UpdateData(FALSE);

	m_ctlGroup.EnableWindow(FALSE);
	m_ctlExtensions.EnableWindow(FALSE);
	m_ctlRemoveBtn.EnableWindow(FALSE);

	return CPropertyPage::OnSetActive();
}

BOOL CPrefSharing::OnKillActive() 
{
	SaveCurrent();
	return CPropertyPage::OnKillActive();
}

char *CPrefSharing::GetPath()
{
	BROWSEINFO bi;
	bi.hwndOwner = m_hWnd;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = NULL;
	bi.lpszTitle = _T("Select a Directory to Share");
	bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
	bi.lpfn = NULL;
	bi.lParam = 0;

	LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
	char path[MAX_PATH];
	if (!pidl || !::SHGetPathFromIDList(pidl, path))
		return NULL;

	int size = strlen(path);
	if(path[size - 1] == '\\')
		path[size - 1] = '\0';

	return strdup(path);
}

void CPrefSharing::SaveCurrent()
{
	Trut::Shared *shared = m_current;
	if (!shared)
		return;
	m_current = NULL;

	char ext[128];
	ext[0] = '\0';
	int len = m_ctlExtensions.GetLine(0, ext, sizeof(ext));
	if (len >= 0)
		ext[len] = '\0';

	Trut::Group *group = NULL;
	UINT item = m_ctlGroup.GetCurSel();
	if (item == CB_ERR)
		return;

	group = (Trut::Group*) m_ctlGroup.GetItemDataPtr(item);
	g_app.trut->UpdateShared(shared, ext, group);
}

IMPLEMENT_DYNAMIC(CPrefSheet, CPropertySheet)

CPrefSheet::CPrefSheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CPrefSheet::CPrefSheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

CPrefSheet::~CPrefSheet()
{
}


BEGIN_MESSAGE_MAP(CPrefSheet, CPropertySheet)
	//{{AFX_MSG_MAP(CPrefSheet)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CTrutellaApp, CWinApp)
	//{{AFX_MSG_MAP(CTrutellaApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_PREFS, OnFilePrefs)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()

CTrutellaApp::CTrutellaApp()
{
}

BOOL CTrutellaApp::InitInstance()
{
	CWaitCursor c;

	m_InitFailed = FALSE;

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#if 0
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	// Call this when linking to MFC statically
	Enable3dControlsStatic();
#endif
#endif

	// Change the registry key under which our settings are stored.
	SetRegistryKey(_T("Intel"));

	// Load standard INI file options (including MRU)
	LoadStdProfileSettings(0);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CTrutellaDoc),
		RUNTIME_CLASS(CMainFrame), // main SDI frame window
		RUNTIME_CLASS(CTrutellaView));
	AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
	{
		m_InitFailed = TRUE;
		return FALSE;
	}

	// Create directories
	CreateDirectory("C:\\Ptl", NULL);
	CreateDirectory("C:\\Ptl\\Downloads", NULL);
	SetCurrentDirectory("C:\\Ptl\\Downloads");

	// Load certificates
	store = new PTP::Store(HKEY_CURRENT_USER,
			       "Software\\PTL\\Cert",
			       NULL,
			       NULL);
	if (store->Load())
	{
		m_InitFailed = TRUE;
		return FALSE;
	}

	// Load identity settings
	PTP::Identity *local = store->Find(NULL, 1);
	if (local)
		m_IdFriendlyName = local->GetName();

	trut = new Trut(store);
	
	// Start listening on the default port
	trut->SetOpenCallback(OpenCallback);
	trut->SetCloseCallback(CloseCallback);
	trut->AddPort(0);
	
	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	((CMainFrame *)m_pMainWnd)->GetActiveDocument()->UpdateAllViews(NULL);

	// Try to re-establish host connections
	// Load registry settings and restore connections
	UINT i, n;
	CString Name, Key;
	n = GetProfileInt("Hosts", "Count", 0);
	for(i = 0; i < n; i++)
	{
		Key.Format("%d", i);
		Name = GetProfileString("Hosts", Key, "");
		if(Name != "")
		{
			SetStatusText("Connecting to " + Name + "...");
			Trut::Host *host = trut->AddHost(Name);
			if(!host)
			{
				MessageBox(m_pMainWnd->m_hWnd, 
					   "Could not restore connection to "
					   + Name
					   + ".",
					   "Trutella",
					   MB_OK);
			}
		}
	}
	
	// Final aesthetics
	SetStatusText("Ready");
	m_pMainWnd->SetFocus();
	
	return TRUE;
}

int CTrutellaApp::ExitInstance() 
{
	if(!m_InitFailed)
	{
		WriteProfileInt("Hosts", "Count", hosts.size());
		HostList::iterator hostPos;
		UINT i;

		for(hostPos = hosts.begin(), i = 0;
		    hostPos != hosts.end();
		    hostPos++, i++)
		{
			CString key;
			key.Format("%d", i);
			WriteProfileString("Hosts", key, (*hostPos)->GetUrl());
		}
	}

	delete trut;
	delete store;
	return CWinApp::ExitInstance();
}

void CTrutellaApp::OpenCallback(Trut::Host *host, void **context)
{
	if (host->GetDirection() == PTP::Net::Connection::OUTBOUND)
		g_app.hosts.push_back(host);
}

void CTrutellaApp::CloseCallback(Trut::Host *host, void *context)
{
	if (host->GetDirection() == PTP::Net::Connection::OUTBOUND)
		g_app.hosts.remove(host);
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CTrutellaApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

void CTrutellaApp::OnFilePrefs() 
{
	// Build and display the preferences dialog
	CPrefSheet dlg("Trutella Preferences");
	CPrefConnectionPage pgConnections;
	CPrefSharing pgSharing;
	CPrefGroups pgGroups;

	dlg.AddPage(&pgConnections);
	dlg.AddPage(&pgSharing);
	dlg.AddPage(&pgGroups);

	if(dlg.DoModal() == IDOK)
	{
		// Get data from the dialog objects...
	}

	// Update the application window with changes
	CMainFrame *pMainFrame = (CMainFrame*)GetMainWnd();
	pMainFrame->GetActiveDocument()->UpdateAllViews(NULL);

}

void CTrutellaApp::FormatIPAddr(UINT ip, CString &str)
{
	str.Format("%d.%d.%d.%d",
		   (ip & 0xff000000) >> 24, (ip & 0xff0000) >> 16, 
		   (ip & 0xff00) >> 8, ip & 0xff);
}

void CTrutellaApp::SetStatusText(CString s)
{
	CMainFrame *pMainFrame = (CMainFrame *)AfxGetMainWnd();
	if(pMainFrame)
		pMainFrame->SetStatusText(s);
}

IMPLEMENT_DYNCREATE(CTrutellaDoc, CDocument)

BEGIN_MESSAGE_MAP(CTrutellaDoc, CDocument)
	//{{AFX_MSG_MAP(CTrutellaDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CTrutellaDoc::CTrutellaDoc()
{
	// TODO: add one-time construction code here

}

CTrutellaDoc::~CTrutellaDoc()
{
}

BOOL CTrutellaDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	SetTitle(g_app.m_IdFriendlyName);
	return TRUE;
}

void CTrutellaDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

IMPLEMENT_DYNCREATE(CTrutellaView, CFormView)

BEGIN_MESSAGE_MAP(CTrutellaView, CFormView)
	//{{AFX_MSG_MAP(CTrutellaView)
	ON_BN_CLICKED(IDC_SEARCH, OnSearch)
	ON_WM_DESTROY()
	ON_NOTIFY(NM_DBLCLK, IDC_SEARCHRESULTS, OnDblclkSearchresults)
	ON_BN_CLICKED(IDC_KILLXFER, OnKillxfer)
	ON_BN_CLICKED(IDC_REMOVEERRORS, OnRemoveerrors)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CTrutellaView::CTrutellaView()
	: CFormView(CTrutellaView::IDD)
{
	//{{AFX_DATA_INIT(CTrutellaView)
	m_SearchText = _T("");
	m_nGroupID = -1;
	m_nXferMode = -1;
	//}}AFX_DATA_INIT
	// TODO: add construction code here

}

CTrutellaView::~CTrutellaView()
{
}

void CTrutellaView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTrutellaView)
	DDX_Control(pDX, IDC_GROUP, m_ctlGroupCombo);
	DDX_Control(pDX, IDC_XFERSEL, m_ctlXferMode);
	DDX_Control(pDX, IDC_XFERS, m_ctlXferList);
	DDX_Control(pDX, IDC_SEARCHRESULTS, m_ctlSearchResults);
	DDX_Text(pDX, IDC_SEARCHTEXT, m_SearchText);
	DDX_CBIndex(pDX, IDC_GROUP, m_nGroupID);
	DDX_CBIndex(pDX, IDC_XFERSEL, m_nXferMode);
	//}}AFX_DATA_MAP
}

BOOL CTrutellaView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs
	return CFormView::PreCreateWindow(cs);
}

void CTrutellaView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();
	GetParentFrame()->RecalcLayout();
	ResizeParentToFit();

	// Set up list view columns
	DWORD dwStyle;
	dwStyle = m_ctlSearchResults.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;
	m_ctlSearchResults.SetExtendedStyle(dwStyle);
	dwStyle = m_ctlXferList.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;
	m_ctlXferList.SetExtendedStyle(dwStyle);
	m_ctlSearchResults.InsertColumn(0, "Path", LVCFMT_LEFT, 250);
	m_ctlSearchResults.InsertColumn(1, "Size", LVCFMT_LEFT, 100);
	m_ctlSearchResults.InsertColumn(2, "Group", LVCFMT_LEFT, 150);

	m_ctlXferList.InsertColumn(0, "Path", LVCFMT_LEFT, 250);
	m_ctlXferList.InsertColumn(1, "Transferred", LVCFMT_LEFT, 100);
	m_ctlXferList.InsertColumn(2, "Status", LVCFMT_LEFT, 150);
	m_ctlXferMode.SetCurSel(0);
	m_ctlXferMode.EnableWindow(FALSE);

}

void CTrutellaView::OnSearch() 
{
	UpdateData(TRUE);

	if(m_SearchText == "")
		return;

	// Remove current search results
	CleanSearchResults();

	// Get group pointer referenced by current combobox item,
	// or NULL for the world.
	UINT nIndex = m_ctlGroupCombo.GetCurSel();
	Trut::Group *group = NULL;
	if(nIndex != CB_ERR)
		group = (Trut::Group*)m_ctlGroupCombo.GetItemDataPtr(nIndex);

	g_app.trut->Search(m_SearchText,
			    SearchCallback,
			    &m_ctlSearchResults,
			    group);
}

void CTrutellaView::SearchCallback(Trut::File *file, void *context)
{
	static CCriticalSection lock;

	lock.Lock();

	CListCtrl *results = (CListCtrl*) context;
	UINT item = results->GetItemCount();
	results->InsertItem(LVIF_TEXT | LVIF_PARAM,
			    item,
			    file->GetName(), 
			    0, 0, 0,
			    (LPARAM) file);

	CString size;
	size.Format("%d", file->GetSize());
	results->SetItem(item, 1, LVIF_TEXT, size, 0, 0, 0, 0);
	const char *name = (file->GetGroup()
			    ? file->GetGroup()->GetName():"None (public)");
	results->SetItem(item, 2, LVIF_TEXT, name, 0, 0, 0, 0);

	lock.Unlock();
}

void CTrutellaView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	GetDocument()->SetTitle(g_app.m_IdFriendlyName);

	m_ctlGroupCombo.ResetContent();
	m_ctlGroupCombo.AddString("All of GnutellaNet");
	m_ctlGroupCombo.SetItemDataPtr(0, NULL);

	CTrutellaApp::GroupList::iterator i;
	for(i = g_app.groups.begin(); i != g_app.groups.end(); i++)
	{
		Trut::Group *group = *i;
		int item = m_ctlGroupCombo.AddString(group->GetName());
		m_ctlGroupCombo.SetItemDataPtr(item, group);
	}
	m_ctlGroupCombo.SetCurSel(m_ctlGroupCombo.GetCount() - 1);
}

void CTrutellaView::CleanSearchResults()
{
	for(int i = 0; i < (int) m_ctlSearchResults.GetItemCount(); i++)
	{
		Trut::File *file = (Trut::File*)
			m_ctlSearchResults.GetItemData(i);
		delete file;
	}
	m_ctlSearchResults.DeleteAllItems();

}

void CTrutellaView::OnDestroy() 
{
	CFormView::OnDestroy();
	CleanSearchResults();	
}

void CTrutellaView::OnDblclkSearchresults(NMHDR* pNMHDR, LRESULT* pResult) 
{
	POSITION pos = m_ctlSearchResults.GetFirstSelectedItemPosition();
	if (!pos)
		return;

	int selected = m_ctlSearchResults.GetNextSelectedItem(pos);
	Trut::File *file = (Trut::File *)
		m_ctlSearchResults.GetItemData(selected);
	if (!file)
		return;
	
	TransferCallback(file, Trut::GET_OK, NULL, 0, &m_ctlXferList);
	g_app.trut->Get(file,
			file->GetName(),
			TransferCallback,
			&m_ctlXferList);
	
	*pResult = 0;
}

void CTrutellaView::TransferCallback(Trut::File *file,
				     Trut::GetStatus status,
				     BYTE *data,
				     unsigned long size,
				     void *context)
{
	if(!file || !context)
		return;
	
	static CCriticalSection lock;
	lock.Lock();
	
	CListCtrl *list = (CListCtrl*)context;

	LVFINDINFO info;
	info.flags = LVFI_PARAM;
	info.lParam = (LPARAM)file;

	int item = list->FindItem(&info);
	if(item == -1)
	{
		// not found, add a new item
		item = list->InsertItem(
			LVIF_TEXT | LVIF_PARAM, 
			list->GetItemCount(),
			file->GetName(),
			0, 0, 0,
			(LPARAM) file);
	}

	// Update subitems
	CString strSize, strStat;
	strSize.Format("%d", size);
	switch(status)
	{
	case Trut::GET_OK:
		strStat = "Transferring";
		break;
	case Trut::GET_DONE:
		strStat = "Done";
		break;
	default:
		strStat = "Error";
		break;
	}

	list->SetItem(item, 1, LVIF_TEXT, strSize, 0, 0, 0, 0);
	list->SetItem(item, 2, LVIF_TEXT, strStat, 0, 0, 0, 0);

	// Mark transfer as finished by disassociating it from the Trut::File
	if(status == Trut::GET_DONE || status == Trut::GET_ERROR)
		list->SetItemData(item, NULL);

	lock.Unlock();
}

void CTrutellaView::OnKillxfer() 
{
	POSITION pos = m_ctlXferList.GetFirstSelectedItemPosition();
	if(!pos)
	{
		MessageBox("Please select the transfer you'd like to stop.");
		return;
	}

	int selected = m_ctlXferList.GetNextSelectedItem(pos);
	Trut::File *file = (Trut::File*)m_ctlXferList.GetItemData(selected);
	if(file)
	{
		g_app.trut->GetStop(file);
		m_ctlXferList.DeleteItem(selected);
	}
	else
	{
		MessageBox("This transfer has already completed.");
	}
}

void CTrutellaView::OnRemoveerrors() 
{
	CWaitCursor c;
	int nItem;

	for(nItem = m_ctlXferList.GetItemCount() - 1; nItem >= 0; nItem--)
	{
		Trut::File *file = (Trut::File*)
			m_ctlXferList.GetItemData(nItem);
		if(!file)
			m_ctlXferList.DeleteItem(nItem);
	}	
}
