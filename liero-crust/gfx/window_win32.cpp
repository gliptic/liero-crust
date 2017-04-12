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
	if(this->hdc) ReleaseDC((HWND)this->hwnd, (HDC)this->hdc);
	if(this->hwnd) DestroyWindow((HWND)this->hwnd);
}

// Mode guessing experimentally adapted from GLFW library.
// http://glfw.sourceforge.net/

static int find_closest_video_mode(int *w, int *h, int *bpp, int *refresh) {

	int mode, bestmode, match, bestmatch, bestrr;
	BOOL success;
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

static void on_focus(CommonWindow& self) {
	int w = self.width, h = self.height, bpp = self.bpp, rr = self.refresh_rate;

	if (self.fullscreen && set_video_mode(find_closest_video_mode(&w, &h, &bpp, &rr)) == 0) {
		self.refresh_rate = rr;
		self.bpp = bpp;
	}
}

static void on_unfocus(CommonWindow& self) {
	if (self.fullscreen) {
		ChangeDisplaySettings(NULL, CDS_FULLSCREEN);
	}
}

static LRESULT CALLBACK window_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam) {
	LONG_PTR lptr = GetWindowLongPtr(wnd, GWLP_USERDATA);

	if (lptr) {
		CommonWindow* self = (CommonWindow *)lptr;
	
		switch (message) {
			case WM_SETFOCUS:
				if(self->fullscreen && IsWindowVisible(wnd)) {

					if (self->iconified) {
						OpenIcon(wnd);
						
						on_focus(*self);
						self->iconified = false;
					}
					return 0;
				}
				break;

			case WM_KILLFOCUS:
				if (self->fullscreen && IsWindowVisible(wnd)) {

					if (!self->iconified) {
						on_unfocus(*self);
						CloseWindow(wnd);
						self->iconified = true;
					}
					return 0;
				}
				break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:

				if(wparam == VK_F4 && (lparam & (1<<29)))
					close(*self);

				//printf("%d %x %x\n", message, wparam, lparam);
				return 0;
	
			case WM_SETCURSOR:
				if (LOWORD(lparam) == HTCLIENT) {
					SetCursor(0);
					return TRUE;
				}
				break;

			case WM_CLOSE:
				close(*self);
				return 0;

			case WM_INPUT: {
				UINT dwSize;

				GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

				if (dwSize > 0) {
					//void* rip = alloca(dwSize);
					u32 buf[256];
					void* rip = buf;

					if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, rip, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
#if 0
						printf("Wrong size\n");
#endif
					}

					RAWINPUT* ri = static_cast<RAWINPUT*>(rip);
					
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

			case WM_PAINT: {
				int swap = 0;

				if (swap) {
					SwapBuffers((HDC)self->hdc);
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
	wc.lpszClassName = "m";
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = window_proc;
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

static int setup_input(CommonWindow& self) {
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

	self.swapMouse = GetSystemMetrics(SM_SWAPBUTTON) != 0;
	return 0;
fail:
	return -1;
}

static void update_mouse_pos(CommonWindow* self) {
	POINT pos;
	if(!GetCursorPos(&pos) || !ScreenToClient((HWND)self->hwnd, &pos))
		return;

	if(pos.x < 0) pos.x = 0;
	if(pos.y < 0) pos.y = 0;
	if(pos.x >= (LONG)self->width) pos.x = self->width;
	if(pos.y >= (LONG)self->height) pos.y = self->height;

	self->mouseX = (int)pos.x;
	self->mouseY = (int)pos.y;
}

static void update_win32_keys(CommonWindow* self) {
	BYTE keystate[256];
	if (GetKeyboardState(keystate)) {
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

static HGLRC create_context(int pf, HDC hdc, PIXELFORMATDESCRIPTOR* pfd) {
	HGLRC dummyHrc;
	if(!SetPixelFormat(hdc, pf, pfd)) return NULL;

	dummyHrc = wglCreateContext(hdc);
	if(!dummyHrc) return NULL;
	if(!wglMakeCurrent(hdc, dummyHrc)) return NULL;
	return dummyHrc;
}

static int create_window(CommonWindow* self, int dummy, int fullscreen) {
	DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	DWORD styleEx = WS_EX_APPWINDOW;
	HDC hdc = NULL;
	HWND hwnd = NULL;
	HGLRC dummyHrc = NULL;
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

		MoveWindow((HWND)self->hwnd, windowX, windowY, windowW, windowH, 0);

		CHECK(setup_input(*self));
	}
	return 0;

fail:
	if(hwnd) DestroyWindow(hwnd);
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

	if(!!IsWindowVisible((HWND)this->hwnd) == state)
		return 0; // Already in the right state
		
	if (state) {
		on_focus(*this);
	} else {
		on_unfocus(*this);
	}
		
	ShowWindow((HWND)this->hwnd, state ? SW_SHOW : SW_HIDE);


	{
		DEVMODE devMode;
		if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
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

	SwapBuffers((HDC)this->hdc);
	
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
