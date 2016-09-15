#include "glew.h"
#if defined(_WIN32)
#  include "wglew.h"
#elif !defined(__APPLE__) || defined(GLEW_APPLE_GLX)
#  include "glxew.h"
#endif

/*
 * Define glewGetContext and related helper macros.
 */
#ifdef GLEW_MX
#  define glewGetContext() ctx
#  ifdef _WIN32
#    define GLEW_CONTEXT_ARG_DEF_INIT GLEWContext* ctx
#    define GLEW_CONTEXT_ARG_VAR_INIT ctx
#    define wglewGetContext() ctx
#    define WGLEW_CONTEXT_ARG_DEF_INIT WGLEWContext* ctx
#    define WGLEW_CONTEXT_ARG_DEF_LIST WGLEWContext* ctx
#  else /* _WIN32 */
#    define GLEW_CONTEXT_ARG_DEF_INIT void
#    define GLEW_CONTEXT_ARG_VAR_INIT
#    define glxewGetContext() ctx
#    define GLXEW_CONTEXT_ARG_DEF_INIT void
#    define GLXEW_CONTEXT_ARG_DEF_LIST GLXEWContext* ctx
#  endif /* _WIN32 */
#  define GLEW_CONTEXT_ARG_DEF_LIST GLEWContext* ctx
#else /* GLEW_MX */
#  define GLEW_CONTEXT_ARG_DEF_INIT void
#  define GLEW_CONTEXT_ARG_VAR_INIT
#  define GLEW_CONTEXT_ARG_DEF_LIST void
#  define WGLEW_CONTEXT_ARG_DEF_INIT void
#  define WGLEW_CONTEXT_ARG_DEF_LIST void
#  define GLXEW_CONTEXT_ARG_DEF_INIT void
#  define GLXEW_CONTEXT_ARG_DEF_LIST void
#endif /* GLEW_MX */

#if defined(__APPLE__)
#include <stdlib.h>
#include <string.h>
#include <AvailabilityMacros.h>

#ifdef MAC_OS_X_VERSION_10_3

#include <dlfcn.h>

void* NSGLGetProcAddress (const GLubyte *name)
{
  static void* image = NULL;
  if (NULL == image) 
  {
    image = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
  }
  return image ? dlsym(image, (const char*)name) : NULL;
}
#else

#include <mach-o/dyld.h>

void* NSGLGetProcAddress (const GLubyte *name)
{
  static const struct mach_header* image = NULL;
  NSSymbol symbol;
  char* symbolName;
  if (NULL == image)
  {
    image = NSAddImage("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", NSADDIMAGE_OPTION_RETURN_ON_ERROR);
  }
  /* prepend a '_' for the Unix C symbol mangling convention */
  symbolName = malloc(strlen((const char*)name) + 2);
  strcpy(symbolName+1, (const char*)name);
  symbolName[0] = '_';
  symbol = NULL;
  /* if (NSIsSymbolNameDefined(symbolName))
	 symbol = NSLookupAndBindSymbol(symbolName); */
  symbol = image ? NSLookupSymbolInImage(image, symbolName, NSLOOKUPSYMBOLINIMAGE_OPTION_BIND | NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR) : NULL;
  free(symbolName);
  return symbol ? NSAddressOfSymbol(symbol) : NULL;
}
#endif /* MAC_OS_X_VERSION_10_3 */
#endif /* __APPLE__ */

#if defined(__sgi) || defined (__sun)
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

void* dlGetProcAddress (const GLubyte* name)
{
  static void* h = NULL;
  static void* gpa;

  if (h == NULL)
  {
    if ((h = dlopen(NULL, RTLD_LAZY | RTLD_LOCAL)) == NULL) return NULL;
    gpa = dlsym(h, "glXGetProcAddress");
  }

  if (gpa != NULL)
    return ((void*(*)(const GLubyte*))gpa)(name);
  else
    return dlsym(h, (const char*)name);
}
#endif /* __sgi || __sun */

/*
 * Define glewGetProcAddress.
 */
#if defined(_WIN32)
#  define glewGetProcAddress(name) wglGetProcAddress((LPCSTR)name)
#else
#  if defined(__APPLE__)
#    define glewGetProcAddress(name) NSGLGetProcAddress(name)
#  else
#    if defined(__sgi) || defined(__sun)
#      define glewGetProcAddress(name) dlGetProcAddress(name)
#    else /* __linux */
#      define glewGetProcAddress(name) (*glXGetProcAddressARB)(name)
#    endif
#  endif
#endif

/*
 * Define GLboolean const cast.
 */
#define CONST_CAST(x) (*(GLboolean*)&x)

/*
 * GLEW, just like OpenGL or GLU, does not rely on the standard C library.
 * These functions implement the functionality required in this file.
 */

static GLuint _glewStrCLen (const GLubyte* s, GLubyte c)
{
  GLuint i=0;
  if (s == NULL) return 0;
  while (s[i] != '\0' && s[i] != c) i++;
  return (s[i] == '\0' || s[i] == c) ? i : 0;
}

static GLboolean _glewStrSame4 (const GLubyte* a, const GLubyte* b)
{
	GLubyte ac, bc;
	while(1)
	{
		ac = *a;
		bc = *b;
		if (ac == '\0' || ac == ' ' || bc == '\0' || ac == ' '
		 || ac != bc)
			break;
		++a;
		++b;
	}

	if (ac == ' ') ac = '\0';
	if (bc == ' ') bc = '\0';

	return ac == bc ? GL_TRUE : GL_FALSE;
}


#include <stdlib.h>
#include <string.h>

typedef struct _glewStringEntry_ {
	GLubyte const* str;
	int index;
} _glewStringEntry;

static int _glewAvailable[6];

static GLuint strhash(const GLubyte* str) {
	GLuint acc = 1;
	while(*str != ' ' && *str != '\0')
	{
		acc = 0x10100021*acc ^ *str++;
	}

	return (acc & 0xffffffff) >> 28;
}

static _glewStringEntry _glewStringTable[16];

static _glewStringEntry* _glewFindString(GLubyte const* str) {
	GLuint h = strhash(str);
	const GLubyte* comp;
	while ((comp = _glewStringTable[h].str) != 0) {
		if (_glewStrSame4(str, comp))
			return &_glewStringTable[h];
		h = (h + 1) & (16 - 1);
	}
	
	return NULL;
}

static void _glewAddString(GLubyte const* str, int index) {
	GLuint h;
	if (NULL != _glewFindString(str))
		return;

	h = strhash(str);
	while(_glewStringTable[h].str)
	{
		h = (h + 1) & (16 - 1);
	}

	_glewStringTable[h].str = str;
	_glewStringTable[h].index = index;
}

/* For adding used extension strings. The strings are mapped to indices according
 to their order. The index is used to reference the extension later on. */
static void _glewAddStrings(GLubyte const* p) {
	int index = 0;
	while (*p) {
		GLubyte const* begin = p;
		while(*p && *p != ' ')
			++p;
		_glewAddString(begin, index++);
		if(*p)
			++p; /* Skip space */
	}
}

/* Mark the extensions that appear in the supplied string */
static void _glewMarkAvailable(GLubyte const* p) {

	while (*p) {
		_glewStringEntry* entry;
		GLubyte const* begin = p;
		while(*p && *p != ' ')
			++p;

		entry = _glewFindString(begin);
		if(entry)
			_glewAvailable[entry->index] = 1;

		if(*p)
			++p; /* Skip space */
	}
}

char const* _glewUsed = "  GL_ARB_multisample WGL_ARB_multisample WGL_ARB_pixel_format WGL_EXT_swap_control";
void (*_glewFuncTable[30])();

static GLubyte _glewFuncUsedPerExt[6] = {
	1,26,0,0,2,1
};

static char const* __glewFuncUsedNames = "glActiveTexture\0glAttachShader\0glBindAttribLocation\0glCompileShader\0glCreateProgram\0glCreateShader\0glDeleteProgram\0glDeleteShader\0glDetachShader\0glDisableVertexAttribArray\0glEnableVertexAttribArray\0glGetProgramiv\0glGetProgramInfoLog\0glGetShaderiv\0glGetShaderInfoLog\0glShaderSource\0glGetUniformLocation\0glLinkProgram\0glUseProgram\0glUniform1f\0glUniform1i\0glUniform2f\0glUniformMatrix2fv\0glValidateProgram\0glVertexAttrib2f\0glVertexAttrib4f\0glVertexAttribPointer\0wglChoosePixelFormatARB\0wglGetPixelFormatAttribivARB\0wglSwapIntervalEXT\0";

static int _glewMajor, _glewMinor;
static int wgl_crippled;

GLenum glewInit() {
	const GLubyte* s;
	GLuint dot;
	GLint major, minor;
	/* query opengl version */
	s = glGetString(GL_VERSION);
	dot = _glewStrCLen(s, '.');
	if (dot == 0)
		return GLEW_ERROR_NO_GL_VERSION;
  
	major = s[dot-1]-'0';
	minor = s[dot+1]-'0';

	if (minor < 0 || minor > 9)
		minor = 0;
	if (major < 0 || major > 9)
		return GLEW_ERROR_NO_GL_VERSION;

	/* Add used extensions */
	_glewAddStrings((GLubyte const*)_glewUsed);

	if (major == 1 && minor == 0) {
		return GLEW_ERROR_GL_VERSION_10_ONLY;
	} else {
		int v = major * 0x10 + minor;
		_glewMajor = major;
		_glewMinor = minor;
		
if (v >= 0x13) _glewAvailable[0] = 1;
if (v >= 0x20) _glewAvailable[1] = 1;
	}

	_glewMarkAvailable((GLubyte*)glGetString(GL_EXTENSIONS));

#ifdef _WIN32
	{
		GLubyte* p = NULL;
		/* find wgl extension string query functions */
		PFNWGLGETEXTENSIONSSTRINGARBPROC _wglewGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)glewGetProcAddress((const GLubyte*)"wglGetExtensionsStringARB");
		PFNWGLGETEXTENSIONSSTRINGEXTPROC _wglewGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)glewGetProcAddress((const GLubyte*)"wglGetExtensionsStringEXT");
		/* initialize extensions */
		wgl_crippled = _wglewGetExtensionsStringARB == NULL && _wglewGetExtensionsStringEXT == NULL;

		if (_wglewGetExtensionsStringARB != NULL) {
			p = (GLubyte*)_wglewGetExtensionsStringARB(wglGetCurrentDC());
		} else if (_wglewGetExtensionsStringEXT != NULL) {
			p = (GLubyte*)_wglewGetExtensionsStringEXT();
		}

		_glewMarkAvailable(p);
	}
#endif

	{
		GLubyte const* funcs_per_ext = _glewFuncUsedPerExt;
		GLubyte const* cur_func = (GLubyte const*)__glewFuncUsedNames;
		int ext_index = 0;
		int func_index = 0;
		int r = 1, k = 0;

		for(ext_index = 0; ext_index < 6; ++ext_index) {
			int available = _glewAvailable[ext_index];
#ifdef _WIN32
			if (cur_func[0] == 'w' && wgl_crippled)
				available = 1; /* Try anyway */
#endif
			for (k = 0; k < funcs_per_ext[ext_index]; ++k) {
				if (available)
					r = ((_glewFuncTable[func_index] = (void(*)())glewGetProcAddress(cur_func)) != NULL) && r;

				++func_index;
				cur_func += _glewStrCLen(cur_func, '\0') + 1;
			}

			if(!r) available = 0;

			_glewAvailable[ext_index] = available;
		}
	}

	return GLEW_OK;
}
