@include src/glew_head.c

#include <stdlib.h>
#include <string.h>

typedef struct _glewStringEntry_ {
	GLubyte const* str;
	int index;
} _glewStringEntry;

static int _glewAvailable[$EXT_USED_COUNT$];

static GLuint strhash(const GLubyte* str) {
	GLuint acc = 1;
	while(*str != ' ' && *str != '\0')
	{
		acc = 0x10100021*acc ^ *str++;
	}

	return (acc & 0xffffffff) >> $TABLE_SHIFT$;
}

static _glewStringEntry _glewStringTable[$TABLE_SIZE$];

static _glewStringEntry* _glewFindString(GLubyte const* str) {
	GLuint h = strhash(str);
	const GLubyte* comp;
	while ((comp = _glewStringTable[h].str) != 0) {
		if (_glewStrSame4(str, comp))
			return &_glewStringTable[h];
		h = (h + 1) & ($TABLE_SIZE$ - 1);
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
		h = (h + 1) & ($TABLE_SIZE$ - 1);
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

char const* _glewUsed = "$EXT_USED_DESC$";
void (*_glewFuncTable[$FUNC_USED_COUNT$])();

static GLubyte _glewFuncUsedPerExt[$EXT_USED_COUNT$] = {
	$FUNC_USED_PER_EXT$
};

static char const* __glewFuncUsedNames = "$FUNC_USED_DESC$";

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
		
@expand		EXT_MARK_GL_VERSION if (v >= $HEXVERSION$) _glewAvailable[$INDEX$] = 1;
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
			p = (GLubyte*)_wglewGetExtensionsStringARB(tl::win::wglGetCurrentDC());
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

		for(ext_index = 0; ext_index < $EXT_USED_COUNT$; ++ext_index) {
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

#ifdef __cplusplus
}
#endif
