/* ------------------------------------------------------------------------- */

static PFNWGLGETEXTENSIONSSTRINGARBPROC _wglewGetExtensionsStringARB = NULL;
static PFNWGLGETEXTENSIONSSTRINGEXTPROC _wglewGetExtensionsStringEXT = NULL;

#include <stdlib.h>
#include <string.h>

GLboolean wglewGetExtension (const char* name)
{    
  GLubyte* p;
  GLubyte* end;
  GLuint len = _glewStrLen((const GLubyte*)name);
  if (_wglewGetExtensionsStringARB == NULL)
    if (_wglewGetExtensionsStringEXT == NULL)
      return GL_FALSE;
    else
      p = (GLubyte*)_wglewGetExtensionsStringEXT();
  else
    p = (GLubyte*)_wglewGetExtensionsStringARB(wglGetCurrentDC());
  if (0 == p) return GL_FALSE;
  end = p + _glewStrLen(p);
  while (p < end)
  {
    GLuint n = _glewStrCLen(p, ' ');
    if (len == n && _glewStrSame((const GLubyte*)name, p, n)) return GL_TRUE;
    p += n+1;
  }
  return GL_FALSE;
}

static void _glewFillTableWGL()
{
	GLubyte* p;
	GLubyte* copyp;
	if (_wglewGetExtensionsStringARB == NULL)
	{
		if (_wglewGetExtensionsStringEXT == NULL)
			return;
		else
			p = (GLubyte*)_wglewGetExtensionsStringEXT();
	}
	else
		p = (GLubyte*)_wglewGetExtensionsStringARB(wglGetCurrentDC());

	/* Copy since it may be invalidated */
	copyp = malloc(strlen((const char*)p) + 1);
	strcpy((char*)copyp, (const char*)p);

	_glewAddStrings(copyp);
}

static GLboolean wgl_crippled;

GLenum wglewContextInit (WGLEW_CONTEXT_ARG_DEF_LIST)
{
  
  /* find wgl extension string query functions */
  _wglewGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)glewGetProcAddress((const GLubyte*)"wglGetExtensionsStringARB");
  _wglewGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)glewGetProcAddress((const GLubyte*)"wglGetExtensionsStringEXT");
  /* initialize extensions */
  wgl_crippled = _wglewGetExtensionsStringARB == NULL && _wglewGetExtensionsStringEXT == NULL;

  _glewFillTableWGL();
