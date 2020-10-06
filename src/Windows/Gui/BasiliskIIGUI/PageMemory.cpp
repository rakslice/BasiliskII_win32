// PageMemory.cpp : implementation file
//

#include "stdafx.h"
#include "BasiliskIIGUI.h"
#include "PageMemory.h"
#include "sysdeps.h"
#include "util_windows.h"
#include "rom_patches.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPageMemory property page

IMPLEMENT_DYNCREATE(CPageMemory, CPropertyPage)

CPageMemory::CPageMemory() : CPropertyPage(CPageMemory::IDD)
{
	//{{AFX_DATA_INIT(CPageMemory)
	m_rom_path = _T("");
	m_ram_size = 0;
	m_rom_type = _T("");
	m_checksum = _T("");
	m_checksum_calc = _T("");
	//}}AFX_DATA_INIT
}

CPageMemory::~CPageMemory()
{
}

void CPageMemory::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPageMemory)
	DDX_Text(pDX, IDC_MEMORY_ROM_PATH, m_rom_path);
	DDV_MaxChars(pDX, m_rom_path, 255);
	DDX_Text(pDX, IDC_MEMORY_RAM_SIZE, m_ram_size);
	DDV_MinMaxInt(pDX, m_ram_size, 4, 8192);
	DDX_Text(pDX, IDC_MEMORY_ROM_TYPE, m_rom_type);
	DDX_Text(pDX, IDC_MEMORY_ROM_CHECKSUM, m_checksum);
	DDV_MaxChars(pDX, m_checksum, 8);
	DDX_Text(pDX, IDC_MEMORY_ROM_CHECKSUM_CALC, m_checksum_calc);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPageMemory, CPropertyPage)
	//{{AFX_MSG_MAP(CPageMemory)
	ON_BN_CLICKED(IDC_MEMORY_ROM_BROWSE, OnMemoryRomBrowse)
	ON_EN_UPDATE(IDC_MEMORY_ROM_PATH, OnUpdateMemoryRomPath)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPageMemory message handlers

void CPageMemory::check_rom_size() 
{
	CFile f;
	char *rom;
	uint32 size, csum1 = 0, csum2 = 0;

	// m_os8_ok = "Unknown";
	m_rom_type = "Unknown";
	m_checksum = "Unknown";
	m_checksum_calc = "Unknown";

	if(f.Open(m_rom_path,CFile::modeRead)) {
		size = f.SeekToEnd();
		/*
		if(size == 1048576) {
			m_os8_ok = "Yes";
		} else {
			m_os8_ok = "No";
		}
		*/

		f.SeekToBegin();

		rom = new char [size];

		if(f.Read( rom, size ) == size) {
			switch( ntohs(*(uint16 *)(rom + 8)) ) {
				case ROM_VERSION_64K:
					m_rom_type = "Original Macintosh ROM (64KB)";
					break;
				case ROM_VERSION_PLUS:
					m_rom_type = "Mac Plus ROM (128KB)";
					break;
				case ROM_VERSION_CLASSIC:
					m_rom_type = "SE/Classic ROM (256/512KB)";
					break;
				case ROM_VERSION_II:
					m_rom_type = "Not 32-bit clean Mac II ROM (256KB)";
					break;
				case ROM_VERSION_32:
					m_rom_type = "32-bit clean Mac II ROM (512KB/1MB)";
					break;
			}
			csum1 = ntohl(*((uint32 *)rom));
			sprintf( m_checksum.GetBuffer(100), "%08X", csum1 );
			m_checksum.ReleaseBuffer();

			uint16 *r = (uint16 *)rom;
			for( uint32 i=2; i<size/2; i++ ) {
				csum2 += ntohs(r[i]);
			}

			sprintf( m_checksum_calc.GetBuffer(100), "%08X", csum2 );
			m_checksum_calc.ReleaseBuffer();
		}

		f.Close();

		delete [] rom;
	}
}

void CPageMemory::OnMemoryRomBrowse() 
{
	if(UpdateData(TRUE)) {
		CFileDialog dlg( TRUE, _T("*"), m_rom_path,
					OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
					_T("All Files|*.*||") );
		if(dlg.DoModal() == IDOK) {
			m_rom_path = dlg.GetPathName();
			check_rom_size();
			UpdateData(FALSE);
		}
	}
}

void CPageMemory::OnUpdateMemoryRomPath() 
{
	if(UpdateData(TRUE)) {
		check_rom_size();
		// GetDlgItem(IDC_MEMORY_OS8_OK)->SetWindowText( m_os8_ok );
		GetDlgItem(IDC_MEMORY_ROM_TYPE)->SetWindowText( m_rom_type );
		GetDlgItem(IDC_MEMORY_ROM_CHECKSUM)->SetWindowText( m_checksum );
		GetDlgItem(IDC_MEMORY_ROM_CHECKSUM_CALC)->SetWindowText( m_checksum_calc );
	}
}
