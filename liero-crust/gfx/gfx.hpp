#ifndef UUID_5269BCE1BDB24F787DB6E9AA228BE4DA
#define UUID_5269BCE1BDB24F787DB6E9AA228BE4DA

#include <tl/platform.h>

#if 0 // TL_WINDOWS
#	ifdef GFX_EXPORTS
#		define GFX_API __declspec(dllexport)
#	else
#		define GFX_API __declspec(dllimport)
#	endif
#else
#	define GFX_API
#endif

#define BONK_USE_GL2 1

int const AttribPosition = 0;
int const AttribColor = 1;
int const AttribTexCoord = 2;

int const AttribLast = AttribTexCoord;

#endif // UUID_5269BCE1BDB24F787DB6E9AA228BE4DA
