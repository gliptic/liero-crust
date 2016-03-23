#ifndef UUID_CA40FB257B704973309351AD9E18848C
#define UUID_CA40FB257B704973309351AD9E18848C

#include "gfx.h"

typedef struct vec
{
	double x, y;
} vec;

typedef struct triangle
{
	vec vert[3];
} triangle;

typedef struct triangulator triangulator;

GFX_API triangulator* triangulator_create(double const* vertices, int* vertex_count, int ring_count);
GFX_API triangle* triangulator_get_triangles(triangulator* self, int* count);
GFX_API void triangulator_free(triangulator* self);

#endif // UUID_CA40FB257B704973309351AD9E18848C
