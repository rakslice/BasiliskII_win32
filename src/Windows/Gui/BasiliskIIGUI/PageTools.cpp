// PageTools.cpp : implementation file
//

#include "stdafx.h"
#include "BasiliskIIGUI.h"
#include "PageTools.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPageTools property page

IMPLEMENT_DYNCREATE(CPageTools, CPropertyPage)

CPageTools::CPageTools() : CPropertyPage(CPageTools::IDD)
{
	//{{AFX_DATA_INIT(CPageTools)
	m_is_on_top = FALSE;
	m_gui_autorestart = -1;
	m_lowmem_cache = FALSE;
	//}}AFX_DATA_INIT
}

CPageTools::~CPageTools()
{
}

void CPageTools::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPageTools)
	DDX_Check(pDX, IDC_TOOLS_IS_ON_TOP, m_is_on_top);
	DDX_CBIndex(pDX, IDC_TOOLS_GUI_AUTORESTART, m_gui_autorestart);
	DDX_Check(pDX, IDC_EXPERIMENT_LOWMEM_CACHE, m_lowmem_cache);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPageTools, CPropertyPage)
	//{{AFX_MSG_MAP(CPageTools)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPageTools message handlers

