@include src/glew_head.h

extern char const* _glewUsed;
extern void (*_glewFuncTable[$FUNC_USED_COUNT$])();

@expand EXT_USED_GL #define $NAME$ (_glewUsed[$INDEX$])

@expand EXT_FUNC_USED_GL typedef $TYPEDECL$;

@expand EXT_FUNC_USED_GL #define $NAME$ (($TYPENAME$)_glewFuncTable[$INDEX$])

@expand EXT_TOKEN_USED_GL #define $NAME$ $VALUE$

@include src/glew_tail.h
