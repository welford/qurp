/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_win.c -- Win32 video driver

#include "quakedef.h"
#include "winquake.h"
#include "d_local.h"
#include "resource.h"
#include "./gl.h"

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3

unsigned short	d_8to16table[256]; //never used, but referenced a bunch so we keep it around for now.

extern int	Minimized;

//used in the new stuff
HWND		mainwindow;
HDC			maindc;
//end 

int			window_center_x, window_center_y, window_width, window_height;
RECT		window_rect;

static int		firstupdate = 1, _windowed_mouse = 0;
static qboolean	vid_initialized = false;
static int		lockcount;
static HICON	hIcon;
viddef_t	vid;				// global video state

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 3)

// Note that 0 is MODE_WINDOWED
cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		windowed_mouse = {"windowed_mouse","1", true};
cvar_t		win_fullscreen = {"win_fullscreen","0", true};
cvar_t		vid_fullscreen_mode = {"vid_fullscreen_mode","3", true};
cvar_t		vid_window_x = {"vid_window_x", "0", true};
cvar_t		vid_window_y = {"vid_window_y", "0", true};
cvar_t		show_fps = {"show_fps", "0", true};
cvar_t		r_resolution = {"r_resolution","0", false};

typedef struct {
	int		width;
	int		height;
} lmode_t;

typedef struct {
	char    name[16];
	int		w,h;
} rres_t;

rres_t renderRes[] = {
	{"custom",		-1,    -1},
	//{"160x100",		160,   100}, //low res it possible but you need to make sure 
	{"320x200",		320,   200},
	{"320x240",		320,   240},
	{"400x300",		400,   300},
	{"480x320",		480,   320},
	{"512x384",		512,   384},
	{"640x480",		640,   480},
	{"800x600",		800,   600},
	{"1024x768",	1024,  768},
	{"1284x720",	1284,  720},
	{"1920x1080",	1920,  1080},
	{"2560x1440",	2560,  1440},
};


int			vid_modenum = NO_MODE;
int			vid_testingmode, vid_realmode;
double		vid_testendtime;
int			vid_default = MODE_WINDOWED;

modestate_t	modestate = MS_FULLSCREEN;

static byte		*vid_surfcache;
static int		vid_surfcachesize;
static int		VID_highhunkmark;

unsigned char	vid_curpal[256*3];

//int     mode;

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			mode13;
	int			stretched;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[13];
} vmode_t;

static byte	backingbuf[48*24];

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


/*
================
VID_RememberWindowPos
================
*/
void VID_RememberWindowPos (void)
{
	RECT	rect;

	if (GetWindowRect (mainwindow, &rect))
	{
		if ((rect.left < GetSystemMetrics (SM_CXSCREEN)) &&
			(rect.top < GetSystemMetrics (SM_CYSCREEN))  &&
			(rect.right > 0)                             &&
			(rect.bottom > 0))
		{
			Cvar_SetValue ("vid_window_x", (float)rect.left);
			Cvar_SetValue ("vid_window_y", (float)rect.top);
		}
	}
}


/*
================
VID_CheckWindowXY
================
*/
void VID_CheckWindowXY (void)
{

	if (((int)vid_window_x.value > (GetSystemMetrics (SM_CXSCREEN) - 160)) ||
		((int)vid_window_y.value > (GetSystemMetrics (SM_CYSCREEN) - 120)) ||
		((int)vid_window_x.value < 0)									   ||
		((int)vid_window_y.value < 0))
	{
		Cvar_SetValue ("vid_window_x", 0.0);
		Cvar_SetValue ("vid_window_y", 0.0 );
	}
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (int x, int y)
{

	window_rect.left = x;
	window_rect.top = y;
	window_rect.right = x + window_width;
	window_rect.bottom = y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}


/*
================
VID_CheckAdequateMem
================
*/
qboolean VID_CheckAdequateMem (int width, int height)
{
	int		tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);

	tbuffersize += D_SurfaceCacheForRes (width, height);

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
	if ((host_parms.memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
		 0x10000 * 3) < minimum_memory)
	{
		return false;		// not enough memory for mode
	}

	return true;
}


/*
================
VID_AllocBuffers
================
*/
qboolean VID_AllocBuffers (int width, int height)
{
	int		tsize, tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);

	tsize = D_SurfaceCacheForRes (width, height);

	tbuffersize += tsize;

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
	if ((host_parms.memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
		 0x10000 * 3) < minimum_memory)
	{
		Con_SafePrintf ("Not enough memory for video mode\n");
		return false;		// not enough memory for mode
	}

	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
	}

	VID_highhunkmark = Hunk_HighMark ();

	d_pzbuffer = Hunk_HighAllocName (tbuffersize, "video");

	vid_surfcache = (byte *)d_pzbuffer +
			width * height * sizeof (*d_pzbuffer);
	
	return true;
}


void initFatalError(void)
{
	exit(EXIT_FAILURE);
}


BITMAPINFO * pbmi = 0;
LPBYTE pBits = NULL;
HBITMAP hBitMap = 0;
//LPLOGPALETTE ppltt;

void VID_LockBuffer (void)
{
	lockcount++;

	if (lockcount > 1)
		return;

	vid.buffer = vid.conbuffer = vid.direct = pBits;
	vid.rowbytes = vid.conrowbytes = sizeof(char) * vid.width;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = (void *)(byte *)vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;

	if (lcd_x.value)
		screenwidth <<= 1;
}
		
		
void VID_UnlockBuffer (void)
{
	lockcount--;

	if (lockcount > 0)
		return;

	if (lockcount < 0)
		Sys_Error ("Unbalanced unlock");

	// to turn up any unlocked accesses
	vid.buffer = vid.conbuffer = vid.direct = d_viewbuffer = NULL;

}


int VID_ForceUnlockedAndReturnState (void)
{
	int	lk;

	if (!lockcount)
		return 0;

	lk = lockcount;

	lockcount = 1;
	VID_UnlockBuffer ();

	return lk;
}


void VID_ForceLockState (int lk)
{

	if (lk)
	{
		lockcount = 0;
		VID_LockBuffer ();
	}

	lockcount = lk;
}


void VID_SetPalette (unsigned char *palette)
{
	INT			i;
	palette_t	pal[256];
    HDC			hdc;

	//TODO
}


void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette (palette);
}

void VID_Fullscreen(bool);
void VID_Windowed_f (void){ VID_Fullscreen(false); }
void VID_Fullscreen_f (void) { VID_Fullscreen(true);}


/*
=================
VID_Minimize_f
=================
*/
void VID_Minimize_f (void)
{

// we only support minimizing windows; if you're fullscreen,
// switch to windowed first
	if (modestate == MS_WINDOWED) //JWA
		ShowWindow (mainwindow, SW_MINIMIZE);
}


static const unsigned int multisamples = 0; //if we are going down the blitting/FBO route we need to make sure multisamples match between , FBO and backbuffer
static const unsigned int clr_bits = 16, alpha_bits = 8, depth_bits = 16, stencil_bits = 8;

/*
===================
DefineAndSetPixelFormat
===================
*/
int DefineAndSetPixelFormat(PIXELFORMATDESCRIPTOR* pfd, HDC hdc, int clrBits, int alphaBits, int dphBits, int stnBits)
{
	int pixelFormat;
	ZeroMemory(pfd, sizeof(pfd));
	pfd->nSize = sizeof(pfd);
	pfd->nVersion = 1;
	pfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd->iPixelType = PFD_TYPE_RGBA;
	pfd->cColorBits = clrBits;
	pfd->cAlphaBits = alphaBits;
	pfd->cDepthBits = dphBits;
	pfd->cStencilBits = stnBits;
	pfd->iLayerType = PFD_MAIN_PLANE;
	pixelFormat = ChoosePixelFormat(hdc, pfd);
	return SetPixelFormat(hdc, pixelFormat, pfd);
}

/*
===================
GetBetterFormat
===================
*/
int GetBetterFormat(HDC hdc, unsigned int ms, int clrBits, int alphaBits, int dphBits, int stnBits)
{
	/*
	whould probably check that ms is a valid Multisanmple number
	*/
	static const int SAMPLE_BUFFER_VALUE_IDX = 3;
	int pixelAttribs[] =
	{
		WGL_SAMPLES_ARB, ms,
		WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, clrBits,
		WGL_ALPHA_BITS_ARB, alphaBits,
		WGL_DEPTH_BITS_ARB, dphBits,
		WGL_STENCIL_BITS_ARB, stnBits,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		0
	};
	int* sampleCount = pixelAttribs + 1;
	int* useSampleBuffer = pixelAttribs + 3;
	int pixelFormat = -1;
	unsigned int numFormats;

	//this is a bad bit of code....
	if (ms == 0)
		pixelAttribs[SAMPLE_BUFFER_VALUE_IDX] = GL_FALSE;
	else
		pixelAttribs[SAMPLE_BUFFER_VALUE_IDX] = GL_TRUE;


	// Try fewer and fewer samples per pixel till we find one that is supported:
	if (*sampleCount > 0)
	{
		while (pixelFormat <= 0 && *sampleCount >= 0)
		{
			wglChoosePixelFormatARB(hdc, pixelAttribs, 0, 1, &pixelFormat, &numFormats);
			(*sampleCount)--;
			if (*sampleCount < 0 && pixelFormat <= 0) {
				printf("Couldn't find a suitable MSAA format\n");
			}
		}
		if (pixelFormat != -1) {
			printf("MSAA samples : %d\n", *sampleCount);//*binds tighter than +1					
		}
	}
	else
	{
		wglChoosePixelFormatARB(hdc, pixelAttribs, 0, 1, &pixelFormat, &numFormats);
	}

	if (pixelFormat < 0) {
		printf("Couldn't find a suitable Pixel format\n");
	}

	return pixelFormat;

}


void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	RECT    rect;
	int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY * 2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0 : CenterX;
	CenterY = (CenterY < 0) ? 0 : CenterY;
	SetWindowPos(hWndCenter, NULL, CenterX, CenterY, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

static void VID_SizeChanged()
{
	RECT window,render;
	GetWindowRect(mainwindow, &window);
	GetClientRect(mainwindow, &render);
	window_width = (render.right-render.left);
	window_height = (render.bottom-render.top);
	//vid.aspect = ((float)window_height / (float)window_width) * (320.0 / 240.0); //look into this
	//force a redraw
	scr_fullupdate = 0;
	SCR_UpdateScreen();
	VID_UpdateWindowStatus(window.left,window.top);
}

void VID_Fullscreen(bool full)
{
	static OrigStyle, OrigExStyle;
	static RECT OrigRect;
	//https://stackoverflow.com/questions/4041331/screen-area-vs-work-area-rectangle
	//general flow should be
	//1.  start windows											DONE
	//2.  use clicks "maximize windows" we do the above			DONE
	//3. user clicks use full screen we maximize as per normal with windows (show sys menu, min/max buttons)	DONE
	//4. use clicks full screen in menu it's the full screen borderless window we currenty have below	DONE
	//5. start without -startwindowed is the same as 4			DONE

	
	if(full) {
		OrigStyle = GetWindowLong(mainwindow, GWL_STYLE);
		OrigExStyle = GetWindowLong(mainwindow, GWL_EXSTYLE);
		GetWindowRect(mainwindow, &OrigRect);

		SetWindowLong(mainwindow, GWL_STYLE, OrigStyle & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(mainwindow, GWL_EXSTYLE, OrigExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

		MONITORINFO monitor_info;
		monitor_info.cbSize = sizeof(monitor_info);
		GetMonitorInfo(MonitorFromWindow(mainwindow, MONITOR_DEFAULTTONEAREST),
						&monitor_info);
		const RECT rcWnd = monitor_info.rcMonitor;
		SetWindowPos(mainwindow, NULL, rcWnd.left,rcWnd.top,(rcWnd.right-rcWnd.left),(rcWnd.bottom-rcWnd.top),	SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

		win_fullscreen.value = 1;
	}
	else {
		SetWindowLong(mainwindow, GWL_STYLE, OrigStyle);
		SetWindowLong(mainwindow, GWL_EXSTYLE, OrigExStyle);
		SetWindowPos(mainwindow, NULL, OrigRect.left,OrigRect.top,(OrigRect.right-OrigRect.left),(OrigRect.bottom-OrigRect.top), SWP_SHOWWINDOW | SWP_FRAMECHANGED);

		win_fullscreen.value = 0;
	}
	VID_SizeChanged();
}

#define WIN_CLASS_NAME "Quake"
void VID_InitNew (HINSTANCE hInstance)
{
	hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON2));

	WNDCLASSEX wc = {				//http://msdn.microsoft.com/en-us/library/windows/desktop/ms633577(v=vs.85).aspx
		sizeof(WNDCLASSEX),
		CS_CLASSDC,					// see http://msdn.microsoft.com/en-us/library/windows/desktop/ff729176(v=vs.85).aspx
		(WNDPROC)MainWndProc,		//windows procedure
		0L,
		0L,
		hInstance,					//windows instance
		0,							//icon
		LoadCursor(NULL, IDC_ARROW),//cursor
		0,							//background
		0,							//menu name
		WIN_CLASS_NAME,					//class name
		0
	};								//wclass icon	

	if(!RegisterClassEx(&wc))
		Sys_Error ("Couldn't register window class");

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
}


qboolean VID_InitRenderMode()
{
	HDC		hdc;
	//used to try and get the render context that we want
	PIXELFORMATDESCRIPTOR pfd;
	HGLRC	tempRC,baseRC;
	int		pixel_format = 0;
	RECT	WindowRect, rect;
	int		RenderWidth, RenderHeight;

	WindowRect.top = WindowRect.left = 0;

	if (COM_CheckParm("-windowwidth") && COM_CheckParm("-windowheight"))
	{
		window_width = Q_atoi(com_argv[COM_CheckParm("-windowwidth") + 1]);
		window_height = Q_atoi(com_argv[COM_CheckParm("-windowheight") + 1]);

		WindowRect.right = window_width;
		WindowRect.bottom = window_height;

	}
	else
	{
		WindowRect.right = 1920;
		WindowRect.bottom = 1080;
	}

	if (COM_CheckParm("-renderwidth") && COM_CheckParm("-renderheight"))
	{
		RenderWidth = Q_atoi(com_argv[COM_CheckParm("-renderwidth") + 1]);
		RenderHeight = Q_atoi(com_argv[COM_CheckParm("-renderheight") + 1]);

	}
	else
	{
		RenderWidth = 320;
		RenderHeight = 240;
	}

	if(RenderWidth < 320) RenderWidth = 320;
	if(RenderHeight < 240) RenderHeight = 240;

	DWORD	WindowStyle, ExWindowStyle;
	WindowStyle = WS_POPUP | WS_VISIBLE;
	ExWindowStyle = 0;
	rect = WindowRect;

	modestate = MS_FULLSCREEN;
	if (COM_CheckParm("-startwindowed"))//JAMES change this, just use windowed fullscreen!
	{
		modestate = MS_WINDOWED;
	}
	win_fullscreen.value = 0;
	WindowStyle = WS_OVERLAPPED | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

	window_width = rect.right - rect.left;
	window_height = rect.bottom - rect.top;
	int window_x = GetSystemMetrics(SM_CXSCREEN) / 2 - window_width / 2;
	int window_y = GetSystemMetrics(SM_CYSCREEN) / 2 - window_height / 2;

	//use the dialogue window to get a gl render context
	maindc = GetDC(hwnd_dialog);
	DefineAndSetPixelFormat(&pfd, maindc, clr_bits, alpha_bits, depth_bits, stencil_bits);
	tempRC = wglCreateContext(maindc);

	if (!tempRC)
		Sys_Error("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent(maindc, tempRC))
		Sys_Error("wglMakeCurrent failed");

	InitialiseWGLFunctionPointers();

	pixel_format = GetBetterFormat(maindc, multisamples, clr_bits, alpha_bits, depth_bits, stencil_bits);
	if (pixel_format >= 0)
	{
		RECT rect = {0,0,window_width,window_height};

		AdjustWindowRectEx(&rect,WindowStyle,false,ExWindowStyle);
		int adj_window_width = rect.right - rect.left;
		int adj_window_height = rect.bottom - rect.top;	

		// Win32 allows the pixel format to be set only once per app, so destroy and re-create the app:
		wglDeleteContext(tempRC); //might be redundant and the window is destroyed below?
		mainwindow = CreateWindowEx(ExWindowStyle, WIN_CLASS_NAME, "Quake", WindowStyle, window_x, window_y, adj_window_width, adj_window_height, 0, 0, 0, 0);
		maindc = GetDC(mainwindow);
		SetPixelFormat(maindc, pixel_format, &pfd);
		baseRC = wglCreateContext(maindc);
		wglMakeCurrent(maindc, baseRC);
	}
	else
	{
		Sys_Error("VID_SetWindowedMode failed to find suitable pixel format");
	}
	maindc = GetDC(mainwindow); //update the main dc to the newly created window

	if (!mainwindow)
		Sys_Error("Couldn't create DIB window");

	// because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC(mainwindow, hdc);
	vid.width = vid.conwidth = RenderWidth;
	vid.height = vid.conheight = RenderHeight;
	vid.numpages = 1;

	SendMessage(mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage(mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	ShowWindow(mainwindow, SW_SHOWDEFAULT);
	DestroyWindow (hwnd_dialog); //hide "starting quake" window

	if(modestate == MS_FULLSCREEN)
	{
		VID_Fullscreen(true);
	}

	return true;
}

unsigned char * p_palette = NULL;
void	VID_SetPaletteNew(unsigned char* palette){
	p_palette = palette;
}

extern int renderLastSecondCnt;

void VID_DeleteDIBNew () {
	DeleteObject(hBitMap);
	free(pbmi);
	pbmi = 0;
	hBitMap = 0;

}

void VID_CreateDIBNew (int w, int h, unsigned char *palette) {
	int i = 0, row=0, col=0, clr=0;
	LPBYTE pb = NULL;
	if(pbmi){VID_DeleteDIBNew();}
	//Create DIB SECTION
	pbmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)); //256 = 16*16 palette size
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = w;
	pbmi->bmiHeader.biHeight = -h;
	pbmi->bmiHeader.biPlanes = 1; //can only be 1 see docs
	pbmi->bmiHeader.biBitCount = 8;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = 0; //0 if we are BI_RGB, which we are
	pbmi->bmiHeader.biXPelsPerMeter = 0;
	pbmi->bmiHeader.biYPelsPerMeter = 0;
	pbmi->bmiHeader.biClrUsed = 0; //0 == maximum
	pbmi->bmiHeader.biClrImportant = 0;

	RGBQUAD *pClrMap = ((LPSTR)pbmi + (WORD)(pbmi->bmiHeader.biSize));
	//copy out palette
	for (i = 0; i < 256; i++)
	{
		pbmi->bmiColors[i].rgbRed =      palette[(i * 3) + 0];
		pbmi->bmiColors[i].rgbGreen =    palette[(i * 3) + 1];
		pbmi->bmiColors[i].rgbBlue =     palette[(i * 3) + 2];
		pbmi->bmiColors[i].rgbReserved = 0;
	}

	//DIB_RGB_COLORS here referes to the colours in the palette
	hBitMap = CreateDIBSection(maindc,pbmi,DIB_RGB_COLORS, &pBits,NULL,0);
	
	//fill framebuffer with palette
	pb = pBits;
	for (int _h=0; _h<h; _h++)
	{
		for (int _w=0; _w<w; _w++)
		{
			int tmp = (_w/(w / 16)) + ((_h/(h/ 16))*16);
			*pb = tmp;
			pb++;
		}
	}
	//
	vid.buffer = vid.conbuffer = vid.direct = pBits;
	vid.rowbytes = vid.conrowbytes = sizeof(char) * w;
	vid.numpages = 1;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.height = vid.conheight = h;
	vid.width = vid.conwidth = w;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	SetStretchBltMode(maindc,COLORONCOLOR); //seems to default to BLACKONWHITE which creates a darker image when downscaling
}

void VID_DrawDIBNew(int w, int h) {
	if (pbmi) {
		float ww = (float)(w) / (float)(vid.width) * window_width;
		float wh = (float)(h) / (float)(vid.height) * window_height;
		StretchDIBits(maindc, 0, 0, ww, wh, vid.width-w, vid.height-h, w, h, pBits, pbmi, DIB_RGB_COLORS, SRCCOPY);
		renderLastSecondCnt++;
	}
}

extern qboolean r_viewchanged;
void VID_SetRenderResolutionIdx (int idx)
{
	if (idx < 0) { idx = 0; }
	if (idx >= (sizeof(renderRes) / sizeof(renderRes[0]))) { 
		idx = (sizeof(renderRes) / sizeof(renderRes[0])) - 1; 
	}
	if (idx == 0) {
		
	}
	else {
		r_viewchanged = 1;
		VID_CreateDIBNew(renderRes[idx].w,renderRes[idx].h,p_palette);
		if (!VID_AllocBuffers (vid.height,vid.width))
		{
			printf("SOMETHING WENT WRONG");
		}
		D_InitCaches (vid_surfcache, vid_surfcachesize);
		//Draw_Init ();
		//SCR_Init ();
		//R_Init ();
	}
	r_resolution.value = idx; //
	scr_fullupdate = 0;			//force full redraw (inc status)
}

void VID_IncRenderResolution_f (void)
{
	VID_SetRenderResolutionIdx(r_resolution.value + 1);
}

void VID_DecRenderResolution_f (void)
{
	VID_SetRenderResolutionIdx(r_resolution.value - 1);
}

char* VID_GetRenderResolutionStr (void)
{
	int temp = (int)(r_resolution.value);
	return renderRes[temp].name;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void AppActivate(BOOL fActive, BOOL minimize);

void	VID_Init (unsigned char *palette)
{
	int		i, bestmatch, bestmatchmetric, t, dr, dg, db;
	int		basenummodes;
	byte	*ptmp;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&windowed_mouse);
	Cvar_RegisterVariable (&vid_fullscreen_mode);
	Cvar_RegisterVariable (&vid_window_x);
	Cvar_RegisterVariable (&vid_window_y);
	Cvar_RegisterVariable (&show_fps);

	Cmd_AddCommand ("vid_windowed", VID_Windowed_f);
	Cmd_AddCommand ("vid_fullscreen", VID_Fullscreen_f);
	Cmd_AddCommand ("vid_minimize", VID_Minimize_f);
	Cmd_AddCommand ("vid_resolutionup", VID_IncRenderResolution_f);
	Cmd_AddCommand ("vid_resolutiondown", VID_DecRenderResolution_f);

	VID_InitNew(global_hInstance);
	VID_InitRenderMode();
	S_Init();
	VID_SetPaletteNew(palette);
	VID_CreateDIBNew(vid.width, vid.height, palette);
	//alloc vid memory/cache stuff
	if (!VID_AllocBuffers (vid.width, vid.height))
	{
		printf("SOMETHING WENT WRONG"); 
	}
	vid_initialized = true;
	D_InitCaches (vid_surfcache, vid_surfcachesize);
	VID_DrawDIBNew(vid.width, vid.height);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int*)vid.colormap + 2048));
	vid_testingmode = 0;
	AppActivate(true, false);
	if(windowed_mouse.value) {
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
}


void	VID_Shutdown (void)
{
	HDC				hdc;
	int				dummy;

	if (vid_initialized)
	{
		PostMessage (HWND_BROADCAST, WM_PALETTECHANGED, (WPARAM)mainwindow, (LPARAM)0);
		PostMessage (HWND_BROADCAST, WM_SYSCOLORCHANGE, (WPARAM)0, (LPARAM)0);
		AppActivate(false, false);
		vid_testingmode = 0;
		vid_initialized = 0;
	}
}


/*
================
FlipScreen
================
*/
void FlipScreen(vrect_t *rects)
{
	if(!rects)return;
	// Flip the surfaces
	VID_DrawDIBNew(rects->width,rects->height);
}

extern double updateTimeSecs;
extern int framesLastSecond,renderLastSecond;
void	VID_Update (vrect_t *rects)
{
	vrect_t	rect;
	RECT	trect;

	if (firstupdate)
	{
		GetWindowRect (mainwindow, &trect);

		if ((trect.left != (int)vid_window_x.value) ||
			(trect.top  != (int)vid_window_y.value))
		{
			if (COM_CheckParm ("-resetwinpos"))
			{
				Cvar_SetValue ("vid_window_x", 0.0);
				Cvar_SetValue ("vid_window_y", 0.0);
			}

			VID_CheckWindowXY ();
			SetWindowPos (mainwindow, NULL, (int)vid_window_x.value,
				(int)vid_window_y.value, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
		}
		firstupdate = 0;
	}

	if(show_fps.value){
		#define DLEN 16
		static char pFPS[DLEN] = "",pMS[DLEN] = "";
		int fps = 1.0f / updateTimeSecs;
		VID_LockBuffer();
		snprintf(pFPS,DLEN,"FPS:%d",fps);
		Draw_String (200, 0, pFPS);
		snprintf(pMS,DLEN,"secs:%f",updateTimeSecs);
		Draw_String (200, 8, pMS);
		snprintf(pMS,DLEN,"TPS:%d",framesLastSecond);
		Draw_String (200, 16, pMS);
		snprintf(pMS,DLEN,"RPS:%d",renderLastSecond);
		Draw_String (200, 24, pMS);
		VID_UnlockBuffer();
		#undef DLEN
	}

	// We've drawn the frame; copy it to the screen
	FlipScreen (rects);

	if ((int)vid_mode.value != vid_realmode)
	{
		//VID_SetMode ((int)vid_mode.value, vid_curpal);
		Cvar_SetValue ("vid_mode", (float)vid_modenum);
							// so if mode set fails, we don't keep on
							//  trying to set that mode
		vid_realmode = vid_modenum;
	}

	// handle the mouse state when windowed if that's changed
	if ((int)windowed_mouse.value != _windowed_mouse)
	{
		if (windowed_mouse.value)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}

		_windowed_mouse = (int)windowed_mouse.value;
	}
}


/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	int		i, j, reps, repshift;
	vrect_t	rect;

	if (!vid_initialized)
		return;

	if (vid.aspect > 1.5)
	{
		reps = 2;
		repshift = 1;
	}
	else
	{
		reps = 1;
		repshift = 0;
	}

	VID_LockBuffer ();

	if (!vid.direct)
		Sys_Error ("NULL vid.direct pointer");

	for (i=0 ; i<(height << repshift) ; i += reps)
	{
		for (j=0 ; j<reps ; j++)
		{
			memcpy (&backingbuf[(i + j) * 24],
					vid.direct + x + ((y << repshift) + i + j) * vid.rowbytes,
					width);
			memcpy (vid.direct + x + ((y << repshift) + i + j) * vid.rowbytes,
					&pbitmap[(i >> repshift) * width],
					width);
		}
	}

	VID_UnlockBuffer ();

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height << repshift;
	rect.pnext = NULL;

	FlipScreen (&rect);

}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
	int		i, j, reps, repshift;
	vrect_t	rect;

	if (!vid_initialized)
		return;

	if (vid.aspect > 1.5)
	{
		reps = 2;
		repshift = 1;
	}
	else
	{
		reps = 1;
		repshift = 0;
	}

	VID_LockBuffer ();

	if (!vid.direct)
		Sys_Error ("NULL vid.direct pointer");

	for (i=0 ; i<(height << repshift) ; i += reps)
	{
		for (j=0 ; j<reps ; j++)
		{
			memcpy (vid.direct + x + ((y << repshift) + i + j) * vid.rowbytes,
					&backingbuf[(i + j) * 24],
					width);
		}
	}

	VID_UnlockBuffer ();

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height << repshift;
	rect.pnext = NULL;

	FlipScreen (&rect);
}


//==========================================================================

byte        scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10,  K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
}; 

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;

	return scantokey[key];
}

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
    HDC			hdc;
    int			i, t;
	static BOOL	sound_active;

	ActiveApp = fActive;

	// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		S_ClearBuffer ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		S_ClearBuffer ();
		sound_active = true;
	}

	// minimize/restore fulldib windows/mouse-capture normal windows on demand
	if (windowed_mouse.value)
	{
		if (ActiveApp)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}

		if (!ActiveApp)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

void VID_MouseActive(qboolean active)
{

	if (windowed_mouse.value)
	{
		if (active)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}


/*
================
VID_HandlePause
================
*/
void VID_HandlePause (qboolean pause)
{

	VID_MouseActive(!pause);
}


/*
===================================================================

MAIN WINDOW

===================================================================
*/

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
	LONG			lRet = 0;
	int				fwKeys, xPos, yPos, fActive, fMinimized, temp;
	HDC				hdc;
	PAINTSTRUCT		ps;
	static int		recursiveflag;
	printf("msg: %d\n",uMsg);
	switch (uMsg)
	{
		case WM_CREATE:
			break;
		case WM_SYSCOMMAND:

		// Check for maximize being hit
			switch (wParam & ~0x0F)
			{
				case SC_MAXIMIZE:
					lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
					break;
				default:
					S_BlockSound ();
					S_ClearBuffer ();

					lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);

					S_UnblockSound ();
			}
			break;

		case WM_MOVE:
			VID_UpdateWindowStatus ((int) LOWORD(lParam), (int) HIWORD(lParam));
			VID_DrawDIBNew(vid.width, vid.height);
			if (!Minimized)
				VID_RememberWindowPos ();
			break;
		case WM_SIZE:
			Minimized = false;
			if (!(wParam & SIZE_RESTORED))
			{
				if (wParam & SIZE_MINIMIZED)
					Minimized = true;
			}
			VID_SizeChanged();
			break;
		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);
		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();
			break;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);

			VID_DrawDIBNew(vid.width, vid.height);

			if (host_initialized)
				SCR_UpdateWholeScreen ();

			EndPaint(hWnd, &ps);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (MapKey(lParam), true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (MapKey(lParam), false);
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);
			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;
		// KJB: Added these new palette functions
		case WM_PALETTECHANGED:
			if ((HWND)wParam == hWnd)
				break;
		case WM_DISPLAYCHANGE:
			break;

   	    case WM_CLOSE:
		// this causes Close in the right-click task bar menu not to work, but right
		// now bad things happen if Close is handled in that case (garbage and a
		// crash on Win95)
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}
			break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
	        break;
    }

    /* return 0 if handled message, 1 if not */
    return lRet;
}


extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
	int		ismode13;
	int		width;
} modedesc_t;

#define MAX_COLUMN_SIZE		5
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 6)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			lnummodes, i, j, k, column, row, dup, dupmode;
	char		temp[100];
	vmode_t		*pv;
	modedesc_t	tmodedesc;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);


	M_Print (13*8, 36, "Legacy Menu");
}


/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	if (vid_testingmode)
		return;

	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
				   ((vid_line + 2) % VID_ROW_SIZE);

		if (vid_line >= vid_wmodes)
			vid_line = vid_wmodes - 1;
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
				   ((vid_line + 4) % VID_ROW_SIZE);

		if (vid_line >= vid_wmodes)
			vid_line = (vid_line / VID_ROW_SIZE) * VID_ROW_SIZE;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line -= VID_ROW_SIZE;

		if (vid_line < 0)
		{
			vid_line += ((vid_wmodes + (VID_ROW_SIZE - 1)) /
					VID_ROW_SIZE) * VID_ROW_SIZE;

			while (vid_line >= vid_wmodes)
				vid_line -= VID_ROW_SIZE;
		}
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line += VID_ROW_SIZE;

		if (vid_line >= vid_wmodes)
		{
			vid_line -= ((vid_wmodes + (VID_ROW_SIZE - 1)) /
					VID_ROW_SIZE) * VID_ROW_SIZE;

			while (vid_line < 0)
				vid_line += VID_ROW_SIZE;
		}
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu1.wav");
		//VID_SetMode (modedescs[vid_line].modenum, vid_curpal);
		break;

	case 'T':
	case 't':
		S_LocalSound ("misc/menu1.wav");
	// have to set this before setting the mode because WM_PAINT
	// happens during the mode set and does a VID_Update, which
	// checks vid_testingmode
		vid_testingmode = 1;
		vid_testendtime = realtime + 5.0;

		/*if (!VID_SetMode (modedescs[vid_line].modenum, vid_curpal))
		{
			vid_testingmode = 0;
		}*/
		break;

	case 'D':
	case 'd':
		S_LocalSound ("misc/menu1.wav");
		firstupdate = 0;
		break;

	default:
		break;
	}
}
