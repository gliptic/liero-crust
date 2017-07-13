/* ------------------------------------------------------------------------- */

/* error codes */
#define GLEW_OK 0
#define GLEW_NO_ERROR 0
#define GLEW_ERROR_NO_GL_VERSION 1  /* missing GL version */
#define GLEW_ERROR_GL_VERSION_10_ONLY 2  /* GL 1.1 and up are not supported */
#define GLEW_ERROR_GLX_VERSION_11_ONLY 3  /* GLX 1.2 and up are not supported */

/* string codes */
#define GLEW_VERSION 1
#define GLEW_VERSION_MAJOR 2
#define GLEW_VERSION_MINOR 3
#define GLEW_VERSION_MICRO 4

GLEWAPI GLenum glewInit ();

GLEWAPI GLboolean glewExperimental;
GLEWAPI GLboolean glewGetExtension (const char* name);
GLEWAPI const GLubyte* glewGetErrorString (GLenum error);
GLEWAPI const GLubyte* glewGetString (GLenum name);

#ifdef __cplusplus
}
#endif

#ifdef GLEW_APIENTRY_DEFINED
#undef GLEW_APIENTRY_DEFINED
#undef APIENTRY
#undef GLAPIENTRY
#define GLAPIENTRY
#endif

#ifdef GLEW_CALLBACK_DEFINED
#undef GLEW_CALLBACK_DEFINED
#undef CALLBACK
#endif

#ifdef GLEW_WINGDIAPI_DEFINED
#undef GLEW_WINGDIAPI_DEFINED
#undef WINGDIAPI
#endif

#undef GLAPI
/* #undef GLEWAPI */

#endif /* __glew_h__ */
