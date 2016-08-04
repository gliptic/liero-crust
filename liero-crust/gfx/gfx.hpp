#ifndef UUID_5269BCE1BDB24F787DB6E9AA228BE4DA
#define UUID_5269BCE1BDB24F787DB6E9AA228BE4DA

#include "tl/platform.h"

#if 0 // TL_WINDOWS
#	ifdef GFX_EXPORTS
#		define GFX_API __declspec(dllexport)
#	else
#		define GFX_API __declspec(dllimport)
#	endif
#else
#	define GFX_API
#endif

#endif // UUID_5269BCE1BDB24F787DB6E9AA228BE4DA
