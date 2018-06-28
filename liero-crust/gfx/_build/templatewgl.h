@include src/wglew_head.h

@expand EXT_USED_WGL #define $NAME$ (_glewUsed[$INDEX$])

#define BOOL tl::win::BOOL
#define HDC tl::win::HDC
#define FLOAT tl::win::FLOAT
#define UINT tl::win::UINT

@expand EXT_FUNC_USED_WGL typedef $WGL_TYPEDECL$;

#undef BOOL
#undef HDC
#undef FLOAT
#undef UINT

@expand EXT_FUNC_USED_WGL #define $NAME$ (($TYPENAME$)_glewFuncTable[$INDEX$])


@expand EXT_TOKEN_USED_WGL #define $NAME$ $VALUE$

typedef const char* (TL_WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (tl::win::HDC hdc);
typedef const char* (TL_WINAPI * PFNWGLGETEXTENSIONSSTRINGEXTPROC) (void);

@include src/wglew_tail.h
