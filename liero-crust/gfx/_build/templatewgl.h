@include src/wglew_head.h

@expand EXT_USED_WGL #define $NAME$ (_glewUsed[$INDEX$])

@expand EXT_FUNC_USED_WGL typedef $WGL_TYPEDECL$;

@expand EXT_FUNC_USED_WGL #define $NAME$ (($TYPENAME$)_glewFuncTable[$INDEX$])

@expand EXT_TOKEN_USED_WGL #define $NAME$ $VALUE$

typedef const char* (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
typedef const char* (WINAPI * PFNWGLGETEXTENSIONSSTRINGEXTPROC) (void);

@include src/wglew_tail.h
