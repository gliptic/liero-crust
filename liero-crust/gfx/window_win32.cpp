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

#define GFX_SUPPORT_FSAA 1

namespace gfx {

#define TICKS_FROM_MS(ms) ((ms) * 10000)
#define TICKS_IN_SECOND (10000000)

#define INPUT_BUFFER_SIZE (32)

#define CHECK(r) do { if(r) goto fail; } while(0)
#define CHECKB(r) do { if(!(r)) goto fail; } while(0)
#define CHECKR(r) do { err = (r); if(err) goto fail; } while(0)

}

#include "window_common.hpp"

namespace gfx {

static void close(CommonWindow& self) {
	self.set_visible(0);
	self.closed = true;
}

CommonWindow::~CommonWindow() {
	close(*this);
	if(this->hdc) tl::win::ReleaseDC((tl::win::HWND)this->hwnd, (tl::win::HDC)this->hdc);
	if(this->hwnd) tl::win::DestroyWindow((tl::win::HWND)this->hwnd);
}

// Mode guessing experimentally adapted from GLFW library.
// http://glfw.sourceforge.net/

static int find_closest_video_mode(int *w, int *h, int *bpp, int *refresh) {

	int mode, bestmode, match, bestmatch, bestrr;
	tl::win::BOOL success;
	tl::win::DEVMODEA dm;

	// Find best match
	bestmatch = 0x7fffffff;
	bestrr = 0x7fffffff;
	mode = bestmode = 0;
	do {
		dm.dmSize = sizeof(dm);
		success = EnumDisplaySettingsA(NULL, mode, &dm);
		if (success) {
			match = dm.dmBitsPerPel - *bpp;
			if (match < 0) match = -match;
			match = (match << 25) |
					((dm.dmPelsWidth - *w) * (dm.dmPelsWidth - *w) +
						(dm.dmPelsHeight - *h) * (dm.dmPelsHeight - *h));

			int rr = (dm.dmDisplayFrequency - *refresh) * (dm.dmDisplayFrequency - *refresh);
			if ((match < bestmatch) || (match == bestmatch && *refresh > 0 && rr < bestrr)) {
				bestmatch = match;
				bestmode = mode;
				bestrr = rr;
				*w = dm.dmPelsWidth;
				*h = dm.dmPelsHeight;
				*bpp = dm.dmBitsPerPel;
				*refresh = dm.dmDisplayFrequency;
			}
		}
		++mode;
	} while (success);

	return bestmode;
}

static int set_video_mode(int mode) {

	// Get the parameters
	tl::win::DEVMODEA dm;
	dm.dmSize = sizeof(dm);
	tl::win::EnumDisplaySettingsA(NULL, mode, &dm);

	// Set which fields we want to specify
	dm.dmFields = tl::win::DM_PELSWIDTH | tl::win::DM_PELSHEIGHT | tl::win::DM_BITSPERPEL;

	// Change display setting
	dm.dmSize = sizeof(dm);
	if (ChangeDisplaySettingsA(&dm, tl::win::CDS_FULLSCREEN) != tl::win::DISP_CHANGE_SUCCESSFUL)
		return -1;
	return 0;
}

static void on_focus(CommonWindow& self) {
	int w = self.width, h = self.height, bpp = self.bpp, rr = self.refresh_rate;

	if (self.fullscreen && set_video_mode(find_closest_video_mode(&w, &h, &bpp, &rr)) == 0) {
		self.refresh_rate = rr;
		self.bpp = bpp;
	}
}

static void on_unfocus(CommonWindow& self) {
	if (self.fullscreen) {
		tl::win::ChangeDisplaySettingsA(NULL, tl::win::CDS_FULLSCREEN);
	}
}

static tl::win::LRESULT TL_STDCALL window_proc(tl::win::HWND wnd, tl::win::UINT message, tl::win::WPARAM wparam, tl::win::LPARAM lparam) {
	tl::win::LONG_PTR lptr = tl::win::GetWindowLongPtrA(wnd, tl::win::GWLP_USERDATA);

	if (lptr) {
		CommonWindow* self = (CommonWindow *)lptr;
	
		switch (message) {
			case tl::win::WM_SETFOCUS:
				if(self->fullscreen && tl::win::IsWindowVisible(wnd)) {

					if (self->iconified) {
						tl::win::OpenIcon(wnd);
						
						on_focus(*self);
						self->iconified = false;
					}
					return 0;
				}
				break;

			case tl::win::WM_KILLFOCUS:
				if (self->fullscreen && tl::win::IsWindowVisible(wnd)) {

					if (!self->iconified) {
						on_unfocus(*self);
						tl::win::CloseWindow(wnd);
						self->iconified = true;
					}
					return 0;
				}
				break;

			case tl::win::WM_SYSKEYDOWN:
			case tl::win::WM_SYSKEYUP:

				if(wparam == tl::win::VK_F4 && (lparam & (1<<29)))
					close(*self);

				//printf("%d %x %x\n", message, wparam, lparam);
				return 0;
	
			case tl::win::WM_SETCURSOR:
				if (tl::win::LOWORD(lparam) == tl::win::HTCLIENT) {
					tl::win::SetCursor(0);
					return 1;
				}
				break;

			case tl::win::WM_CLOSE:
				close(*self);
				return 0;

			case tl::win::WM_INPUT: {
				tl::win::UINT dwSize;

				tl::win::GetRawInputData((tl::win::HRAWINPUT)lparam, tl::win::RID_INPUT, NULL, &dwSize, sizeof(tl::win::RAWINPUTHEADER));

				if (dwSize > 0) {
					u32 buf[256];
					void* rip = buf;

					if (tl::win::GetRawInputData((tl::win::HRAWINPUT)lparam, tl::win::RID_INPUT, rip, &dwSize, sizeof(tl::win::RAWINPUTHEADER)) != dwSize) {
#if 0
						printf("Wrong size\n");
#endif
					}

					tl::win::RAWINPUT* ri = static_cast<tl::win::RAWINPUT*>(rip);
					
#if 0
					printf("type = %d. VK = %x. EI = %d. F = %d. M = %d. MC = %d.\n",
						ri->header.dwType, ri->data.keyboard.VKey, ri->data.keyboard.ExtraInformation,
						ri->data.keyboard.Flags, ri->data.keyboard.Message, ri->data.keyboard.MakeCode);
#endif

					set_button(self, gfx_native_to_keys[ri->data.keyboard.VKey], (ri->data.keyboard.Flags & 1) ^ 1, 0, true);
				}

				// Let DefWindowProc clean up
				break;
			}

			case tl::win::WM_PAINT: {
				int swap = 0;

				if (swap) {
					tl::win::SwapBuffers((tl::win::HDC)self->hdc);
				}

				tl::win::ValidateRect(wnd, 0);
				return 0;
			}
		}
	}

	return tl::win::DefWindowProcA(wnd, message, wparam, lparam);
}

static tl::win::LPCSTR window_class() {
	tl::win::WNDCLASSA wc;
	static tl::win::LPCSTR name = 0;
	if (name)
		return name;

	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = "m";
	wc.style = tl::win::CS_OWNDC;
	wc.lpfnWndProc = window_proc;
	//wc.cbClsExtra = 0;
	//wc.cbWndExtra = 0;
	wc.hInstance = tl::win::GetModuleHandleA(0);
	wc.hIcon = tl::win::LoadIconA(wc.hInstance, MAKEINTRESOURCEA(101));
	//wc.hCursor = 0;
	//wc.hbrBackground = 0;
	//wc.lpszMenuName = 0;
		
	name = (tl::win::LPCSTR)tl::win::RegisterClassA(&wc);
	//check(name, "registering a window class");
	return name;
}

static void process_messages() {

	tl::win::MSG message;

	while (tl::win::PeekMessageA(&message, 0, 0, 0, tl::win::PM_REMOVE)) {
		//if (!handledByHook(message))
		{
			tl::win::TranslateMessage(&message);
			tl::win::DispatchMessageA(&message);
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

static int setup_input(CommonWindow& self) {
	tl::win::RAWINPUTDEVICE Rid[1];

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

	self.swapMouse = tl::win::GetSystemMetrics(tl::win::SM_SWAPBUTTON) != 0;
	return 0;
fail:
	return -1;
}

static void update_mouse_pos(CommonWindow* self) {
	tl::win::POINT pos;
	if(!tl::win::GetCursorPos(&pos) || !tl::win::ScreenToClient((tl::win::HWND)self->hwnd, &pos))
		return;

	if(pos.x < 0) pos.x = 0;
	if(pos.y < 0) pos.y = 0;
	if(pos.x >= (tl::win::LONG)self->width) pos.x = self->width;
	if(pos.y >= (tl::win::LONG)self->height) pos.y = self->height;

	self->mouseX = (int)pos.x;
	self->mouseY = (int)pos.y;
}

static void update_win32_keys(CommonWindow* self) {
	tl::win::BYTE keystate[256];
	if (tl::win::GetKeyboardState(keystate)) {
		for (int i = 0; i < 256; ++i) {
			int key = gfx_native_to_keys[i];
			if (key) {
				set_button(self, key, (keystate[i] >> 7) & 1, 0, true);
			}
		}
	}
}

#if 0
static void update_di_buttons(CommonWindow* self, int collect_events) {

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
}
#endif

static tl::win::HGLRC create_context(int pf, tl::win::HDC hdc, tl::win::PIXELFORMATDESCRIPTOR* pfd) {
	tl::win::HGLRC dummyHrc;
	if(!tl::win::SetPixelFormat(hdc, pf, pfd)) return NULL;

	dummyHrc = tl::win::wglCreateContext(hdc);
	if(!dummyHrc) return NULL;
	if(!wglMakeCurrent(hdc, dummyHrc)) return NULL;
	return dummyHrc;
}

static int create_window(CommonWindow* self, int dummy, int fullscreen) {
	tl::win::DWORD style = tl::win::WS_CLIPSIBLINGS | tl::win::WS_CLIPCHILDREN;
	tl::win::DWORD styleEx = tl::win::WS_EX_APPWINDOW;
	tl::win::HDC hdc = NULL;
	tl::win::HWND hwnd = NULL;
	tl::win::HGLRC dummyHrc = NULL;
	tl::win::PIXELFORMATDESCRIPTOR pfd;
	int pf = 0;
	int chosenFsaa = 0;
		
	if(fullscreen) {
		style |= tl::win::WS_POPUP;
		styleEx |= tl::win::WS_EX_TOPMOST;
	} else {
		style |= tl::win::WS_CAPTION | tl::win::WS_SYSMENU | tl::win::WS_MINIMIZEBOX;
		styleEx |= tl::win::WS_EX_WINDOWEDGE;
	}

	// Create window
	hwnd = tl::win::CreateWindowExA(styleEx, window_class(), 0, style,
		tl::win::CW_USEDEFAULT, tl::win::CW_USEDEFAULT, tl::win::CW_USEDEFAULT, tl::win::CW_USEDEFAULT, 0, 0,
		tl::win::GetModuleHandleA(0), 0);
	CHECKB(hwnd);

	hdc = tl::win::GetDC(hwnd);
	CHECKB(hdc);

	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize        = sizeof(pfd);
	pfd.nVersion     = 1;
	pfd.dwFlags      = tl::win::PFD_DRAW_TO_WINDOW | tl::win::PFD_SUPPORT_OPENGL | tl::win::PFD_DOUBLEBUFFER;
	pfd.iLayerType   = tl::win::PFD_MAIN_PLANE;
	pfd.iPixelType   = tl::win::PFD_TYPE_RGBA;
	if(dummy) {
		pfd.cColorBits = 16;
	} else {
		pfd.cColorBits = 32;
		pfd.cStencilBits = 8;
	}

	if (dummy) {
		pf = ChoosePixelFormat(hdc, &pfd);
		CHECKB(pf);
		printf("dummy: %u", tl::timer([&] {
			dummyHrc = create_context(pf, hdc, &pfd);
		}));
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
		tl::win::UINT count = 0;

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

		CHECKB(wglChoosePixelFormatARB(hdc, iattr, NULL, dummy ? 256 : 1, formats, &count));

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

		common_setup_gl((CommonWindow*)self);

		tl::win::SetLastError(0);
		tl::win::SetWindowLongPtrA(hwnd, tl::win::GWLP_USERDATA, (tl::win::LONG_PTR)self);
	} else {
		// It's over for dummy
		tl::win::wglMakeCurrent(0, 0);
		tl::win::wglDeleteContext(dummyHrc);

		tl::win::DestroyWindow(hwnd);
		return 0;
	}

	self->hdc = hdc;
	self->hwnd = hwnd;

	
	{ // Place window
		// Determine the size the window needs to have.
		tl::win::RECT rc;
		unsigned windowW, windowH;
		int windowX = 0;
		int windowY = 0;

		rc.left = 0;
		rc.top = 0;
		rc.right = self->width;
		rc.bottom = self->height;
		tl::win::AdjustWindowRectEx(&rc, style, 0, styleEx);
		windowW = rc.right - rc.left;
		windowH = rc.bottom - rc.top;

		if(!fullscreen) {
			// Center the window.
			tl::win::HWND desktopWindow = tl::win::GetDesktopWindow();
			tl::win::RECT desktopRect;
			int desktopW, desktopH;

			tl::win::GetClientRect(desktopWindow, &desktopRect);
			desktopW = desktopRect.right - desktopRect.left;
			desktopH = desktopRect.bottom - desktopRect.top;
			windowX = (desktopW - windowW) / 2;
			windowY = (desktopH - windowH) / 2;
		}

		tl::win::MoveWindow((tl::win::HWND)self->hwnd, windowX, windowY, windowW, windowH, 0);

		CHECK(setup_input(*self));
	}
	return 0;

fail:
	if(hwnd) tl::win::DestroyWindow(hwnd);
	return -1;
}

int CommonWindow::set_mode(u32 width_new, u32 height_new, bool fullscreen_new) {
	int err;

	this->width = width_new;
	this->height = height_new;
	this->fullscreen = fullscreen_new;

#if GFX_SUPPORT_FSAA
	if (this->fsaa > 1) // Only construct dummy if we need to
		CHECKR(create_window(this, 1, fullscreen));
#endif
	CHECKR(create_window(this, 0, fullscreen));
	
	return 0;

fail:
	return err;
}

int CommonWindow::set_visible(bool state) {

	if(!!tl::win::IsWindowVisible((tl::win::HWND)this->hwnd) == state)
		return 0; // Already in the right state
		
	if (state) {
		on_focus(*this);
	} else {
		on_unfocus(*this);
	}
		
	tl::win::ShowWindow((tl::win::HWND)this->hwnd, state ? tl::win::SW_SHOW : tl::win::SW_HIDE);


	{
		tl::win::DEVMODEA devMode;
		if (EnumDisplaySettingsA(NULL, tl::win::ENUM_CURRENT_SETTINGS, &devMode)) {
			this->refresh_rate = devMode.dmDisplayFrequency;
#if GFX_PREDICT_VSYNC
			this->min_swap_interval = TICKS_IN_SECOND / devMode.dmDisplayFrequency;
#endif
		}
	}

	return 0;
}

int CommonWindow::update() {

	process_messages();
	update_mouse_pos(this);
	update_win32_keys(this);

	return !this->closed;
}

u32 slept_ms = 0;

int CommonWindow::end_drawing() {

#if GFX_PREDICT_VSYNC
	if (this->min_swap_interval < TICKS_FROM_MS(18)
	 && this->swaps * TICKS_FROM_MS(1) <= this->swap_delay) {
		// Seems we get delay from SwapBuffers
		u64 now = tl_get_ticks();

		u64 threshold = TICKS_FROM_MS(5ull);

		if ((now - this->swap_end) <= this->min_swap_interval - threshold) {
			Sleep(1);
			++slept_ms;
			now = tl_get_ticks();
			if ((now - this->swap_end) <= this->min_swap_interval - threshold) {
				return 0;
			}
		}
	}
#endif

	tl::win::SwapBuffers((tl::win::HDC)this->hdc);
	
#if GFX_PREDICT_VSYNC
	this->prev_swap_end = this->swap_end;
	this->swap_end = tl_get_ticks();

	this->swap_delay += lswap_delay;
	++this->swaps;

	//printf("Frame: %lld\n", (this->swap_end - this->prev_swap_end));


	if (this->prev_swap_end && this->min_swap_interval == 0) {
		this->frame_intervals[this->frame_interval_n++ % 4] = tl::narrow<u32>(this->swap_end - this->prev_swap_end);

		if (this->frame_interval_n >= 4) {
			u32 max_in_window = tl::max(
				tl::max(this->frame_intervals[0], this->frame_intervals[1]),
				tl::max(this->frame_intervals[2], this->frame_intervals[3]));
			u32 avg_in_window = (this->frame_intervals[0] + this->frame_intervals[1] + this->frame_intervals[2] + this->frame_intervals[3]) / 4;

			TL_UNUSED(avg_in_window);

			//printf("Frame: %lld\n", max_in_window);
			if (this->min_swap_interval > max_in_window) {
				this->min_swap_interval = max_in_window;
#if 0
				printf("M: %lld\n", this->min_swap_interval);
				//printf("%lld\n", this->frame_intervals[0]);
				//printf("%lld\n", this->frame_intervals[1]);
				//printf("%lld\n", this->frame_intervals[2]);
				//printf("%lld\n", this->frame_intervals[3]);
#endif
			}
		}

	}

	u64 prev_second = this->prev_swap_end / 10000000;
	u64 cur_second = this->swap_end / 10000000;

	if (prev_second != cur_second) {
#if 0
		printf("fps: %d. swap block: %f ms\n", (int)this->swaps, (this->swap_delay / 10000.0) / this->swaps);
		//printf("prev frame: %f ms\n", (this->swap_end - this->prev_swap_end) / 10000.0);
		printf("draw delay: %f ms. slept: %d ms\n", ((this->draw_delay) / 10000.0) / this->swaps, slept_ms);
#endif

		this->swaps = 0;
		this->swap_delay = 0;
		this->draw_delay = 0;
	}

#endif

	return 1;
}

}
