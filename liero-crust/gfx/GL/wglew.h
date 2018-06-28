#ifndef __wglew_h__
#define __wglew_h__
#define __WGLEW_H__

#ifdef __wglext_h_
#error wglext.h included before wglew.h
#endif

#define __wglext_h_

//#if !defined(WINAPI)
//#  undef WIN32_LEAN_AND_MEAN
//#  define WIN32_LEAN_AND_MEAN 1
//#  undef NOMINMAX
//#  define NOMINMAX 1
//#  include <windows.h>
#  include "tl/windows/miniwindows.h"
//#  undef WIN32_LEAN_AND_MEAN
//#endif

/*
 * GLEW_STATIC needs to be set when using the static version.
 * GLEW_BUILD is set when building the DLL version.
 */
#ifdef GLEW_STATIC
#  define GLEWAPI extern
#else
#  ifdef GLEW_BUILD
#    define GLEWAPI extern __declspec(dllexport)
#  else
#    define GLEWAPI extern __declspec(dllimport)
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define WGL_ARB_multisample (_glewUsed[3])
#define WGL_ARB_pixel_format (_glewUsed[4])
#define WGL_EXT_swap_control (_glewUsed[5])

#define BOOL tl::win::BOOL
#define HDC tl::win::HDC
#define FLOAT tl::win::FLOAT
#define UINT tl::win::UINT

typedef BOOL (TL_WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);
typedef BOOL (TL_WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int* piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
typedef BOOL (TL_WINAPI *PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int* piAttributes, int *piValues);

#undef BOOL
#undef HDC
#undef FLOAT
#undef UINT

#define wglSwapIntervalEXT ((PFNWGLSWAPINTERVALEXTPROC)_glewFuncTable[29])
#define wglChoosePixelFormatARB ((PFNWGLCHOOSEPIXELFORMATARBPROC)_glewFuncTable[27])
#define wglGetPixelFormatAttribivARB ((PFNWGLGETPIXELFORMATATTRIBIVARBPROC)_glewFuncTable[28])


#define WGL_ACCUM_BLUE_BITS_ARB 0x2020
#define WGL_NEED_PALETTE_ARB 0x2004
#define WGL_BLUE_BITS_ARB 0x2019
#define WGL_SAMPLES_ARB 0x2042
#define WGL_GREEN_SHIFT_ARB 0x2018
#define WGL_RED_BITS_ARB 0x2015
#define WGL_ACCUM_GREEN_BITS_ARB 0x201F
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_STEREO_ARB 0x2012
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_SHARE_STENCIL_ARB 0x200D
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_AUX_BUFFERS_ARB 0x2024
#define WGL_SWAP_LAYER_BUFFERS_ARB 0x2006
#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_SWAP_METHOD_ARB 0x2007
#define WGL_RED_SHIFT_ARB 0x2016
#define WGL_SWAP_UNDEFINED_ARB 0x202A
#define WGL_NUMBER_UNDERLAYS_ARB 0x2009
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_ALPHA_BITS_ARB 0x201B
#define WGL_ALPHA_SHIFT_ARB 0x201C
#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_SHARE_ACCUM_ARB 0x200E
#define WGL_ACCUM_BITS_ARB 0x201D
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_ACCUM_RED_BITS_ARB 0x201E
#define WGL_TRANSPARENT_ARB 0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB 0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB 0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB 0x2039
#define WGL_TYPE_COLORINDEX_ARB 0x202C
#define WGL_ACCUM_ALPHA_BITS_ARB 0x2021
#define WGL_TRANSPARENT_INDEX_VALUE_ARB 0x203B
#define WGL_STENCIL_BITS_ARB 0x2023
#define WGL_NEED_SYSTEM_PALETTE_ARB 0x2005
#define WGL_SWAP_EXCHANGE_ARB 0x2028
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB 0x203A
#define WGL_SUPPORT_GDI_ARB 0x200F
#define WGL_BLUE_SHIFT_ARB 0x201A
#define WGL_DRAW_TO_BITMAP_ARB 0x2002
#define WGL_SWAP_COPY_ARB 0x2029
#define WGL_NUMBER_OVERLAYS_ARB 0x2008
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_GREEN_BITS_ARB 0x2017
#define WGL_SAMPLE_BUFFERS_ARB 0x2041
#define WGL_SHARE_DEPTH_ARB 0x200C
#define WGL_NO_ACCELERATION_ARB 0x2025
#define WGL_GENERIC_ACCELERATION_ARB 0x2026

typedef const char* (TL_WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (tl::win::HDC hdc);
typedef const char* (TL_WINAPI * PFNWGLGETEXTENSIONSSTRINGEXTPROC) (void);

/* ------------------------------------------------------------------------- */

#ifdef GLEW_MX

typedef struct WGLEWContextStruct WGLEWContext;
GLEWAPI GLenum wglewContextInit (WGLEWContext* ctx);
GLEWAPI GLboolean wglewContextIsSupported (WGLEWContext* ctx, const char* name);

#define wglewInit() wglewContextInit(wglewGetContext())
#define wglewIsSupported(x) wglewContextIsSupported(wglewGetContext(), x)

#define WGLEW_GET_VAR(x) (*(const GLboolean*)&(wglewGetContext()->x))
#define WGLEW_GET_FUN(x) wglewGetContext()->x

#else /* GLEW_MX */

#define WGLEW_GET_VAR(f, x) ((x) == 2 || (!(x) && f()))
#define WGLEW_GET_FUN(f, x) ((x) || f(), (x))

GLEWAPI GLboolean wglewIsSupported (const char* name);

#endif /* GLEW_MX */

GLEWAPI GLboolean wglewGetExtension (const char* name);

#ifdef __cplusplus
}
#endif

#undef GLEWAPI

#endif /* __wglew_h__ */

