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
