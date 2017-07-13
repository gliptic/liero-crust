/* ------------------------------------------------------------------------- */

#include <stdlib.h>
#include <string.h>

typedef struct _glewStringEntry_
{
	const GLubyte* str;
} _glewStringEntry;

static GLuint strhash(const GLubyte* str)
{
	GLuint acc = 1;
	while(*str != ' ' && *str != '\0')
	{
		acc = 0x10100021*acc ^ *str++;
	}

	return (acc & 0xffffffff) >> _GLEW_STRING_TABLE_SHIFT;
}

static _glewStringEntry _glewStringTable[_GLEW_STRING_TABLE_SIZE];

static GLboolean _glewFindString(const GLubyte* str)
{
	GLuint h = strhash(str);
	const GLubyte* comp;
	while(comp = _glewStringTable[h].str)
	{
		if (_glewStrSame4(str, comp))
			return GL_TRUE;
		h = (h + 1) & (_GLEW_STRING_TABLE_SIZE - 1);
	}
	
	return GL_FALSE;
}

static void _glewAddString(const GLubyte* str)
{
	GLuint h;
	if (_glewFindString(str))
		return;

	h = strhash(str);
	while(_glewStringTable[h].str)
	{
		h = (h + 1) & (_GLEW_STRING_TABLE_SIZE - 1);
	}

	_glewStringTable[h].str = str;
}

static void _glewAddStrings(GLubyte* p)
{
	while (*p)
	{
		GLubyte* begin = p;
		while(*p && *p != ' ')
			++p;
		_glewAddString(begin);
		if(*p)
			++p; /* Skip space */
	}
}

static void _glewFillTable()
{
	GLubyte* p = (GLubyte*)glGetString(GL_EXTENSIONS);
	/* Copy since it may be invalidated */
	GLubyte*copyp = malloc(strlen((const char*)p) + 1);
	strcpy((char*)copyp, (const char*)p);

	_glewAddStrings(copyp);
}



/* 
 * Search for name in the extensions string. Use of strstr()
 * is not sufficient because extension names can be prefixes of
 * other extension names. Could use strtok() but the constant
 * string returned by glGetString might be in read-only memory.
 */
GLboolean glewGetExtension (const char* name)
{
	return _glewFindString(name);
#if 0
  GLubyte* p;
  GLubyte* end;
  GLuint len = _glewStrLen((const GLubyte*)name);
  p = (GLubyte*)glGetString(GL_EXTENSIONS);
  if (0 == p) return GL_FALSE;
  end = p + _glewStrLen(p);
  while (p < end)
  {
    GLuint n = _glewStrCLen(p, ' ');
    if (len == n && _glewStrSame((const GLubyte*)name, p, n)) return GL_TRUE;
    p += n+1;
  }
  return GL_FALSE;
#endif
}

/* ------------------------------------------------------------------------- */

static int _glewMajor, _glewMinor;

#ifndef GLEW_MX
static
#endif
GLenum glewContextInit (GLEW_CONTEXT_ARG_DEF_LIST)
{
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
  
  if (major == 1 && minor == 0)
  {
    return GLEW_ERROR_GL_VERSION_10_ONLY;
  }
  else
  {
    int v = major * 0x10 + minor;
	_glewMajor = major;
	_glewMinor = minor;

	

	if (v >= 0x11) _glewAddString("GL_VERSION_1_1");
	if (v >= 0x12) _glewAddString("GL_VERSION_1_2");
	if (v >= 0x13) _glewAddString("GL_VERSION_1_3");
	if (v >= 0x14) _glewAddString("GL_VERSION_1_4");
	if (v >= 0x15) _glewAddString("GL_VERSION_1_5");
	if (v >= 0x20) _glewAddString("GL_VERSION_2_0");
	if (v >= 0x21) _glewAddString("GL_VERSION_2_1");
	if (v >= 0x31) _glewAddString("GL_VERSION_3_1");
	if (v >= 0x32) _glewAddString("GL_VERSION_3_2");
	if (v >= 0x33) _glewAddString("GL_VERSION_3_3");
	if (v >= 0x40) _glewAddString("GL_VERSION_4_0");
  }

  _glewFillTable();
  /* initialize extensions */
