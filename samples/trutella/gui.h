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

#ifndef __GUI_H__
#define __GUI_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN

#include <afxwin.h>   // MFC core and standard components
#include <afxext.h>   // MFC extensions
#include <afxdisp.h>  // MFC Automation classes
#include <afxdtctl.h> // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>   // MFC support for Windows Common Controls
#endif
#include <afxmt.h>    // MFC support for synchronization

#include <list>
#include "resource.h"
#include "trut.h"

using namespace std;

class CMainFrame : public CFrameWnd
{
protected:
	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)

public:
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

	void SetStatusText(CString s);
	virtual ~CMainFrame();

protected:
	CStatusBar  m_wndStatusBar;

	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CPrefConnectionPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CPrefConnectionPage)

public:
	CPrefConnectionPage();
	~CPrefConnectionPage();

	//{{AFX_DATA(CPrefConnectionPage)
	enum { IDD = IDD_PREFS_CONNECTIONS };
	CListCtrl	m_ctlHostList;
	CString	m_Host;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CPrefConnectionPage)
public:
	virtual BOOL OnSetActive();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:
	void AddListItem(Trut::Host *host);

	//{{AFX_MSG(CPrefConnectionPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnAddhost();
	afx_msg void OnRemovehost();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CPrefGroups : public CPropertyPage
{
	DECLARE_DYNCREATE(CPrefGroups)

public:
	CPrefGroups();
	~CPrefGroups();

	//{{AFX_DATA(CPrefGroups)
	enum { IDD = IDD_PREFS_GROUPS };
	CListCtrl	m_ctlGroupList;
	CString	m_GroupName;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CPrefGroups)
public:
	virtual BOOL OnSetActive();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:
	static void JoinCallback(Trut::Group *group,
				 Trut::JoinStatus status,
				 void *context);
	static int AcceptCallback(Trut::Group *group,
				  const char *id,
				  void *context);

	//{{AFX_MSG(CPrefGroups)
	virtual BOOL OnInitDialog();
	afx_msg void OnLeavegroup();
	afx_msg void OnJoingroup();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CPrefSharing : public CPropertyPage
{
	DECLARE_DYNCREATE(CPrefSharing)

public:
	CPrefSharing();
	~CPrefSharing();

	//{{AFX_DATA(CPrefSharing)
	enum { IDD = IDD_PREFS_SHARING };
	CButton		m_ctlRescanBtn;
	CButton		m_ctlRemoveBtn;
	CButton		m_ctlAddBtn;
	CEdit		m_ctlExtensions;
	CComboBox	m_ctlGroup;
	CListCtrl	m_ctlShareList;
	CString		m_ext;
	int			m_groupItem;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CPrefSharing)
public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CPrefSharing)
	virtual BOOL OnInitDialog();
	afx_msg void OnAddshare();
	afx_msg void OnRemoveshare();
	afx_msg void OnDestroy();
	afx_msg void OnClickSharelist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRescanshare();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	char *GetPath();
	void SaveCurrent();

	Trut::Shared *m_current;
};

class CPrefSheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CPrefSheet)

public:
	CPrefSheet(UINT nIDCaption,
		   CWnd* pParentWnd = NULL,
		   UINT iSelectPage = 0);
	CPrefSheet(LPCTSTR pszCaption,
		   CWnd* pParentWnd = NULL,
		   UINT iSelectPage = 0);

	virtual ~CPrefSheet();

protected:
	DECLARE_MESSAGE_MAP()
};

class CTrutellaApp : public CWinApp
{
public:
	typedef list<Trut::Host *> HostList;
	typedef list<Trut::Group *> GroupList;
	typedef list<Trut::Shared*> ShareList;

	void SetStatusText(CString s);
	CString m_IdFriendlyName;
	void FormatIPAddr(UINT ip, CString &str);
	CTrutellaApp();
	
	PTP::Store *store;
	Trut *trut;
	HostList hosts;
	GroupList groups;
	ShareList shares;

	//{{AFX_VIRTUAL(CTrutellaApp)
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTrutellaApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFilePrefs();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	static void OpenCallback(Trut::Host *host, void **context);
	static void CloseCallback(Trut::Host *host, void *context);

private:
	BOOL m_InitFailed;
};

class CTrutellaDoc : public CDocument
{
protected:
	CTrutellaDoc();
	DECLARE_DYNCREATE(CTrutellaDoc)

public:
	//{{AFX_VIRTUAL(CTrutellaDoc)
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

	virtual ~CTrutellaDoc();

protected:
	DECLARE_MESSAGE_MAP()
};

class CTrutellaView : public CFormView
{
protected:
	CTrutellaView();
	DECLARE_DYNCREATE(CTrutellaView)

public:
	//{{AFX_DATA(CTrutellaView)
	enum { IDD = IDD_TRUTELLA_FORM };
	CComboBox	m_ctlGroupCombo;
	CComboBox	m_ctlXferMode;
	CListCtrl	m_ctlXferList;
	CListCtrl	m_ctlSearchResults;
	CString	m_SearchText;
	int		m_nGroupID;
	int		m_nXferMode;
	//}}AFX_DATA

public:
	CTrutellaDoc* GetDocument();

	//{{AFX_VIRTUAL(CTrutellaView)
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL

public:
	virtual ~CTrutellaView();

protected:
	static void SearchCallback(Trut::File *file, void *context);
	static void TransferCallback(Trut::File *file,
				     Trut::GetStatus status,
				     BYTE *data,
				     unsigned long size,
				     void *context);

protected:
	void CleanSearchResults();
	//{{AFX_MSG(CTrutellaView)
	afx_msg void OnSearch();
	afx_msg void OnDestroy();
	afx_msg void OnDblclkSearchresults(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillxfer();
	afx_msg void OnRemoveerrors();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline CTrutellaDoc *
CTrutellaView::GetDocument()
{
	return (CTrutellaDoc*)m_pDocument;
}

#endif // __GUI_H__
