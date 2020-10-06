/*
 *  main_windows.cpp - Startup code for Win32
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <process.h>

#include "sysdeps.h"
#include "readcpu.h"
#include "cpu_emulation.h"
#include "memory.h"
#include "newcpu.h"
#include "sys.h"
#include "xpram.h"
#include "timer.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "scsi.h"
#include "audio.h"
#include "video.h"
#include "serial.h"
#include "ether.h"
#include "clip.h"
#include "rom_patches.h"
#include "prefs.h"
#include "user_strings.h"
#include "version.h"
#include "main.h"
#include "adb.h"
#include "main_windows.h"
#include "threads_windows.h"
#include "video_windows.h"
#include "timer_windows.h"
#include "xpram_windows.h"
#include "desktop_windows.h"
#include "sys_windows.h"
#include "prefs_windows.h"
#include "resource.h"
#include "keyboard_windows.h"
#include "check_windows.h"
#include "kernel_windows.h"
#include "audio_windows.h"
#include "fpu.h"
#include "experiment_windows.h"
#include "compiler.h"
#include "extfs.h"
#include "clip_windows.h"
#include "counter.h"
#include "startupsound.h"
#include "mem_limits.h"


#include <dbt.h>
#include <windowsx.h>

#define DEBUG 0
#include "debug.h"

#define NT_PROFILE_TIMES 0


// #include <zmouse.h>
#define MSH_MOUSEWHEEL "MSWHEEL_ROLLMSG"
static UINT m_mousewheel_old_msg = 0;


// Constants
const char ROM_FILE_NAME[] = "ROM";

// From newcpu.cpp
extern "C" int quit_program;

// CPU and FPU type
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing = false;
uint32 ROM_checksum = 0;

uint32 InterruptFlags = 0;

// os flags are set up in initialize()
int win_os;
int win_os_major;

// TRUE if older than OSR2
BOOL win_os_old = FALSE;

#ifdef OPTIMIZED_8BIT_MEMORY_ACCESS
BOOL mem_8_only = TRUE;
extern "C" uae_u32 MEMBaseDiff = 0;
#ifdef SWAPPED_ADDRESS_SPACE
uae_u32 MEMBaseLongTop = 0;
uae_u32 MEMBaseWordTop = 0;
uae_u32 MEMBaseByteTop = 0;
#endif
#else
BOOL mem_8_only = FALSE;
#endif

bool m_ROM_is_protected = true;


BOOL timer_running = TRUE;
BOOL one_sec_running = TRUE;

HWND hMainWnd = 0;
HWND hAuxWnd = 0;
HINSTANCE hInst = 0;

#define IDM_MAINWINDOW_ABOUT				0x7000
#define IDM_MAINWINDOW_ADB					0x7010
#define IDM_MAINWINDOW_MEDIA_ALL		0x7020
#define IDM_MAINWINDOW_MEDIA_FLOPPY	0x7030
#define IDM_MAINWINDOW_MEDIA_CD			0x7040
#define IDM_MAINWINDOW_MEDIA_HD			0x7050
#define IDM_MAINWINDOW_OS8_MOUSE		0x7060
#define IDM_MAINWINDOW_ROM_PROTECT	0x7070

// mouse and keyboard
static bool capturing = false;
static bool dragging = false;
int m_right_mouse = 0;
int m_show_cursor = 1;
static bool m_menu_clicked = false;
static bool m_os8_mouse = true;
int m_disable_internal_wait = 0;

enum { MOUSEWHEEL_HORIZONTAL, MOUSEWHEEL_VERTICAL };
static int16 m_mousewheeldirection = MOUSEWHEEL_VERTICAL;
static int16 m_mousewheelmode = 1;
static int16 m_mousewheellines = 3;
static bool m_mouse_wheel_reverse_x = false;
static bool m_mouse_wheel_reverse_y = false;
static int16 m_mouse_wheel_click_mode = 0;
static char m_mouse_wheel_cust_00[257];
static char m_mouse_wheel_cust_01[257];
static char m_mouse_wheel_cust_10[257];
static char m_mouse_wheel_cust_11[257];
static int16 m_mousemovementmode = 0;

bool m_menu_select = false;

bool m_use_alt_escape = false;
bool m_use_alt_tab = false;
bool m_use_control_escape = false;
bool m_use_alt_space = false;

bool m_debug_disable_accurate_timer = false;

static int16 m_gui_autorestart = 0;


// Keep track what Mac keys are down so we can make them up when we lose focus.
#define MAX_KEYS 256
static BYTE mac_keys_down[MAX_KEYS];

// Media detection
UINT registered_media_hotkey	= VK_F11;
UINT registered_floppy_hotkey = VK_F8;
UINT registered_cd_hotkey			= VK_F9;
UINT registered_hd_hotkey			= VK_F10;

// TODO: put these into global thread table
HANDLE ether_th1 = 0;
HANDLE ether_th2 = 0;

char *ini_file_name = "BasiliskII.ini";

// local forwards
unsigned int WINAPI cpu_thread(LPVOID param);
unsigned int WINAPI tick_func_inaccurate(LPVOID param);
unsigned int WINAPI tick_func_accurate(LPVOID param);
unsigned int WINAPI tick_func_inaccurate_adb(LPVOID param);
unsigned int WINAPI tick_func_accurate_adb(LPVOID param);
unsigned int WINAPI one_sec_func(LPVOID param);
unsigned int WINAPI old_dt_func(LPVOID param);

static void launch_gui()
{
	if(m_gui_autorestart) {
		HWND w = FindWindow(0,"Basilisk II preferences");
		if(w) {
			if(m_gui_autorestart==1) {
				ShowWindow( w, SW_SHOWNORMAL );
				BringWindowToTop( w );
			}
		} else {
			char path[_MAX_PATH], *p;
			GetModuleFileName( (HMODULE)hInst, path, sizeof(path) );
			p = strrchr( path, '\\' );
			if(p) {
				*(++p) = 0;
				strcat( path, "BasiliskIIGUI.exe" );
			}
			if(m_gui_autorestart==1) {
				WinExec( path, SW_SHOWNORMAL );
			} else {
				WinExec( path, SW_SHOWMINNOACTIVE );
			}
		}
	}
}

static void do_mac_key_down( int key )
{
	if(key >= 0) {
		if(!mac_keys_down[key]) {
			ADBKeyDown((int)key);
			mac_keys_down[key] = 1;
		}
	}
}

static void do_mac_key_up( int key )
{
	if(key >= 0) {
		if(mac_keys_down[key]) {
			ADBKeyUp(key);
			mac_keys_down[key] = 0;
		}
	}
}

static int do_key_down(HWND hWnd, UINT uMessage, WPARAM wparam, LPARAM lparam, int alt )
{
	if((lparam & 0x40000000) == 0) {
		int key = scancode_2_mac(lparam);
		if(key >= 0) {
			do_mac_key_down(key);
			return(1);
		}
	}
	return(1);
}

static int do_key_up( HWND hWnd, UINT uMessage, WPARAM wparam, LPARAM lparam, int alt )
{
	int key = scancode_2_mac(lparam);
	if(key >= 0) {
		do_mac_key_up(key);
		return(1);
	}
	return(0);
}


void show_cursor( int onoff )
{
	if(onoff) {
		if(!m_show_cursor) {
			ShowCursor(TRUE);
			m_show_cursor = 1;
		}
	} else {
		if(m_show_cursor) {
			ShowCursor(FALSE);
			m_show_cursor = 0;
		}
	}
}

// GUI thread is not affected.

void suspend_emulation(void)
{
	audio_switch_inout( false );
	if(threads[THREAD_CPU].h && hMainWnd) {
		for( int i=0; i<THREAD_COUNT; i++ ) {
			if(threads[i].h) {
				SetThreadPriority( threads[i].h, threads[i].priority_suspended );
			}
		}
		if(ether_th1) SetThreadPriority( ether_th1, threads[THREAD_ETHER].priority_suspended );
		if(ether_th2) SetThreadPriority( ether_th2, threads[THREAD_ETHER].priority_suspended );
	}
}

void resume_emulation(void)
{
	if(threads[THREAD_CPU].h && hMainWnd) {
		for( int i=0; i<THREAD_COUNT; i++ ) {
			if(threads[i].h) {
				SetThreadPriority( threads[i].h, threads[i].priority_running );
			}
		}
		if(ether_th1) SetThreadPriority( ether_th1, threads[THREAD_ETHER].priority_running );
		if(ether_th2) SetThreadPriority( ether_th2, threads[THREAD_ETHER].priority_running );
	}
	audio_switch_inout( true );
}

static void media_check( media_t media )
{
	mount_removable_media( media );
}

static __inline__ UINT get_registered_media_hotkey( void )
{
	return( registered_media_hotkey );
}

static __inline__ UINT get_registered_floppy_hotkey( void )
{
	return( registered_floppy_hotkey );
}

static __inline__ UINT get_registered_cd_hotkey( void )
{
	return( registered_cd_hotkey );
}

static __inline__ UINT get_registered_hd_hotkey( void )
{
	return( registered_hd_hotkey );
}

static __inline__ bool is_shift_down(void)
{
	return( (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 );
}

static __inline__ bool is_control_down(void)
{
	return( (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 );
}

static void check_save_window_pos( HWND hWnd )
{
	if(is_windowed && !IsIconic(hWnd) && !IsZoomed(hWnd)) {
		RECT cr, wr;

		GetWindowRect( hWnd, &wr );
		GetClientRect( hWnd, &cr );
		save_window_position( cr.right - cr.left, cr.bottom - cr.top, wr.left, wr.top );
	}
}


//////////////// window dragging ////////////////
static POINT drag_cursor;

static void drag_window_start( HWND hWnd )
{
	GetCursorPos( &drag_cursor );
}

static void drag_window_stop( HWND hWnd )
{
}

static void drag_window_move( HWND hWnd )
{
	int dx, dy;
	POINT p;
	RECT r;

	GetWindowRect( hWnd, &r );
	GetCursorPos( &p );
	dx = p.x - drag_cursor.x;
	dy = p.y - drag_cursor.y;
	drag_cursor = p;
	SetWindowPos( hWnd, 0, r.left+dx, r.top+dy, 0, 0, SWP_NOSIZE|SWP_NOZORDER );
	UpdateWindow( hWnd );
}
//////////////// window dragging ////////////////


//////////////// wheelmouse ////////////////
static void do_mouse_wheel( HWND hWnd, short zDelta )
{
	int mac;
	short steps;

	if(m_mousewheelmode == 0) {
		if(m_mouse_wheel_reverse_y) zDelta = -zDelta;
		mac = (zDelta < 0) ? 0x79 : 0x74;
		steps = 1;
	} else {
		if(m_mousewheeldirection == MOUSEWHEEL_VERTICAL) {
			if(m_mouse_wheel_reverse_y) zDelta = -zDelta;
			mac = (zDelta < 0) ? 0x3D : 0x3E;
			steps = abs(zDelta) / WHEEL_DELTA * m_mousewheellines;
		} else {
			if(m_mouse_wheel_reverse_x) zDelta = -zDelta;
			mac = (zDelta < 0) ? 0x3C : 0x3B;
			steps = abs(zDelta) / WHEEL_DELTA * m_mousewheellines;
		}
	}

	for( short multiplier=0; multiplier<steps; multiplier++ ) {
		do_mac_key_down( mac );
		do_mac_key_up( mac );
	}
}

BYTE dehex2( char *s )
{
	char tmp[3];

	tmp[0] = *s++;
	tmp[1] = *s;
	tmp[2] = 0;

	return( (BYTE)strtoul(tmp,0,16) );
}

static void do_mac_keys( char *s )
{
	BYTE code;

	while( *s ) {
		if( *s == '+' ) {
			// Key down.
			s++;
			if(isxdigit(*s) && isxdigit(s[1])) {
				code = dehex2(s);
				do_mac_key_down(code);
				s += 2;
				// Avoid overflowing the queue
				// New: this is no good when browsing in Netscape.
				// It thinks it has to update the screen -> slow browsing.
				// Rather let it overflow.
				Sleep(20);
			} else {
				// Syntax error.
				break;
			}
		} else if( *s == '-' ) {
			// Key up.
			s++;
			if(isxdigit(*s) && isxdigit(s[1])) {
				code = dehex2(s);
				do_mac_key_up(code);
				s += 2;
				Sleep(20);
			} else {
				// Syntax error.
				break;
			}
		} else {
			// Syntax error.
			break;
		}
	}
}
//////////////// wheelmouse ////////////////

static int CALLBACK about_proc( HWND hDlg, unsigned message, WPARAM wParam, LPARAM lParam )
{
	int result = 0;
	static HFONT hfont = 0;

	switch (message) {
		case WM_INITDIALOG:
			hfont = CreateFont( -18, 0, 0, 0, FW_BOLD/*FW_NORMAL*/, (BYTE)FALSE, 0, 0, 0, 0, 0, 0, 0, "Comic Sans MS" );
		  SendDlgItemMessage( hDlg, IDC_ABOUT_CAPTION, WM_SETFONT, (WPARAM)hfont, 0);
			center_window( hDlg );
			result = 1;
			break;
		case WM_DESTROY:
      if(hfont) DeleteObject(hfont);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog( hDlg, TRUE );
					result = 1;
					break;
				case IDCANCEL:
					EndDialog( hDlg, FALSE );
					result = 1;
					break;
			}
	  	break;
	}
  return(result);
}

void update_system_menu( HWND hWnd )
{
	if(hWnd && GetWindowLong(hWnd, GWL_STYLE) & WS_SYSMENU) {
		static BOOL sysmenu_added = FALSE;
		if(!sysmenu_added) {
			HMENU hmsys = GetSystemMenu(hWnd,FALSE);
			if(hmsys) {

				ChangeMenu( hmsys, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
				ChangeMenu( hmsys, 0, "Check for all removable media\tCtrl+Shift+F11", IDM_MAINWINDOW_MEDIA_ALL, MF_APPEND|MF_STRING );
				ChangeMenu( hmsys, 0, "Check for floppies\tCtrl+Shift+F8", IDM_MAINWINDOW_MEDIA_FLOPPY, MF_APPEND|MF_STRING );
				ChangeMenu( hmsys, 0, "Check for CD's\tCtrl+Shift+F9", IDM_MAINWINDOW_MEDIA_CD, MF_APPEND|MF_STRING );
				ChangeMenu( hmsys, 0, "Check for HD's\tCtrl+Shift+F10", IDM_MAINWINDOW_MEDIA_HD, MF_APPEND|MF_STRING );

				ChangeMenu( hmsys, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
				ChangeMenu( hmsys, 0, "More complete ADB mouse emulation", IDM_MAINWINDOW_ADB, 
					m_mousemovementmode ? MF_APPEND|MF_STRING|MF_CHECKED : MF_APPEND|MF_STRING );

				ChangeMenu( hmsys, 0, "Sticky menu bar (OS8 style mouse clicks)", IDM_MAINWINDOW_OS8_MOUSE, 
					m_os8_mouse ? MF_APPEND|MF_STRING|MF_CHECKED : MF_APPEND|MF_STRING );

#ifdef OPTIMIZED_8BIT_MEMORY_ACCESS
				ChangeMenu( hmsys, 0, "ROM is write-protected", IDM_MAINWINDOW_ROM_PROTECT,
					m_ROM_is_protected ? MF_APPEND|MF_STRING|MF_CHECKED : MF_APPEND|MF_STRING );
#else
				ChangeMenu( hmsys, 0, "ROM is write-protected", IDM_MAINWINDOW_ROM_PROTECT,
					MF_APPEND|MF_STRING|MF_CHECKED|MF_GRAYED|MF_DISABLED );
#endif

				ChangeMenu( hmsys, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
				ChangeMenu( hmsys, 0, "About Basilisk II...", IDM_MAINWINDOW_ABOUT, MF_APPEND|MF_STRING );

				sysmenu_added = TRUE;
			}
		}
	}
}

static void start_60HZ_thread(void)
{
	timer_running = TRUE;
	if(m_debug_disable_accurate_timer) {
		if(m_mousemovementmode) {
			threads[THREAD_60_HZ].h = (HANDLE)_beginthreadex( 0, 0, tick_func_inaccurate_adb, 0, 0, &threads[THREAD_60_HZ].tid );
		} else {
			threads[THREAD_60_HZ].h = (HANDLE)_beginthreadex( 0, 0, tick_func_inaccurate, 0, 0, &threads[THREAD_60_HZ].tid );
		}
	} else {
		if(m_mousemovementmode) {
			threads[THREAD_60_HZ].h = (HANDLE)_beginthreadex( 0, 0, tick_func_accurate_adb, 0, 0, &threads[THREAD_60_HZ].tid );
		} else {
			threads[THREAD_60_HZ].h = (HANDLE)_beginthreadex( 0, 0, tick_func_accurate, 0, 0, &threads[THREAD_60_HZ].tid );
		}
	}
	SetThreadPriority( threads[THREAD_60_HZ].h, threads[THREAD_60_HZ].priority_running );
	SetThreadAffinityMask( threads[THREAD_60_HZ].h, threads[THREAD_60_HZ].affinity_mask );
}

static void stop_60HZ_thread(void)
{
	timer_running = FALSE;
	while( threads[THREAD_60_HZ].h ) {
		Sleep(100);
	}
	Sleep(20);
}

static void start_1HZ_thread(void)
{
	one_sec_running = TRUE;
	threads[THREAD_1_HZ].h = (HANDLE)_beginthreadex( 0, 0, one_sec_func, 0, 0, &threads[THREAD_1_HZ].tid );
	SetThreadPriority( threads[THREAD_1_HZ].h, threads[THREAD_1_HZ].priority_running );
	SetThreadAffinityMask( threads[THREAD_1_HZ].h, threads[THREAD_1_HZ].affinity_mask );
}

static void stop_1HZ_thread(void)
{
	one_sec_running = FALSE;
	while( threads[THREAD_1_HZ].h ) {
		Sleep(100);
	}
	Sleep(20);
}

#define GIGABYTES(n) ( (DWORD)0x40000000 * (n) )

// Cannot use a single mapping.
static HANDLE m_map_ram = 0;
static HANDLE m_map_rom = 0;
static HANDLE m_map_vid = 0;
static LPVOID m_view_ram = 0;
static LPVOID m_view_rom = 0;
static LPVOID m_view_vid = 0;

static void free_triple(void)
{
	if(m_view_ram) {
		UnmapViewOfFile( m_view_ram );
		m_view_ram = 0;
	}
	if(m_view_rom) {
		UnmapViewOfFile( m_view_rom );
		m_view_rom = 0;
	}
	if(m_view_vid) {
		UnmapViewOfFile( m_view_vid );
		m_view_vid = 0;
	}
	if(m_map_ram) {
		CloseHandle(m_map_ram);
		m_map_ram = 0;
	}
	if(m_map_rom) {
		CloseHandle(m_map_rom);
		m_map_rom = 0;
	}
	if(m_map_vid) {
		CloseHandle(m_map_vid);
		m_map_vid = 0;
	}
}

static BOOL allocate_triple(
	DWORD a1, DWORD sz1,
	DWORD a2, DWORD sz2,
	DWORD a3, DWORD sz3
)
{
	BOOL result = FALSE;
	MEMORY_BASIC_INFORMATION inf1, inf2, inf3;

	if( VirtualQuery( (LPCVOID)a1, &inf1, sizeof(inf1) ) == sizeof(inf1) &&
			VirtualQuery( (LPCVOID)a2, &inf2, sizeof(inf2) ) == sizeof(inf2) &&
			VirtualQuery( (LPCVOID)a3, &inf3, sizeof(inf3) ) == sizeof(inf3) )
	{
		if( inf1.State == MEM_FREE &&
		    inf2.State == MEM_FREE &&
		    inf3.State == MEM_FREE &&
				inf1.RegionSize >= sz1 + (a1 - (DWORD)inf1.BaseAddress) &&
				inf2.RegionSize >= sz2 + (a2 - (DWORD)inf2.BaseAddress) &&
				inf3.RegionSize >= sz3 + (a3 - (DWORD)inf3.BaseAddress) )
		{
			m_map_ram = CreateFileMapping(
				(HANDLE)0xFFFFFFFF, NULL,
				PAGE_READWRITE,
				0, sz1, NULL
			);
			m_map_rom = CreateFileMapping(
				(HANDLE)0xFFFFFFFF, NULL,
				PAGE_READWRITE,
				0, sz2, NULL
			);
			m_map_vid = CreateFileMapping(
				(HANDLE)0xFFFFFFFF, NULL,
				PAGE_READWRITE,
				0, sz3, NULL
			);
			if(m_map_ram && m_map_rom && m_map_vid) {
				m_view_ram = MapViewOfFileEx(
					m_map_ram,
					FILE_MAP_WRITE|FILE_MAP_READ,
					0, 0, sz1, (LPVOID)a1
				);
				m_view_rom = MapViewOfFileEx(
					m_map_rom,
					FILE_MAP_WRITE|FILE_MAP_READ,
					0, 0, sz2, (LPVOID)a2
				);
				m_view_vid = MapViewOfFileEx(
					m_map_vid,
					FILE_MAP_WRITE|FILE_MAP_READ,
					0, 0, sz3, (LPVOID)a3
				);
				if(m_view_ram && m_view_rom && m_view_vid) {
					result = TRUE;
				}
			}
		}
	}

	if(!result) free_triple();

	return(result);
}

static DWORD map_address_space(
	DWORD diff_rom_ram,
	DWORD diff_vid_ram,
	DWORD sz_ram,
	DWORD sz_rom,
	DWORD sz_vid
)
{
	DWORD base = 0;
	DWORD result = 0;
	DWORD ram, rom, vid;
	DWORD start, end, step;
	SYSTEM_INFO sysinfo;

	GetSystemInfo( &sysinfo );

	step = sysinfo.dwAllocationGranularity * 64;
	if(win_os == VER_PLATFORM_WIN32_WINDOWS) {
		start = GIGABYTES(2);
		end   = GIGABYTES(3);
	} else {
		start = GIGABYTES(0);
		end   = GIGABYTES(2);
	}

	// a speedup
	if(MacFrameBaseMac == 0xf0000000) {
		start += 0x10000000;
	}

	base = start;
	BOOL still_trying = TRUE;

	while( still_trying ) {
#ifdef SWAPPED_ADDRESS_SPACE
		vid = base;
		rom = base + sz_vid + diff_vid_ram - diff_rom_ram - sz_rom;
		ram = base + sz_vid + diff_vid_ram - sz_ram;
#else
		ram = base;
		rom = base + diff_rom_ram;
		vid = base + diff_vid_ram;
#endif
		if(allocate_triple( ram, sz_ram, rom, sz_rom, vid, sz_vid )) {
			result = base;
			still_trying = FALSE;
		} else {
			base += step;
			if(base >= end) still_trying = FALSE;
		}
	}

	return(result);
}

/*
static LPVOID all_mem = 0;

static void free_triple(void)
{
	if(the_buffer) {
		VirtualFree( the_buffer, 0, MEM_DECOMMIT );
		the_buffer = 0;
	}
	if(RAMBaseHost) {
		VirtualFree( RAMBaseHost, 0, MEM_DECOMMIT );
		RAMBaseHost = 0;
	}
	if(ROMBaseHost) {
		VirtualFree( ROMBaseHost, 0, MEM_DECOMMIT );
		ROMBaseHost = 0;
	}
	if(all_mem) {
		VirtualFree( all_mem, 0, MEM_RELEASE );
		all_mem = 0;
	}
}

static BOOL map_address_space(
	DWORD diff_rom_ram,
	DWORD diff_vid_ram,
	DWORD sz_ram,
	DWORD sz_rom,
	DWORD sz_vid
)
{
	SYSTEM_INFO sysinfo;
	DWORD start, end;

	GetSystemInfo( &sysinfo );

	if(win_os == VER_PLATFORM_WIN32_WINDOWS) {
		start = GIGABYTES(2);
		end   = GIGABYTES(3);
	} else {
		start = GIGABYTES(0);
		end   = GIGABYTES(2);
	}

	DWORD best_base = 0;
	DWORD best_size = 0;

	BOOL prev_was_ok = FALSE;
	DWORD prev_base = 0;
	DWORD prev_size = 0;

	MEMORY_BASIC_INFORMATION inf;
	for( DWORD base=start; base<end; ) {
		if( VirtualQuery( (LPCVOID)base, &inf, sizeof(inf) ) == sizeof(inf) &&
		    inf.RegionSize > 0 )
		{
			if( inf.State == MEM_FREE ) {
				if(prev_was_ok) {
					prev_size += inf.RegionSize;
				} else {
					prev_base = (DWORD)inf.BaseAddress;
					prev_size = inf.RegionSize;
					prev_was_ok = TRUE;
				}
				if( prev_size > best_size ) {
					best_size = prev_size;
					best_base = prev_base;
				}
			} else {
				prev_was_ok = FALSE;
			}
			base += inf.RegionSize;
		} else {
			prev_was_ok = FALSE;
			base += sysinfo.dwAllocationGranularity;
		}
	}

	BOOL ok = FALSE;

	// Now [best_base,best_size] is the largest block we can reserve.

	// Align to 64kB boundary -- upwards.
	DWORD diff = best_base & 0xFFFF;
	if(diff) {
		diff = 0x10000 - diff;
		best_base += diff;
		best_size -= diff;
	}

	if(best_base && best_size) {
		all_mem = VirtualAlloc( 
			(LPVOID *)best_base,
			best_size,
			MEM_RESERVE,
			PAGE_NOACCESS
		);
		if(all_mem) {
			DWORD vid = best_base;
			DWORD ram = best_base - diff_vid_ram;
			DWORD rom = best_base + diff_rom_ram - diff_vid_ram;
			the_buffer = (uint8 *)VirtualAlloc( (LPVOID*)vid, sz_vid, MEM_COMMIT, PAGE_READWRITE );
			RAMBaseHost = (uint8 *)VirtualAlloc( (LPVOID*)ram, sz_ram, MEM_COMMIT, PAGE_READWRITE );
			ROMBaseHost = (uint8 *)VirtualAlloc( (LPVOID*)rom, sz_rom, MEM_COMMIT, PAGE_READWRITE );
			ok = (the_buffer == (uint8 *)vid) &&
					 (RAMBaseHost == (uint8 *)ram) &&
					 (ROMBaseHost == (uint8 *)rom);
			MEMBaseDiff = ram;
		} else {
			DWORD err = GetLastError();
			_asm int 3
		}
	}
	if(!ok) free_triple();
	return ok;
}
*/

static DWORD word_align( DWORD x )
{
	return( ((x + 15) / 16) * 16 );
}

static DWORD page_align( DWORD x )
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	DWORD a = sysinfo.dwAllocationGranularity;
	return( ((x+a-1) / a) * a );
}

static void alloc_memory_8(void)
{
	// Now create mappings and views so that:
	//		(MacFrameBaseMac - ROMBaseMac) == (the_buffer - ROMBaseHost)
	// and
	//		(ROMBaseMac - RAMBaseMac) == (ROMBaseHost - RAMBaseHost)

// _asm int 3

	DWORD ram_mac = (DWORD)RAMBaseMac;
	DWORD rom_mac = (DWORD)ROMBaseMac;
	DWORD vid_mac = (DWORD)MacFrameBaseMac;

	DWORD diff_rom_ram = rom_mac - ram_mac;
	DWORD diff_vid_ram = vid_mac - ram_mac;

	DWORD ram_mem_sz = page_align( RAMSize );
	DWORD rom_mem_sz = page_align( 0x100000 );
	DWORD vid_mem_sz = page_align(
		word_align(GetSystemMetrics(SM_CXSCREEN)) *
		GetSystemMetrics(SM_CYSCREEN) *
		4 +
		65536
	);

	DWORD base = map_address_space(
		diff_rom_ram,
		diff_vid_ram,
		RAMSize,
		ROMSize, // 0x100000,
		vid_mem_sz
	);
	if(!base) {
		ErrorAlert(
			"Could not map the memory to the process address space. "
			"Try to define a smaller amount of RAM, or use the generic version of Basilisk II."
		);
		QuitEmulator();
	}
#ifdef SWAPPED_ADDRESS_SPACE
	the_buffer  = (uint8 *)( base + 0 );
	ROMBaseHost = (uint8 *)( base + vid_mem_sz + diff_vid_ram - diff_rom_ram - rom_mem_sz );
	RAMBaseHost = (uint8 *)( base + vid_mem_sz + diff_vid_ram - ram_mem_sz );
#else
	RAMBaseHost = (uint8 *)( base + 0 );
	ROMBaseHost = (uint8 *)( base	+ diff_rom_ram );
	the_buffer  = (uint8 *)( base	+ diff_vid_ram );
#endif

	/*
	BOOL ok = map_address_space(
		diff_rom_ram,
		diff_vid_ram,
		RAMSize,
		ROMSize, // 0x100000,
		vid_mem_sz
	);
	if(!ok) {
		ErrorAlert(
			"Could not allocate memory for RAM, ROM and Video buffer. "
			"Try to define a smaller amount of RAM, or use the generic version of Basilisk II."
		);
		QuitEmulator();
	}
	*/

	memset( the_buffer, 0, vid_mem_sz );

#ifdef OPTIMIZED_8BIT_MEMORY_ACCESS
	total_mem_limits.mem_start	= (DWORD)the_buffer;
	total_mem_limits.mem_end		= (DWORD)ROMBaseHost + ROMSize;

	RAM_mem_limits.mem_start		= (DWORD)RAMBaseHost;
	RAM_mem_limits.mem_end			= (DWORD)RAMBaseHost + RAMSize;

	ROM_mem_limits.mem_start		= (DWORD)ROMBaseHost;
	ROM_mem_limits.mem_end			= (DWORD)ROMBaseHost + ROMSize;

	Video_mem_limits.mem_start	= (DWORD)the_buffer;
	Video_mem_limits.mem_end		= (DWORD)the_buffer + vid_mem_sz;
#endif

#ifdef OPTIMIZED_8BIT_MEMORY_ACCESS
	MEMBaseDiff = base;
#ifdef SWAPPED_ADDRESS_SPACE
	MEMBaseLongTop = vid_mac + vid_mem_sz - 4;
	MEMBaseWordTop = vid_mac + vid_mem_sz - 2;
	MEMBaseByteTop = vid_mac + vid_mem_sz - 1;
#endif
#endif
}

static void alloc_memory(void)
{
	RAMBaseHost = (uint8 *)VirtualAlloc( 0, RAMSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
	if(!RAMBaseHost) {
		ErrorAlert(GetString(STR_NO_RAM_AREA_ERR));
		QuitEmulator();
	}

	if(experiment_get_bool("DisableLowMemCache")) {
		DWORD OldProtect;
		VirtualProtect( (LPVOID)RAMBaseHost, 64*1024, PAGE_READWRITE|PAGE_NOCACHE, &OldProtect );
	}

	// Create area for Mac ROM
	// Do not use ROMBaseHost here, we need at least 1MB
	ROMBaseHost = (uint8 *)VirtualAlloc( 0, 0x100000, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
	if(!ROMBaseHost) {
		ErrorAlert(GetString(STR_NO_ROM_AREA_ERR));
		QuitEmulator();
	}
}

#define WM_CPU_QUIT_REQUEST (WM_USER+24000)

void QuitEmulator(void)
{
	D(bug("quitting...\r\n"));

	if(threads[THREAD_CPU].h) {
		if(GetCurrentThreadId() == threads[THREAD_CPU].tid) {
			PostMessage( hMainWnd, WM_CPU_QUIT_REQUEST, 0, 0 );
			threads[THREAD_CPU].h = NULL;
			threads[THREAD_CPU].tid = 0;
			_endthreadex( 0 );
		}
	}

	dump_counts();

	// may take up to one second to close down.
	one_sec_running = FALSE;

	// Exit 680x0 emulation
	Exit680x0();

	D(bug("Calling EtherExit\r\n"));
	EtherExit();

	stop_60HZ_thread();

	quit_program = 1;

	Sleep(100);

	stop_1HZ_thread();

	// Normally they die gracefully.
	if(threads[THREAD_60_HZ].h) {
		D(bug("Killing 60Hz\r\n"));
		TerminateThread(threads[THREAD_60_HZ].h,0);
		threads[THREAD_60_HZ].h = 0;
	}
	if(threads[THREAD_1_HZ].h) {
		// This may be killed, but it does not use any resources when asleep.
		D(bug("Killing 1Hz\r\n"));
		TerminateThread(threads[THREAD_1_HZ].h,0);
		threads[THREAD_1_HZ].h = 0;
	}
	if(threads[THREAD_CPU].h) {
		if(GetCurrentThreadId() != threads[THREAD_CPU].tid) {
			D(bug("Killing CPU\r\n"));
			TerminateThread(threads[THREAD_CPU].h,0);
			threads[THREAD_CPU].h = 0;
		}
	}

	D(bug("Terminating hooks\r\n"));
	final_keyboard_hook();
	unregister_desktop_hotkey();

	// Save XPRAM
	D(bug("Calling XPRAMExit\r\n"));
	XPRAMExit();

	// Exit audio
	D(bug("Calling AudioExit\r\n"));
	AudioExit();

	// Exit clipboard
	D(bug("Calling ClipExit\r\n"));
	ClipExit();

	// Exit Time Manager
	D(bug("Calling TimerExit\r\n"));
	TimerExit();

	// Exit serial ports
	D(bug("Calling SerialExit\r\n"));
	SerialExit();

	if(display_type != DISPLAY_FB) {
		// Exit video
		D(bug("Calling VideoExit\r\n"));
		VideoExit();
	}

	// Exit drivers
	D(bug("Calling SCSIExit\r\n"));
	SCSIExit();

	D(bug("Calling CDROMExit\r\n"));
	CDROMExit();

	D(bug("Calling DiskExit\r\n"));
	DiskExit();

	D(bug("Calling SonyExit\r\n"));
	SonyExit();

	if(display_type == DISPLAY_FB) {
		// Exit video
		D(bug("Calling VideoExit\r\n"));
		VideoExit();
	}

	D(bug("Freeing ROM and RAM\r\n"));
	if(mem_8_only) {
		// free_triple();
	} else {
    if(ROMBaseHost) VirtualFree( ROMBaseHost, 0, MEM_RELEASE  );
    if(RAMBaseHost) VirtualFree( RAMBaseHost, 0, MEM_RELEASE  );
	}

	// Exit system routines
	D(bug("Calling SysExit\r\n"));
	SysExit();

	// Exit preferences
	D(bug("Calling PrefsExit\r\n"));
	PrefsExit();

	D(bug("Calling xpar_save_if_needed\r\n"));
	xpar_save_if_needed();

	D(bug("Calling xpar_final\r\n"));
	xpar_final();

	D(bug("Calling KernelExit\r\n"));
	KernelExit();

	D(bug("Calling ExtFSExit\r\n"));
	ExtFSExit();

	launch_gui();

	D(bug("exit(0)\r\n"));
	exit(0);
}

static int check_os(void)
{
	OSVERSIONINFO osv;

	win_os = VER_PLATFORM_WIN32_WINDOWS;
	win_os_major = 0;

	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if(GetVersionEx( &osv )) {
		if(osv.dwPlatformId == VER_PLATFORM_WIN32s) {
			ErrorAlert("This program does not run on Win32s.");
			QuitEmulator();
		} else {
			win_os = osv.dwPlatformId;
		}
		win_os_major = osv.dwMajorVersion;

		if(osv.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
			if(osv.dwBuildNumber <= 1000) {
				win_os_old = TRUE;
			}
		}
	}

	if(win_os == VER_PLATFORM_WIN32_NT && win_os_major < 4) {
		ErrorAlert( "Basilisk II does not run on Windows NT versions less than 4.0." );
		return 0;
	}

#ifdef OPTIMIZED_8BIT_MEMORY_ACCESS
	if(win_os != VER_PLATFORM_WIN32_NT) {
		ErrorAlert( "This version of Basilisk II does not run under Windows 9x." );
		// ... but it actually should run already.
		return 0;
	}
#endif

	return 1;
}

bool read_rom( const char *path, uint8 *buffer, uint32 sz )
{
	HFILE rom_fd = _lopen( path ? path : ROM_FILE_NAME, OF_READ);
	if (rom_fd < 0) {
		ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
		QuitEmulator();
	}
	ROMSize = _llseek(rom_fd, 0, SEEK_END);
	if(!GetPrivateProfileInt( "Debug", "allow_all_rom_sizes", 0, ini_file_name )) {
		if (ROMSize != 64*1024 && ROMSize != 128*1024 && ROMSize != 256*1024 && ROMSize != 512*1024 && ROMSize != 1024*1024) {
			ErrorAlert(GetString(STR_ROM_SIZE_ERR));
			_lclose(rom_fd);
			QuitEmulator();
		}
	}
	_llseek(rom_fd, 0, SEEK_SET);
	if (_lread(rom_fd, buffer, sz) != sz) {
		ErrorAlert(GetString(STR_ROM_FILE_READ_ERR));
		_lclose(rom_fd);
		QuitEmulator();
	}
	_lclose(rom_fd);
	return true;
}

#ifdef SWAPPED_ADDRESS_SPACE
static void reverse_rom( uint8 *p, uint32 sz )
{
	uint8 tmp;
	uint32 i, half = sz/2;
	for(i=0; i<half; i++) {
		tmp = p[i];
		p[i] = p[sz-i-1];
		p[sz-i-1] = tmp;
	}
}
#endif

static void protect_ROM( bool protect )
{
	if(mem_8_only) {
		DWORD OldProtect;
    VirtualProtect( 
			(LPVOID)ROMBaseHost,
			ROMSize, 
			protect ? PAGE_READONLY : PAGE_READWRITE, 
			&OldProtect
		);
	}
}

static void initialize(void)
{
	if(win_os == VER_PLATFORM_WIN32_WINDOWS && win_os_major < 5) {
		m_mousewheel_old_msg = RegisterWindowMessage(MSH_MOUSEWHEEL);
	}

	KernelInit();

	timer_init();

	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;

	srand(time(NULL));
	tzset();

	// Read preferences
	PrefsInit();

	m_disable_internal_wait = GetPrivateProfileInt( "Debug", "disable_internal_wait", 0, ini_file_name );

	m_os8_mouse = PrefsFindBool("stickymenu");
	m_mousewheelmode = PrefsFindInt16("mousewheelmode");
	m_mousewheellines = PrefsFindInt16("mousewheellines");
	m_mouse_wheel_click_mode = PrefsFindInt16("mousewheelclickmode");

	const char *tmp;

	tmp = PrefsFindString("mousewheelcust00");
	if(tmp) strncpy( m_mouse_wheel_cust_00, tmp, sizeof(m_mouse_wheel_cust_00)-1 );
	m_mouse_wheel_cust_00[sizeof(m_mouse_wheel_cust_00)] = 0;

	tmp = PrefsFindString("mousewheelcust01");
	if(tmp) strncpy( m_mouse_wheel_cust_01, tmp, sizeof(m_mouse_wheel_cust_01)-1 );
	m_mouse_wheel_cust_01[sizeof(m_mouse_wheel_cust_01)] = 0;

	tmp = PrefsFindString("mousewheelcust10");
	if(tmp) strncpy( m_mouse_wheel_cust_10, tmp, sizeof(m_mouse_wheel_cust_10)-1 );
	m_mouse_wheel_cust_10[sizeof(m_mouse_wheel_cust_10)] = 0;

	tmp = PrefsFindString("mousewheelcust11");
	if(tmp) strncpy( m_mouse_wheel_cust_11, tmp, sizeof(m_mouse_wheel_cust_11)-1 );
	m_mouse_wheel_cust_11[sizeof(m_mouse_wheel_cust_11)] = 0;

	m_mousemovementmode = PrefsFindInt16("mousemovementmode");
	ADBSetRelMouseMode(m_mousemovementmode != 0);

	threads_init();

	// Init system routines
	SysInit();

#define ONEMB (1024*1024)
#define FOURMB (4*1024*1024)

	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize");
	if (RAMSize < ONEMB) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = ONEMB;
	}

	// Round up. Must be aligned.
	if(RAMSize % FOURMB) {
		RAMSize = (RAMSize/FOURMB + 1) * FOURMB;
	}

	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	uint8 peek_rom[256];
	ROMBaseHost = peek_rom;
	read_rom( rom_path, peek_rom, sizeof(peek_rom) );
	ROM_checksum = ntohl(*(uint32 *)ROMBaseHost);

	xpar_init();

	// Check ROM version
	if (!CheckROM()) {
		ErrorAlert(GetString(STR_UNSUPPORTED_ROM_TYPE_ERR));
		QuitEmulator();
	}

	// Set CPU and FPU type (UAE emulation)
	switch (ROMVersion) {
		case ROM_VERSION_64K:
		case ROM_VERSION_PLUS:
		case ROM_VERSION_CLASSIC:
			CPUType = 0;
			FPUType = 0;
			TwentyFourBitAddressing = true;
			break;
		case ROM_VERSION_II:
			CPUType = 2;
			FPUType = PrefsFindBool("fpu") ? 1 : 0;
			TwentyFourBitAddressing = true;
			break;
		case ROM_VERSION_32:
			if(PrefsFindInt32("cpu") == 68030) CPUType = 3;
			else if(PrefsFindInt32("cpu") == 68040) CPUType = 4;
			else CPUType = 2;
			// Allow 68040 w/o fpu
			FPUType = PrefsFindBool("fpu") ? 1 : 0;
			TwentyFourBitAddressing = false;
			break;
	}
	CPUIs68060 = false;

	if(!Init680x0()) {
		QuitEmulator();
	}

	if(mem_8_only) {
		alloc_memory_8();
	} else {
		alloc_memory();
	}

	// call it again ...now we have valid MemBaseDiff
	if(mem_8_only) {
		if(!Init680x0()) {
			QuitEmulator();
		}
	}

#if !REAL_ADDRESSING
	// Reinitialize UAE memory banks (alloc_memory*() may relocate buffers)
	memory_init();
#endif

	read_rom( rom_path, ROMBaseHost, ROMSize );

	// Load XPRAM
	XPRAMInit();

	// Set boot volume
	int16 i16 = PrefsFindInt16("bootdrive");
	XPRAM[0x78] = i16 >> 8;
	XPRAM[0x79] = i16 & 0xff;

	i16 = PrefsFindInt16("bootdriver");
	XPRAM[0x7a] = i16 >> 8;
	XPRAM[0x7b] = i16 & 0xff;

	bool is_lfb_mode = false;
	if(!mem_8_only) {
		// Do not create any threads before this code
		const char *mode_str = PrefsFindString("screen");
		if(mode_str && (strncmp(mode_str, "fb",2) == 0) && (win_os == VER_PLATFORM_WIN32_NT)) {
			is_lfb_mode = true;
			if(!init_desktop()) {
				ErrorAlert("Could not create new desktop for frame buffer access. Maybe not administrator rights?");
				QuitEmulator();
			}
		}
	}

	if(is_lfb_mode) {
		m_use_alt_escape = m_use_alt_tab = m_use_control_escape = m_use_alt_space = true;
	} else {
		m_use_alt_escape = PrefsFindBool("usealtescape");
		m_use_alt_tab = PrefsFindBool("usealttab");
		m_use_control_escape = PrefsFindBool("usecontrolescape");
		m_use_alt_space = PrefsFindBool("usealtspace");
	}

	m_debug_disable_accurate_timer = PrefsFindBool("disableaccuratetimer");
	m_gui_autorestart = PrefsFindInt16("guiautorestart");

	// Init drivers

	SonyInit();
	DiskInit();
	CDROMInit();
	SCSIInit();

#if SUPPORTS_EXTFS
	// Init external file system
	ExtFSInit();
#endif

	// Init serial ports
	SerialInit();

	// Init network
	EtherInit();

	// Init Time Manager
	TimerInit();

	// Init clipboard
	ClipInit();

	m_right_mouse = PrefsFindInt16("rightmouse");

	char key_file[_MAX_PATH];
	const char *keyboardfile = PrefsFindString("keyboardfile");

	if(keyboardfile && *keyboardfile) {
		if(strchr(keyboardfile,'\\')) {
			// full path (should be!)
			strcpy( key_file, keyboardfile );
		} else {
			GetCurrentDirectory(sizeof(key_file),key_file);
			if(strlen(key_file) > 0 && key_file[strlen(key_file)-1] != '\\') strcat(key_file,"\\");
			strcat( key_file, keyboardfile );
		}
	} else {
		strcpy( key_file, "BasiliskII_keyboard" );
	}

	load_key_codes( key_file, keymap );

	// Init video
	if (!VideoInit(ROMVersion == ROM_VERSION_64K || ROMVersion == ROM_VERSION_PLUS || ROMVersion == ROM_VERSION_CLASSIC)) {
		ErrorAlert("Failed to initialize video.");
		QuitEmulator();
	}

	// Init audio
	AudioInit();

	if(!GetPrivateProfileInt( "Debug", "disable_keyboard_hook", 0, ini_file_name )) {
		init_keyboard_hook();
		register_desktop_hotkey();
	}

#if !REAL_ADDRESSING
	// Reinitialize UAE memory banks (VideoInit() set the frame buffer)
	memory_init();
#endif

	// Install ROM patches
	if (!PatchROM()) {
		ErrorAlert(GetString(STR_UNSUPPORTED_ROM_TYPE_ERR));
		QuitEmulator();
	}

#ifdef SWAPPED_ADDRESS_SPACE
	reverse_rom( ROMBaseHost, ROMSize );
#endif

	if(mem_8_only) {
		protect_ROM( m_ROM_is_protected );
	}

	// Start XPRAM watchdog thread
	threads[THREAD_PARAMETER_RAM].h = (HANDLE)_beginthreadex( 0, 0, xpram_func, 0, 0, &threads[THREAD_PARAMETER_RAM].tid );
	SetThreadPriority( threads[THREAD_PARAMETER_RAM].h, threads[THREAD_PARAMETER_RAM].priority_running );
	SetThreadAffinityMask( threads[THREAD_PARAMETER_RAM].h, threads[THREAD_PARAMETER_RAM].affinity_mask );

	start_1HZ_thread();

	// GUI
	threads[THREAD_GUI].h = 0;
	threads[THREAD_GUI].tid = GetCurrentThreadId();

	// Start 68k and jump to ROM boot routine
	threads[THREAD_CPU].h = (HANDLE)_beginthreadex( 0, 0, cpu_thread, 0, 0, &threads[THREAD_CPU].tid );
	SetThreadPriority( threads[THREAD_CPU].h, threads[THREAD_CPU].priority_running );
	SetThreadAffinityMask( threads[THREAD_CPU].h, threads[THREAD_CPU].affinity_mask );

	start_60HZ_thread();

	SetThreadPriority( GetCurrentThread(), threads[THREAD_GUI].priority_running );
	SetThreadAffinityMask( GetCurrentThread(), threads[THREAD_GUI].affinity_mask );

	if(PrefsFindBool("usestartupsound") && !PrefsFindBool("nosound"))
		play_startup_sound(ROM_checksum);
}

void WarningAlert(const char *text)
{
	show_cursor( 1 );
	video_activate( false );
	MessageBox(hMainWnd,text,GetString(STR_WINDOW_TITLE),MB_OK|MB_ICONINFORMATION);
	video_activate( true );
	show_cursor( 0 );
}

void ErrorAlert(const char *text)
{
	show_cursor( 1 );
	video_activate( false );
	MessageBox(hMainWnd,text,GetString(STR_WINDOW_TITLE),MB_OK|MB_ICONSTOP);
	video_activate( true );
	show_cursor( 0 );
}

/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

#if EMULATED_68K
void FlushCodeCache(void *start, uint32 size)
{
#if USE_COMPILER
	compiler_forget_range( start, size );
#endif
}
#endif

unsigned int WINAPI cpu_thread(LPVOID param)
{
	set_desktop();

	fpu_init();
	fpu_set_integral_fpu( CPUType == 4 );

	// init_m68k();
	m68k_reset();
	m68k_go(true);

  threads[THREAD_CPU].h = NULL;
  threads[THREAD_CPU].tid = 0;
	_endthreadex( 0 );

	return 0;
}

static void do_mbutton_down()
{
	switch(m_mouse_wheel_click_mode) {
		case 0:
			break;
		case 1:
			if(m_mousewheeldirection == MOUSEWHEEL_HORIZONTAL) {
				m_mousewheeldirection = MOUSEWHEEL_VERTICAL;
			} else {
				m_mousewheeldirection = MOUSEWHEEL_HORIZONTAL;
			}
			break;
		case 2:
			do_mac_keys( "+37+3B-3B-37" );
			break;
		case 3:
			if(is_shift_down()) {
				do_mac_keys( "-38" );
				if(is_control_down()) {
					do_mac_keys( "-36" );
					do_mac_keys( m_mouse_wheel_cust_11 );
				} else {
					do_mac_keys( m_mouse_wheel_cust_01 );
				}
			} else {
				if(is_control_down()) {
					do_mac_keys( "-36" );
					do_mac_keys( m_mouse_wheel_cust_10 );
				} else {
					do_mac_keys( m_mouse_wheel_cust_00 );
				}
			}
			break;
	}
}

static void do_windowpos_changing( HWND hWnd, LPWINDOWPOS lpwp )
{
	int szx = GetSystemMetrics(SM_CXSCREEN);
	int szy = GetSystemMetrics(SM_CYSCREEN);
	int x, y, cx, cy;
	RECT r;
	GetWindowRect( hWnd, &r );
	if( lpwp->flags & SWP_NOMOVE ) {
		x = r.left;
		y = r.top;
	} else {
		x = lpwp->x;
		y = lpwp->y;
	}
	if( lpwp->flags & SWP_NOSIZE ) {
		cx = r.right - r.left;
		cy = r.bottom - r.top;
	} else {
		cx = lpwp->cx;
		cy = lpwp->cy;
	}
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(cx > szx) cx = szx;
	if(cy > szy) cy = szy;
	if(x + cx >= szx) x = szx - cx;
	if(y + cy >= szy) y = szy - cy;

	lpwp->x = x;
	lpwp->y = y;
	lpwp->cx = cx;
	lpwp->cy = cy;
}

static void do_devicechange( WPARAM wParam, LPARAM lParam )
{
	if( wParam == DBT_DEVICEREMOVECOMPLETE ) {
		DEV_BROADCAST_HDR *p;
		p = (DEV_BROADCAST_HDR *)lParam;
		if(p->dbch_devicetype == DBT_DEVTYP_VOLUME) {
			media_removed();
		}
	} else if( wParam == DBT_DEVICEARRIVAL ) {
		DEV_BROADCAST_HDR *p;
		p = (DEV_BROADCAST_HDR *)lParam;
		if(p->dbch_devicetype == DBT_DEVTYP_VOLUME) {
			media_arrived();
		}
	}
}

static bool is_maximize_disabled()
{
	return GetPrivateProfileInt( "GUI", "Disable Maximize", 0, ini_file_name ) != 0;
}

static LRESULT do_syscommand( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_ROM_PROTECT) {
		m_ROM_is_protected = !m_ROM_is_protected;
		protect_ROM( m_ROM_is_protected );
		HMENU hmsys = GetSystemMenu(hWnd,FALSE);
		CheckMenuItem( hmsys, IDM_MAINWINDOW_ROM_PROTECT, m_ROM_is_protected ? MF_CHECKED : MF_UNCHECKED );
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_ABOUT) {
		(void)DialogBox( hInst, "DLG_ABOUT_B2", hWnd, (DLGPROC)about_proc );
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_ADB) {
		m_mousemovementmode = !m_mousemovementmode;
		ADBSetRelMouseMode(m_mousemovementmode != 0);
		HMENU hmsys = GetSystemMenu(hWnd,FALSE);
		CheckMenuItem( hmsys, IDM_MAINWINDOW_ADB, m_mousemovementmode ? MF_CHECKED : MF_UNCHECKED );
		stop_60HZ_thread();
		start_60HZ_thread();
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_OS8_MOUSE) {
		m_os8_mouse = !m_os8_mouse;
		HMENU hmsys = GetSystemMenu(hWnd,FALSE);
		CheckMenuItem( hmsys, IDM_MAINWINDOW_OS8_MOUSE, m_os8_mouse ? MF_CHECKED : MF_UNCHECKED );
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_MEDIA_ALL) {
		media_check(MEDIA_REMOVABLE);
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_MEDIA_FLOPPY) {
		media_check(MEDIA_FLOPPY);
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_MEDIA_CD) {
		media_check(MEDIA_CD);
		return 0;
	} else if((LOWORD(wParam) & 0xFFF0) == IDM_MAINWINDOW_MEDIA_HD) {
		media_check(MEDIA_HD);
		return 0;
	} else if(wParam == SC_MAXIMIZE) {
		if(!is_maximize_disabled()) {
			if(0 == GetPrivateProfileInt( "Tips", "Alt-Enter tip", 0, ini_file_name )) {
				WritePrivateProfileInt( "Tips", "Alt-Enter tip", 1, ini_file_name );
				MessageBox(
					hWnd,
					"Basilisk II will now switch to full screen mode.\n"
					"Windowed mode can be restored by pressing ALT-ENTER.\n"
					"\n"
					"You will not see this tip again.\n"
					,
					GetString(STR_WINDOW_TITLE),
					MB_OK|MB_ICONINFORMATION
				);
			}
			toggle_full_screen_mode();
		}
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void do_hotkey( WPARAM wParam )
{
	if(get_registered_desktop_hotkey() == wParam) {
		swap_desktop();
	} else if(get_registered_media_hotkey() == wParam) {
		media_check(MEDIA_REMOVABLE);
	} else if(get_registered_floppy_hotkey() == wParam) {
		media_check(MEDIA_FLOPPY);
	} else if(get_registered_cd_hotkey() == wParam) {
		media_check(MEDIA_CD);
	} else if(get_registered_hd_hotkey() == wParam) {
		media_check(MEDIA_HD);
	}
}

static void do_killfocus( void )
{
	int i;
	for( i=0; i<MAX_KEYS; i++ ) {
		if(mac_keys_down[i]) {
			mac_keys_down[i] = 0;
			ADBKeyUp(i);
		}
	}
	if(!has_own_desktop()) suspend_emulation();
}

static void do_setfocus( void )
{
	if(!has_own_desktop()) resume_emulation();
	NewTextScrap();
}

static void do_ncactivate( WPARAM wParam )
{
	if((BOOL) wParam) {
		video_activate( true );
		if(!has_own_desktop()) {
			resume_emulation();
			if(is_windowed) {
				ShowWindow(hMainWnd, SW_SHOWNORMAL);
			} else {
				ShowWindow(hMainWnd, SW_MAXIMIZE);
			}
			SetActiveWindow(hMainWnd);
			SetFocus(hMainWnd);
		}
		NewTextScrap();
	} else {
		video_activate( false );
		int i;
		for( i=0; i<MAX_KEYS; i++ ) {
			if(mac_keys_down[i]) {
				mac_keys_down[i] = 0;
				ADBKeyUp(i);
			}
		}
		if(!has_own_desktop()) suspend_emulation();
	}
}

static void do_close( HWND hWnd )
{
	static bool inhere = false;
	// Yes it's possible to have many of these in a row
	if(!inhere) {
		inhere = true;
		show_cursor( 1 );
		video_activate( false );
		int answer = MessageBox(
			hWnd,
			"You should normally close Basilisk II only from \"Special\"/\"Shut Down\".\r\n"
			"Really close now (some data may be lost)?",
			GetString(STR_WINDOW_TITLE),
			MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2
		);
		show_cursor( 0 );
		if(answer != IDNO) {
			QuitEmulator();
		} else {
			video_activate( true );
		}
		inhere = false;
	}
}

static void do_size( WPARAM wParam )
{
	if(wParam == SIZE_MINIMIZED) {
		if(!has_own_desktop()) suspend_emulation();
	} else if(wParam == SIZE_RESTORED) {
		if(!has_own_desktop()) resume_emulation();
	} else if(wParam == SIZE_MAXIMIZED) {
		if(!has_own_desktop()) resume_emulation();
	}
}

/*
 *  1Hz thread
 */

unsigned int WINAPI one_sec_func(LPVOID param)
{
	DWORD mac_boot_secs = 0;
	time_t t, bias_minutes;
  int32 should_have_ticks;
	TIME_ZONE_INFORMATION tz;

	set_desktop();

	time( &t );

	memset( &tz, 0, sizeof(tz) );
	if(GetTimeZoneInformation( &tz ) == TIME_ZONE_ID_DAYLIGHT) {
		bias_minutes = tz.Bias + tz.DaylightBias;
	} else {
		bias_minutes = tz.Bias;
	}
	t = t - 60*bias_minutes + TIME_OFFSET;

	mac_boot_secs = t - GetTickCount()/1000 ;
	WriteMacInt32(0x20c, t);

	should_have_ticks = GetTickCount() + 1000;

	// Pseudo Mac 1Hz interrupt, update local time
	while (one_sec_running) {
		Sleep( should_have_ticks - (int)GetTickCount() );
		WriteMacInt32(0x20c, mac_boot_secs + GetTickCount()/1000);
		// WriteMacInt32( 0x20c, ReadMacInt32(0x20c)+1 );
		SetInterruptFlag(INTFLAG_1HZ);
		TriggerInterrupt();
		should_have_ticks += 1000;
	}

  threads[THREAD_1_HZ].h = NULL;
  threads[THREAD_1_HZ].tid = 0;
	_endthreadex( 0 );

	return 0;
}

// extern "C" {extern int debugging_fpp;}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_CPU_QUIT_REQUEST:
			QuitEmulator();
			break;

		case WM_NCLBUTTONDBLCLK:
			if( (int)wParam == HTCAPTION ) {
				if(!is_maximize_disabled()) {
					toggle_full_screen_mode();
				}
				return 0;
			}
			break;

		case WM_MENUSELECT:
			m_menu_select = true;
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_EXITMENULOOP:
			m_menu_select = false;
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
			do_mbutton_down();
			return 0;

		case WM_MOUSEWHEEL:
			do_mouse_wheel(hWnd,(short)HIWORD(wParam));
			return 0;

		case WM_WINDOWPOSCHANGING:
			if(is_windowed && display_type == DISPLAY_DX) {
				do_windowpos_changing( hWnd, (LPWINDOWPOS)lParam );
				return(0);
			}
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_WINDOWPOSCHANGED:
			check_save_window_pos(hWnd);
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_SETCURSOR:
			if(LOWORD(lParam) == HTCLIENT) {
				show_cursor( 0 );
			} else {
				show_cursor( 1 );
			}
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_DEVICECHANGE:
			do_devicechange( wParam, lParam );
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_SYSCOMMAND:
			return do_syscommand( hWnd, msg, wParam, lParam );

		case WM_COMMAND:
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_DESTROY:
			PostQuitMessage(0);
      return(0);

#ifdef WANT_HAVE_FAST_TASK_SWITCH
		case WM_CANCELMODE:
			if(has_own_desktop() && is_screen_inited()) {
				D(bug("cancelmode: swapping desktop\r\n"));
				swap_desktop();
				return(0);
			}
			return(DefWindowProc(hWnd, msg, wParam, lParam));
			break;
#endif

	  case WM_HOTKEY:
			do_hotkey( wParam );
      return(0);

		case WM_KILLFOCUS:
			do_killfocus();
			return(0);

		case WM_SETFOCUS:
			do_setfocus();
			return(0);

		case WM_NCACTIVATE:
			if(display_type == DISPLAY_FB && (BOOL)wParam == TRUE) {
				do_ncactivate( wParam );
			}
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_ACTIVATEAPP:
			do_ncactivate( wParam );
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_CLOSE:
			do_close( hWnd );
			return(0);

		case WM_CHAR:
		case WM_DEADCHAR:
		case WM_SYSDEADCHAR:
			break;

		case WM_PALETTECHANGED:
			if (hWnd != (HWND) wParam) {
				update_palette();
			}
			return DefWindowProc(hWnd, msg, wParam, lParam);
			break;

		case WM_QUERYNEWPALETTE:
			video_activate( true );
			return(update_palette());

		case WM_PAINT:
			Screen_Draw_All( hWnd );
			return(0);

		case WM_ERASEBKGND:
			// This is needed to prevent flashing
			return(1);

    case WM_SIZE:
			do_size( wParam );
			return DefWindowProc(hWnd, msg, wParam, lParam);

		case WM_KEYDOWN:
			// debugging_fpp = 0;

			if(is_control_down() && is_shift_down()) {
				if(wParam == get_registered_media_hotkey()) {
					media_check(MEDIA_REMOVABLE);
				} else if(wParam == get_registered_floppy_hotkey()) {
					media_check(MEDIA_FLOPPY);
				} else if(wParam == get_registered_cd_hotkey()) {
					media_check(MEDIA_CD);
				} else if(wParam == get_registered_hd_hotkey()) {
					media_check(MEDIA_HD);
				} else if(wParam == get_registered_desktop_hotkey()) {
					// If proper hotkey mechanism fails.
					swap_desktop();
				} else {
					do_key_down(hWnd,msg,wParam,lParam,0);
				}
			} else {
				do_key_down(hWnd,msg,wParam,lParam,0);
			}
			return(0);

		case WM_KEYUP:
			if( is_control_down() && is_shift_down() &&
					( wParam == get_registered_media_hotkey() ||
					  wParam == get_registered_floppy_hotkey() ||
					  wParam == get_registered_cd_hotkey() ||
					  wParam == get_registered_hd_hotkey() ||
					  wParam == get_registered_desktop_hotkey()
					))
			{
				// skip hotkey up messages
			} else {
				do_key_up(hWnd,msg,wParam,lParam,0);
			}
			return(0);

		case WM_SYSKEYDOWN:
			do_key_down(hWnd,msg,wParam,lParam,1);
			return(0);

		case WM_SYSKEYUP:
			do_key_up(hWnd,msg,wParam,lParam,1);
			return(0);

		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			if(m_right_mouse == 1) {
				do_mac_key_down( 0x36 );

				// ADBInterrupt() handles mouse first, then keyboard.
				// So we need to make sure there's a delay of at least one tick interrupt
				Sleep(30);

				ADBMouseDown(0);

				// This is not needed now, but I don't want to rely on the order
				// that things happen in ADBInterrupt(). They may change later.
				Sleep(30);

				do_mac_key_up( 0x36 );
			} else if(!capturing && is_windowed) {
				dragging = true;
				SetCapture(hWnd);
				drag_window_start(hWnd);
			}
			break;

		case WM_RBUTTONUP:
			if(m_right_mouse == 1) {
				ADBMouseUp(0);
				if(capturing) {
					capturing = false;
					ReleaseCapture();
				}
			} else if(is_windowed && dragging) {
				ReleaseCapture();
				dragging = false;
				drag_window_stop(hWnd);
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			if(m_os8_mouse) {
				int y = (int)ReadMacInt16(0x82c);
				if(y < 20) {
					m_menu_clicked = !m_menu_clicked;
				} else {
					m_menu_clicked = false;
				}
			}

			// This is atomic
			ADBMouseDown(0);
			if(!dragging && !capturing) {
				capturing = true;
				SetCapture(hWnd);
			}
			break;

		case WM_LBUTTONUP:
			if(m_os8_mouse) {
				int y = (int)ReadMacInt16(0x82c);
				if(m_menu_clicked && y < 20) break;
			}

			// This is atomic
			ADBMouseUp(0);

			if(capturing) {
				capturing = false;
				ReleaseCapture();
			}
			break;

		case WM_MOUSEMOVE:
			if(dragging) {
				drag_window_move(hWnd);
			}
			return DefWindowProc(hWnd, msg, wParam, lParam);

		default:
			if(m_mousewheel_old_msg && (msg == m_mousewheel_old_msg)) {
				do_mouse_wheel(hWnd,(short)HIWORD(wParam));
				return 0;
			} else {
				return DefWindowProc(hWnd, msg, wParam, lParam);
			}
	}
	return 1;
}

/*
 *  60Hz thread
 */

// This rendition runs at an averege of 58.82 Hz
unsigned int WINAPI tick_func_accurate(LPVOID param)
{
	POINT mp_old, mp_new;

	set_desktop();
	GetCursorPos( &mp_old );

	int32 should_have_ticks = GetTickCount() + 100;

	Sleep(200);

	while(timer_running) {

#ifdef B2PROFILE
		// Profiling introduces a great overhead. Without this, I would be
		// measuring the interrupt functions only.
		Sleep(1000);
#else
		int stime = should_have_ticks - (int)GetTickCount();
		if(stime > 0) Sleep( stime );
		should_have_ticks += 17;
#endif
		GetCursorPos( &mp_new );
		if(mp_new.x != mp_old.x || mp_new.y != mp_old.y) {
			mp_old = mp_new;
			ScreenToClient( hMainWnd, &mp_new );
			ADBMouseMoved( mp_new.x, mp_new.y );
		}
		// Trigger 60Hz interrupt
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}

  threads[THREAD_60_HZ].h = NULL;
  threads[THREAD_60_HZ].tid = 0;
	_endthreadex( 0 );

	return 0;
}

unsigned int WINAPI tick_func_accurate_adb(LPVOID param)
{
	POINT mp_old, mp_new;

	set_desktop();
	GetCursorPos( &mp_old );

	int32 should_have_ticks = GetTickCount() + 100;

	RECT screen_rect;
	SetRect( &screen_rect, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) );

	Sleep(200);

	RECT cl_rect;
	SetRect( &cl_rect, 0, 0, VideoMonitor.x, VideoMonitor.y );

	while(timer_running) {

#ifdef B2PROFILE
		// Profiling introduces a great overhead. Without this, I would be
		// measuring the interrupt functions only.
		Sleep(1000);
#else
		int stime = should_have_ticks - (int)GetTickCount();
		if(stime > 0) Sleep( stime );
		should_have_ticks += 17;
#endif
		GetCursorPos( &mp_new );
		if(mp_new.x != mp_old.x || mp_new.y != mp_old.y) {
			if(!m_menu_select && GetForegroundWindow() == hMainWnd) {
				int dy = mp_new.y - mp_old.y;
				int dx = mp_new.x - mp_old.x;
				POINT mp_new_client = mp_new;
				ScreenToClient( hMainWnd, &mp_new_client );

				if(PtInRect(&cl_rect,mp_new_client)) {
					POINT cl;
					cl.x = (int)ReadMacInt16(0x82e) + dx;
					cl.y = (int)ReadMacInt16(0x82c) + dy;
					if( is_windowed && !PtInRect(&cl_rect,cl) ) {
						ClientToScreen( hMainWnd, &cl );
						mp_old = cl;
					} else {
						ADBMouseMoved( dx, dy );
						RECT rect;
						GetWindowRect( hMainWnd, &rect );
						IntersectRect( &rect, &rect, &screen_rect );
						mp_old.x = (rect.right+rect.left)/2;
						mp_old.y = (rect.bottom+rect.top)/2;
					}
					SetCursorPos( mp_old.x, mp_old.y );
				} else {
					ADBMouseMoved( dx, dy );
					mp_old = mp_new;
				}
			}
		}
		// Trigger 60Hz interrupt
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}

  threads[THREAD_60_HZ].h = NULL;
  threads[THREAD_60_HZ].tid = 0;
	_endthreadex( 0 );

	return 0;
}

// This is a very inaccurate timer.
unsigned int WINAPI tick_func_inaccurate(LPVOID param)
{
	POINT mp_old, mp_new;

	set_desktop();
	GetCursorPos( &mp_old );

	Sleep(200);

	while(timer_running) {

#ifdef B2PROFILE
		// Profiling introduces a great overhead. Without this, I would be
		// measuring the interrupt functions only.
		Sleep(1000);
#else
		Sleep(16);
#endif
		GetCursorPos( &mp_new );
		if(mp_new.x != mp_old.x || mp_new.y != mp_old.y) {
			mp_old = mp_new;
			ScreenToClient( hMainWnd, &mp_new );
			ADBMouseMoved( mp_new.x, mp_new.y );
		}
		// Trigger 60Hz interrupt
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}

  threads[THREAD_60_HZ].h = NULL;
  threads[THREAD_60_HZ].tid = 0;
	_endthreadex( 0 );

	return 0;
}

unsigned int WINAPI tick_func_inaccurate_adb(LPVOID param)
{
	POINT mp_old, mp_new;

	set_desktop();
	GetCursorPos( &mp_old );

	RECT screen_rect;
	SetRect( &screen_rect, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) );

	Sleep(200);

	RECT cl_rect;
	SetRect( &cl_rect, 0, 0, VideoMonitor.x, VideoMonitor.y );

	while(timer_running) {

#ifdef B2PROFILE
		// Profiling introduces a great overhead. Without this, I would be
		// measuring the interrupt functions only.
		Sleep(1000);
#else
		Sleep(16);
#endif
		GetCursorPos( &mp_new );
		if(mp_new.x != mp_old.x || mp_new.y != mp_old.y) {
			if(!m_menu_select && GetForegroundWindow() == hMainWnd) {
				int dy = mp_new.y - mp_old.y;
				int dx = mp_new.x - mp_old.x;
				POINT mp_new_client = mp_new;
				ScreenToClient( hMainWnd, &mp_new_client );

				if(PtInRect(&cl_rect,mp_new_client)) {
					POINT cl;
					cl.x = (int)ReadMacInt16(0x82e) + dx;
					cl.y = (int)ReadMacInt16(0x82c) + dy;
					if( is_windowed && !PtInRect(&cl_rect,cl) ) {
						ClientToScreen( hMainWnd, &cl );
						mp_old = cl;
					} else {
						ADBMouseMoved( dx, dy );
						RECT rect;
						GetWindowRect( hMainWnd, &rect );
						IntersectRect( &rect, &rect, &screen_rect );
						mp_old.x = (rect.right+rect.left)/2;
						mp_old.y = (rect.bottom+rect.top)/2;
					}
					SetCursorPos( mp_old.x, mp_old.y );
				} else {
					ADBMouseMoved( dx, dy );
					mp_old = mp_new;
				}
			}
		}
		// Trigger 60Hz interrupt
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}

  threads[THREAD_60_HZ].h = NULL;
  threads[THREAD_60_HZ].tid = 0;
	_endthreadex( 0 );

	return 0;
}

void WritePrivateProfileInt(
	LPSTR lpAppName,
	LPSTR lpKeyName,
	int value,
	LPSTR lpFileName
)
{
	char buf[30];
	sprintf( buf, "%d", value );
	WritePrivateProfileString( lpAppName, lpKeyName, buf, ini_file_name );
}

static void redirect_std_files(void)
{
	if(GetPrivateProfileInt( "Debug", "redirect_std_files", 0, ini_file_name )) {
		(void)freopen( "stdout.txt", "w+", stdout );
		(void)freopen( "stderr.txt", "w+", stderr );
	}
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	// Most of the code below deals with detecting the previous
	// instance. We cannot afford having two copies running.
	// The perceived complexity of the code arises from the fact
	// that we may have created a separate desktop object
	// and the windows in there are not readily enumerable
	// without switching desktop first.

	HANDLE mutant = 0;

	if(0 == GetPrivateProfileInt( "Debug", "allow_multiple_instances", 0, ini_file_name )) {

		// This mutant is never acquired, used only for run detection
		const char *mname = "Basilisk II is running";

		// FindWindow() works only within a given desktop.
		// Need to use a global method.
		HANDLE mutant = OpenMutex( MUTEX_ALL_ACCESS, FALSE, mname );
		if(mutant) {
			CloseHandle(mutant);

			HDESK hDesk = OpenDesktop( get_desktopname(), 0, FALSE, GENERIC_ALL );
			if(hDesk) {
				USEROBJECTFLAGS uof;
				uof.fInherit = FALSE;
				uof.fReserved = FALSE;
				uof.dwFlags = DF_ALLOWOTHERACCOUNTHOOK;
				SetUserObjectInformation( hDesk, UOI_FLAGS, (LPVOID)&uof, sizeof(uof) );
				SetThreadDesktop(hDesk);
				SwitchDesktop(hDesk);
			}

			// Now it's safe to use FindWindow().
			// Search by class since title varies.
			HWND w = FindWindow( get_wnd_class_name(), 0 );
			if(w) {
				PostMessage( w, WM_HOTKEY,
					(WPARAM)get_registered_desktop_hotkey(), 0 );
			}
			return(0);
		}
		mutant = CreateMutex( FALSE, FALSE, mname );
	}

	hInst = hInstance;

	if(*lpCmdLine) {
		char path[_MAX_PATH];
		if(*lpCmdLine == '\"') lpCmdLine++;
		strcpy( path, lpCmdLine );
		int len = strlen(path);
		if( len > 0 && path[len-1] == '\"' ) path[len-1] = 0;
		SetPrefsFile(path);
	} else {
		SetPrefsFile("BasiliskII_prefs");
	}

	redirect_std_files();

	MSG msg;
	msg.wParam = 0;

	FreeLibrary( LoadLibrary( "DDRAW.DLL" ));

	if( check_os() && check_drivers() ) {
		initialize();
		while ( GetMessage(&msg, NULL, 0, 0) ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if(mutant) CloseHandle(mutant);

  threads[THREAD_GUI].h = NULL;
  threads[THREAD_GUI].tid = 0;

	return(msg.wParam);
}
