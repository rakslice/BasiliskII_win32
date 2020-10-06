#if !defined(AFX_ASKSCSIREPLACEMENT_H__A74D55D3_290F_11D3_A917_00201881A006__INCLUDED_)
#define AFX_ASKSCSIREPLACEMENT_H__A74D55D3_290F_11D3_A917_00201881A006__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// AskSCSIReplacement.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAskSCSIReplacement dialog

class CAskSCSIReplacement : public CDialog
{
// Construction
public:
	CAskSCSIReplacement(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAskSCSIReplacement)
	enum { IDD = IDD_ASK_SCSI_REPLACEMENT };
	CString	m_product;
	CString	m_vendor;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAskSCSIReplacement)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAskSCSIReplacement)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ASKSCSIREPLACEMENT_H__A74D55D3_290F_11D3_A917_00201881A006__INCLUDED_)
