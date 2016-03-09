#include "GL/glew.h"
#include "GL/wglew.h"
#include "window.hpp"
#include "buttons.hpp"
#include "buttons_win32.hpp"

#include <tl/cstdint.h>
#include <tl/bits.h>
#include <tl/std.h>
#include <stdio.h>

#if 0
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#endif

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxguid.lib")

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")

namespace gfx {

#define TICKS_FROM_MS(ms) ((ms) * 10000)

#define INPUT_BUFFER_SIZE (32)

#define CHECK(r) do { if(r) goto fail; } while(0)
#define CHECKB(r) do { if(!(r)) goto fail; } while(0)
#define CHECKR(r) do { err = (r); if(err) goto fail; } while(0)

namespace {

typedef struct win32_window : common_window {

	win32_window();

	void close();
	void on_focus();
	void on_unfocus();
	int setup_input();

	HWND hwnd;
	HDC hdc;
	int closed;
	int iconified;
	
#if 0
	IDirectInput8* input;
	IDirectInputDevice8* keyboard;
	IDirectInputDevice8* mouse;
#endif
	int swapMouse;
	u32 fsaaLevels;
} win32_window;

}

#include "window_common.hpp"
#include "buttons_common.hpp"

win32_window::win32_window()
	: hwnd(0), hdc(0), closed(0), iconified(0),
	  swapMouse(0), fsaaLevels(0) {
	init_keymaps();
}

common_window* window_create() {
	win32_window* self = new win32_window();
	return self;
}

void win32_window::close() {
	set_visible(0);
	this->closed = 1;
}

common_window::~common_window() {
	win32_window* self = static_cast<win32_window*>(this);
	self->close();
	if(self->hdc) ReleaseDC(self->hwnd, self->hdc);
	if(self->hwnd) DestroyWindow(self->hwnd);
}

// Mode guessing experimentally adapted from GLFW library.
// http://glfw.sourceforge.net/

static int find_closest_video_mode(int *w, int *h, int *bpp, int *refresh) {

	int mode, bestmode, match, bestmatch, rr, bestrr, success;
	DEVMODE dm;

	// Find best match
	bestmatch = 0x7fffffff;
	bestrr = 0x7fffffff;
	mode = bestmode = 0;
	do {
		dm.dmSize = sizeof(DEVMODE);
		success = EnumDisplaySettings(NULL, mode, &dm);
		if (success) {
			match = dm.dmBitsPerPel - *bpp;
			if (match < 0) match = -match;
			match = (match << 25) |
					((dm.dmPelsWidth - *w) * (dm.dmPelsWidth - *w) +
						(dm.dmPelsHeight - *h) * (dm.dmPelsHeight - *h));
			if (match < bestmatch) {
				bestmatch = match;
				bestmode = mode;
				bestrr = (dm.dmDisplayFrequency - *refresh) *
							(dm.dmDisplayFrequency - *refresh);
			}
			else if (match == bestmatch && *refresh > 0) {
				rr = (dm.dmDisplayFrequency - *refresh) *
						(dm.dmDisplayFrequency - *refresh);
				if (rr < bestrr) {
					bestmatch = match;
					bestmode = mode;
					bestrr = rr;
				}
			}
		}
		++mode;
	} while (success);

	// Get the parameters for the best matching display mode
	dm.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL, bestmode, &dm);

	*w = dm.dmPelsWidth;
	*h = dm.dmPelsHeight;
	*bpp = dm.dmBitsPerPel;
	*refresh = dm.dmDisplayFrequency;

	return bestmode;
}

static int set_video_mode(int mode) {

	// Get the parameters
	DEVMODE dm;
	dm.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL, mode, &dm);

	// Set which fields we want to specify
	dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

	// Change display setting
	dm.dmSize = sizeof(DEVMODE);
	if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		return -1;
	return 0;
}

void win32_window::on_focus() {
	int w = this->width, h = this->height, bpp = this->bpp, rr = this->refresh_rate;
	if (this->fullscreen) {
		if (set_video_mode(find_closest_video_mode(&w, &h, &bpp, &rr)) == 0) {
			this->refresh_rate = rr;
			this->bpp = bpp;
		}
	}
}

void win32_window::on_unfocus() {
	if (this->fullscreen)
		ChangeDisplaySettings(NULL, CDS_FULLSCREEN);
}

static LRESULT CALLBACK windowProc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam) {
	LONG_PTR lptr = GetWindowLongPtr(wnd, GWLP_USERDATA);

	if (lptr) {
		win32_window* self = (win32_window*)lptr;
	
		switch (message) {
			case WM_SETFOCUS:
				if(self->fullscreen && IsWindowVisible(wnd)) {

					if(self->iconified) {
						OpenIcon(wnd);
						
						self->on_focus();
						self->iconified = 0;
					}
					return 0;
				}
				break;

			case WM_KILLFOCUS:
				if (self->fullscreen && IsWindowVisible(wnd)) {

					if(!self->iconified) {
						self->on_unfocus();
						CloseWindow(wnd);
						self->iconified = 1;
					}
					return 0;
				}
				break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:

				if(wparam == VK_F4 && (lparam & (1<<29)))
					self->close();

				//printf("%d %x %x\n", message, wparam, lparam);
				return 0;
	
			case WM_SETCURSOR:
				if (LOWORD(lparam) == HTCLIENT) {
					SetCursor(0);
					return TRUE;
				}
				break;

			case WM_CLOSE:
				self->close();
				return 0;

			case WM_INPUT: {
				UINT dwSize;

				GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

				if (dwSize > 0) {
					void* rip = alloca(dwSize);

					if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, rip, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
						printf("Wrong size\n");
					}

					RAWINPUT* ri = static_cast<RAWINPUT*>(rip);
					
					printf("type = %d. VK = %x. EI = %d. F = %d. M = %d. MC = %d.\n",
						ri->header.dwType, ri->data.keyboard.VKey, ri->data.keyboard.ExtraInformation,
						ri->data.keyboard.Flags, ri->data.keyboard.Message, ri->data.keyboard.MakeCode);

					/*
					event ev = {
						ev_button,
						(ri->data.keyboard.Flags & 1) ^ 1
					};

					ev.d.button.id = ri->data.keyboard.VKey;
					*/

					set_button(self, gfx_native_to_keys[ri->data.keyboard.VKey], (ri->data.keyboard.Flags & 1) ^ 1, 0, 1);

					//self->events.push_back(ev);
				}

				// Let DefWindowProc clean up
				break;
			}

			case WM_PAINT: {
				int swap = 0;

				/* TODO?
				try
				{
				swap = self->draw();
				}
				catch (...)
				{
				throw;
				}*/

				if (swap) {
					SwapBuffers(self->hdc);
				}

				ValidateRect(wnd, 0);
				return 0;
			}
		}
	}

	return DefWindowProc(wnd, message, wparam, lparam);
}

static LPCTSTR window_class() {
	WNDCLASS wc;
	static LPCTSTR name = 0;
	if (name)
		return name;

	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = "Flip::Window";
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = windowProc;
	//wc.cbClsExtra = 0;
	//wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(0);
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(101));
	//wc.hCursor = 0;
	//wc.hbrBackground = 0;
	//wc.lpszMenuName = 0;
		
	name = (LPCTSTR)RegisterClass(&wc);
	//check(name, "registering a window class");
	return name;
}

static void process_messages() {

	MSG message;

	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
		//if (!handledByHook(message))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
	}
		
}

#if 0
#define CHECKDI(r) do { err = (r); if(FAILED(err)) goto fail; } while(0)

static DIOBJECTDATAFORMAT c_rgodfDIKeyboard[256];
static const DIDATAFORMAT c_dfDIKeyboard = { 24, 16, 0x2, 256, 256, c_rgodfDIKeyboard };

static DIOBJECTDATAFORMAT c_rgodfDIMouse[7] = {
	{ &GUID_XAxis, 0, 0x00FFFF03, 0 },
	{ &GUID_YAxis, 4, 0x00FFFF03, 0 },
	{ &GUID_ZAxis, 8, 0x80FFFF03, 0 },
	{ NULL,       12, 0x00FFFF0C, 0 },
	{ NULL,       13, 0x00FFFF0C, 0 },
	{ NULL,       14, 0x80FFFF0C, 0 },
	{ NULL,       15, 0x80FFFF0C, 0 }
};
static const DIDATAFORMAT c_dfDIMouse = { 24, 16, 0x2, 16, 7, c_rgodfDIMouse };


typedef HRESULT (WINAPI *DIRECTINPUT8CREATE)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter);

static HANDLE di_library;
#endif

int win32_window::setup_input() {
	RAWINPUTDEVICE Rid[1];

	/*
	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = RIDEV_NOLEGACY;   // adds HID mouse and also ignores legacy mouse messages
	Rid[0].hwndTarget = 0;
	*/

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x06;
	Rid[0].dwFlags = 0; // RIDEV_NOLEGACY;   // adds HID keyboard and also ignores legacy keyboard messages
	Rid[0].hwndTarget = 0;

	CHECKB(RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])));

#if 0
	IDirectInput8* inputRaw;
	IDirectInputDevice8* kbRaw;
	IDirectInputDevice8* mouseRaw;
	HRESULT err;
	DIPROPDWORD bufferSize;
	DIRECTINPUT8CREATE DirectInput8Create;
	int i;

	if(!di_library)
	{
		di_library = (HMODULE)LoadLibrary("dinput8.dll");
		if(!di_library) goto fail;
	}

	DirectInput8Create = (DIRECTINPUT8CREATE)GetProcAddress(di_library, "DirectInput8Create");
	if(!DirectInput8Create) goto fail;

	CHECKDI(DirectInput8Create(GetModuleHandle(0), DIRECTINPUT_VERSION,
			&IID_IDirectInput8, (void**)(&inputRaw), 0));
	self->input = inputRaw;

	// Prepare property struct for setting the amount of data to buffer.

	bufferSize.diph.dwSize = sizeof(DIPROPDWORD);
	bufferSize.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	bufferSize.diph.dwHow = DIPH_DEVICE;
	bufferSize.diph.dwObj = 0;
	bufferSize.dwData = INPUT_BUFFER_SIZE;

	// Set up the system keyboard.

	for (i = 0; i < 256; i++)
	{
		c_rgodfDIKeyboard[i].pguid = &GUID_Key;
		c_rgodfDIKeyboard[i].dwOfs = i;
		c_rgodfDIKeyboard[i].dwType = 0x8000000C | (i << 8);
		c_rgodfDIKeyboard[i].dwFlags = 0;
	}


	CHECKDI(IDirectInput_CreateDevice(inputRaw, &GUID_SysKeyboard, &kbRaw, 0));
	self->keyboard = kbRaw;

	CHECKDI(IDirectInputDevice_SetDataFormat(kbRaw, &c_dfDIKeyboard));
	CHECKDI(IDirectInputDevice_SetCooperativeLevel(kbRaw, self->hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE));
	CHECKDI(IDirectInputDevice_SetProperty(kbRaw, DIPROP_BUFFERSIZE, &bufferSize.diph));

	IDirectInputDevice_Acquire(kbRaw);
		
	// Set up the system mouse.

	CHECKDI(IDirectInput_CreateDevice(inputRaw, &GUID_SysMouse, &mouseRaw, 0));
	self->mouse = mouseRaw;

	CHECKDI(IDirectInputDevice_SetDataFormat(mouseRaw, &c_dfDIMouse));
	CHECKDI(IDirectInputDevice_SetCooperativeLevel(mouseRaw, self->hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE));
	CHECKDI(IDirectInputDevice_SetProperty(mouseRaw, DIPROP_BUFFERSIZE, &bufferSize.diph));

	IDirectInputDevice_Acquire(mouseRaw);
#endif

	this->swapMouse = GetSystemMetrics(SM_SWAPBUTTON) != 0;
	return 0;
fail:
	return -1;
}

static void update_mouse_pos(win32_window* self) {
	POINT pos;
	if(!GetCursorPos(&pos) || !ScreenToClient(self->hwnd, &pos))
		return;

	if(pos.x < 0) pos.x = 0;
	if(pos.y < 0) pos.y = 0;
	if(pos.x >= (LONG)self->width) pos.x = self->width;
	if(pos.y >= (LONG)self->height) pos.y = self->height;

	self->mouseX = (int)pos.x;
	self->mouseY = (int)pos.y;
}

static void update_win32_keys(win32_window* self) {
	BYTE keystate[256];
	if (GetKeyboardState(keystate)) {
		for (int i = 0; i < 256; ++i) {
			int key = gfx_native_to_keys[i];
			if (key) {
				set_button(self, key, keystate[i], 0, 1);
			}
		}
	}
}

static void update_di_buttons(win32_window* self, int collect_events) {

#if 0
	DIDEVICEOBJECTDATA data[INPUT_BUFFER_SIZE];
	DWORD inOut;
	HRESULT hr;

	inOut = INPUT_BUFFER_SIZE;
	hr = IDirectInputDevice_GetDeviceData(self->mouse, sizeof(data[0]), data, &inOut, 0);
	switch(hr)
	{
		case DI_OK:
		case DI_BUFFEROVERFLOW:
		{
			unsigned i;
			for (i = 0; i < inOut; ++i)
			{
				int down = (data[i].dwData & 0x80);
				DWORD button = data[i].dwOfs - DIMOFS_BUTTON0;

				if (button < 3)
				{
					force_button((gfx_common_window*)self, button, down, 0, collect_events);
				}
				else if (data[i].dwOfs == DIMOFS_Z)
				{
					if (!collect_events || data[i].dwData == 0)
						break;

					/* TODO:
					EButton button;
					button.id = int(data[i].dwData) < 0 ? msWheelDown : msWheelUp;
					button.state = true;
					button.character = 0;
					parent->onButton(button);
					*/
				}
			}
			break;
				
		}

		case DIERR_NOTACQUIRED:
		case DIERR_INPUTLOST:
		{
			unsigned id;
			// Cannot fetch new events: Release all buttons.

			for (id = msRangeBegin; id < msRangeEnd; ++id)
				set_button((gfx_common_window*)self, id, 0, 0, collect_events);

			IDirectInputDevice_Acquire(self->mouse);
			break;
		}
	}

	inOut = INPUT_BUFFER_SIZE;
	hr = IDirectInputDevice_GetDeviceData(self->keyboard, sizeof(data[0]), data, &inOut, 0);
	switch (hr)
	{
		case DI_OK:
		case DI_BUFFEROVERFLOW:
		{
			unsigned i;
			for (i = 0; i < inOut; ++i)
			{
				int down = (data[i].dwData & 0x80);

				uint32_t character = 0;
				if(down)
				{
					// TODO: character = unicodeChar(data[i].dwOfs);
				}
				force_button((gfx_common_window*)self, gfx_native_to_keys[data[i].dwOfs], down, character, collect_events);
			}
			break;
		}

		case DIERR_NOTACQUIRED:
		case DIERR_INPUTLOST:
		{
			unsigned id;
			for (id = kbRangeBegin; id < kbRangeEnd; ++id)
				set_button((gfx_common_window*)self, id, 0, 0, collect_events);
			IDirectInputDevice_Acquire(self->keyboard);
			break;
		}
	}
#endif
}

static HGLRC create_context(int pf, HDC hdc, PIXELFORMATDESCRIPTOR* pfd) {
	HGLRC dummyHrc;
	if(!SetPixelFormat(hdc, pf, pfd)) return NULL;

	dummyHrc = wglCreateContext(hdc);
	if(!dummyHrc) return NULL;
	if(!wglMakeCurrent(hdc, dummyHrc)) return NULL;
	return dummyHrc;
}

static int create_window(win32_window* self, int dummy, int fullscreen) {
	DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	DWORD styleEx = WS_EX_APPWINDOW;
	HDC hdc = NULL;
	HWND hwnd = NULL;
	HGLRC dummyHrc;
	PIXELFORMATDESCRIPTOR pfd;
	int pf = 0;
	int chosenFsaa = 0;
		
	if(fullscreen) {
		style |= WS_POPUP;
		styleEx |= WS_EX_TOPMOST;
	} else {
		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		styleEx |= WS_EX_WINDOWEDGE;
	}

	// Create window
	hwnd = CreateWindowEx(styleEx, window_class(), 0, style,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
		GetModuleHandle(0), 0);
	CHECKB(hwnd);

	hdc = GetDC(hwnd);
	CHECKB(hdc);

	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize        = sizeof(pfd);
	pfd.nVersion     = 1;
	pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iLayerType   = PFD_MAIN_PLANE;
	pfd.iPixelType   = PFD_TYPE_RGBA;
	if(dummy) {
		pfd.cColorBits = 16;
	} else {
		pfd.cColorBits = 32;
		pfd.cStencilBits = 8;
	}

	if (dummy) {
		pf = ChoosePixelFormat(hdc, &pfd);
		CHECKB(pf);
		dummyHrc = create_context(pf, hdc, &pfd);
		CHECKB(dummyHrc);

		init_extensions();
	} else if(self->fsaa > 1) {
		// Real finds closest valid FSAA level
		uint32_t allowed = self->fsaaLevels >> self->fsaa;

		if(allowed)
			chosenFsaa = tl_bottom_bit(allowed) + self->fsaa;
		else
			chosenFsaa = tl_log2(self->fsaaLevels);
	}

	if((chosenFsaa > 1 || dummy) && WGL_ARB_pixel_format && WGL_ARB_multisample) {
		// Dummy finds out valid FSAA levels.
		// Real tries to select pixel format with wglChoosePixelFormatARB.
		int formats[256];
		UINT count = 0;

		int iattr[] = {
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
			WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
			WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
			WGL_COLOR_BITS_ARB, 24,
			WGL_ALPHA_BITS_ARB, 8,
			WGL_DEPTH_BITS_ARB, 24,
			WGL_STENCIL_BITS_ARB, 8,
			WGL_SAMPLES_ARB, dummy ? 2 : chosenFsaa, // 2 for dummy
			0
		};

		CHECKB(wglChoosePixelFormatARB(hdc, iattr, NULL, 256, formats, &count));

		if(dummy) {
			int query = WGL_SAMPLES_ARB;
			unsigned int i;
			for (i = 0; i < count; ++i) {
				int samples;
				if (wglGetPixelFormatAttribivARB(hdc, formats[i], 0, 1, &query, &samples))
					self->fsaaLevels |= (1<<samples);
			}
		} else {
			// Pick best
			CHECKB(count > 0);
			pf = formats[0];
		}
	}

	if(!dummy) {
		if(!pf) {
			// If wglChoosePixelFormatARB failed or not attempted
			self->fsaa = 0;
			pf = ChoosePixelFormat(hdc, &pfd);
		}
		CHECKB(pf);
		CHECKB(create_context(pf, hdc, &pfd)); // TODO: Save hrc when necessary

		common_setup_gl((common_window*)self);

		SetLastError(0);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
	} else {
		// It's over for dummy
		wglMakeCurrent(0, 0);
		wglDeleteContext(dummyHrc);

		DestroyWindow(hwnd);
		return 0;
	}

	self->hdc = hdc;
	self->hwnd = hwnd;

	
	{ // Place window
		// Determine the size the window needs to have.
		RECT rc;
		unsigned windowW, windowH;
		int windowX = 0;
		int windowY = 0;

		rc.left = 0;
		rc.top = 0;
		rc.right = self->width;
		rc.bottom = self->height;
		AdjustWindowRectEx(&rc, style, FALSE, styleEx);
		windowW = rc.right - rc.left;
		windowH = rc.bottom - rc.top;

		if(!fullscreen) {
			// Center the window.
			HWND desktopWindow = GetDesktopWindow();
			RECT desktopRect;
			int desktopW, desktopH;

			GetClientRect(desktopWindow, &desktopRect);
			desktopW = desktopRect.right - desktopRect.left;
			desktopH = desktopRect.bottom - desktopRect.top;
			windowX = (desktopW - windowW) / 2;
			windowY = (desktopH - windowH) / 2;
		}

		MoveWindow(self->hwnd, windowX, windowY, windowW, windowH, 0);

		CHECK(self->setup_input());
	}
	return 0;

fail:
	if(hwnd) DestroyWindow(hwnd);
	return -1;
}

int common_window::set_mode(u32 width, u32 height, int fullscreen) {
	win32_window* self = static_cast<win32_window*>(this);
	int err;

	self->width = width;
	self->height = height;
	self->fullscreen = fullscreen;

	if(self->fsaa > 1) // Only construct dummy if we need to
		CHECKR(create_window(self, 1, fullscreen));
	CHECKR(create_window(self, 0, fullscreen));
	
	return 0;

fail:
	return err;
}

int common_window::set_visible(int state) {
	win32_window* self = static_cast<win32_window*>(this);

	if(IsWindowVisible(self->hwnd) == state)
		return 0; // Already in the right state
		
	if(state) {
		self->on_focus();
	} else {
		self->on_unfocus();
	}
		
	ShowWindow(self->hwnd, state ? SW_SHOW : SW_HIDE);

	{
		DEVMODE devMode;
		if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
			self->refresh_rate = devMode.dmDisplayFrequency;
			self->min_swap_interval = TICKS_FROM_MS(1000ull) / devMode.dmDisplayFrequency;
		}
	}

	return 0;
}

int common_window::update() {
	auto* self = static_cast<win32_window*>(this);

	process_messages();
	update_mouse_pos(self);
	update_win32_keys(self);
#if 0
	update_di_buttons(self, 1);
#endif

	return !self->closed;
}

int common_window::end_drawing() {
	auto* self = static_cast<win32_window*>(this);

	if (self->min_swap_interval < TICKS_FROM_MS(18)
	 && self->swaps * TICKS_FROM_MS(1) <= self->swap_delay) {
		// Seems we get delay from SwapBuffers
		u64 now = tl_get_ticks();

		u64 threshold = TICKS_FROM_MS(10ull);

		if ((now - self->swap_end) <= self->min_swap_interval - threshold) {
			Sleep(1);
			if ((now - self->swap_end) <= self->min_swap_interval - threshold) {
				return 0;
			}
		}
	}

	TL_TIME(swap_delay, {
		glFinish();
		SwapBuffers(self->hdc);
	});
	
	self->prev_swap_end = self->swap_end;
	self->swap_end = tl_get_ticks();

	self->swap_delay += swap_delay;
	++self->swaps;

	//printf("Frame: %lld\n", (self->swap_end - self->prev_swap_end));

	if (self->prev_swap_end && self->min_swap_interval == 0) {
		self->frame_intervals[self->frame_interval_n++ % 4] = self->swap_end - self->prev_swap_end;

		if (self->frame_interval_n >= 4) {
			u64 max_in_window = max(max(self->frame_intervals[0], self->frame_intervals[1]), max(self->frame_intervals[2], self->frame_intervals[3]));
			u64 avg_in_window = (self->frame_intervals[0] + self->frame_intervals[1] + self->frame_intervals[2] + self->frame_intervals[3]) / 4;

			//printf("Frame: %lld\n", max_in_window);

			if (self->min_swap_interval > max_in_window) {
				self->min_swap_interval = max_in_window;
				printf("M: %lld\n", self->min_swap_interval);
				printf("%lld\n", self->frame_intervals[0]);
				printf("%lld\n", self->frame_intervals[1]);
				printf("%lld\n", self->frame_intervals[2]);
				printf("%lld\n", self->frame_intervals[3]);
			}
		}

	}

	u64 prev_second = self->prev_swap_end / 10000000;
	u64 cur_second = self->swap_end / 10000000;

	if (prev_second != cur_second) {
		printf("fps: %d. swap block: %f ms\n", (int)self->swaps, (self->swap_delay / 10000.0) / self->swaps);
		//printf("prev frame: %f ms\n", (self->swap_end - self->prev_swap_end) / 10000.0);
		printf("draw delay: %f ms\n", ((self->draw_delay) / 10000.0) / self->swaps);

		self->swaps = 0;
		self->swap_delay = 0;
		self->draw_delay = 0;
	}

	return 1;
}

}
