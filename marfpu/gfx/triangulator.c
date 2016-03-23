#include "triangulator.h"

#include <stdlib.h>
//#include <stdio.h>
//#include <tl/vector.h>
#include <tl/std.h>
#include <tl/list.h>

struct trap;

typedef struct segment {
	vec from, dir;
	struct trap* edge_trap;
#ifndef NDEBUG
	int id;
#endif
} segment;

typedef struct trap {
	tl_list_node node;
	double prev_x1, prev_x2;
	double prev_y;

	segment* left_seg;
	segment* right_seg;
} trap;

enum event_type {
	begin,
	split,
	join,
	continuation
};

typedef struct vector_event_extra {
	int type;
	segment* seg[2];
	double x, y;
#ifndef NDEBUG
	int id;
#endif
} vector_event_extra;

typedef struct vector_event {
	double y;
	vector_event_extra* extra;
} vector_event;

//static int vector_event_lt(vector_event const* a, vector_event const* b)
//{ return (a->y < b->y) || (a->y == b->y && a->extra->x < b->extra->x); }

static int vector_event_compare_p(double ax, double ay, void const* ap, double bx, double by, void const* bp) {
	assert(ap != bp);
	if(ay < by) return -1;
	if(ay == by)
	{
		if(ax < bx) return -1;
		//if(a->extra->x == b->extra->x) return 0;
		if(ax == bx)
		{
			if(ap < bp) return -1;
		}
	}

	return 1;
}

static int vector_event_compare(void const* a_, void const* b_) {
	vector_event const* a = a_;
	vector_event const* b = b_;

	return vector_event_compare_p(
		a->extra->x, a->y, a->extra,
		b->extra->x, b->y, b->extra);

	/*
	assert(a != b);
	if(a->y < b->y) return -1;
	if(a->y == b->y)
	{
		if(a->extra->x < b->extra->x) return -1;
		//if(a->extra->x == b->extra->x) return 0;
		if(a->extra->x == b->extra->x)
		{
			if(a->extra < b->extra) return -1;
		}
	}

	return 1;*/
}

//std::vector<trap*> traps;

static double find_x(segment* seg, double y) {
	if(seg->dir.y == seg->from.y)
		return seg->from.x;
	//return seg->from.x + (y - seg->from.y) * seg->dir.x / seg->dir.y;
	return seg->from.x + (y - seg->from.y) * (seg->dir.x - seg->from.x) / (seg->dir.y - seg->from.y);
}

static int orient2d_fast(double ax, double ay, double bx, double by, double cx, double cy) {
	//return ((bx - ax)*(cy - ay) - (cx - ax)*(by - ay)) > 0.0 ? 1 : -1;
	return (ay - by)*(cx - ax) + (bx - ax)*(cy - ay) > 0.0 ? 1 : -1;
}



typedef struct triangulator {
	triangle* triangles;
	triangle* next_triangle;

	tl_list active_traps;

	trap* traps;
	trap* next_trap;
} triangulator;

static int trap_id(triangulator* self, trap* t) {
	return (int)(t - self->traps);
}

#ifndef NDEBUG
static int segment_id(triangulator* self, segment* s) {
	return s->id;
}
#endif

#define DEBUG_PRINT(p) (void)0
//#define DEBUG_PRINT(p) printf p
#define TRI_ASSERT(p) (void)0
//#define TRI_ASSERT(p) assert(p)

static void render_trap(triangulator* self, trap* t, double y, double x1, double x2) {
	if(t->prev_y >= y) return;

	TRI_ASSERT(x1 <= x2);
	if(x1 > x2) return; // Ignore non-sensical ones

	if(t->prev_x1 == t->prev_x2)
	{
		self->next_triangle->vert[0].x = t->prev_x1;
		self->next_triangle->vert[0].y = t->prev_y;
		self->next_triangle->vert[1].x = x2;
		self->next_triangle->vert[1].y = y;
		self->next_triangle->vert[2].x = x1;
		self->next_triangle->vert[2].y = y;

		++self->next_triangle;
	}
	else
	{
		self->next_triangle->vert[0].x = t->prev_x1;
		self->next_triangle->vert[0].y = t->prev_y;
		self->next_triangle->vert[1].x = t->prev_x2;
		self->next_triangle->vert[1].y = t->prev_y;
		self->next_triangle->vert[2].x = x1;
		self->next_triangle->vert[2].y = y;

		++self->next_triangle;

		if(x1 != x2)
		{
			self->next_triangle->vert[0].x = t->prev_x2;
			self->next_triangle->vert[0].y = t->prev_y;
			self->next_triangle->vert[1].x = x2;
			self->next_triangle->vert[1].y = y;
			self->next_triangle->vert[2].x = x1;
			self->next_triangle->vert[2].y = y;

			++self->next_triangle;
		}
	}

	t->prev_y = y;
	t->prev_x1 = x1;
	t->prev_x2 = x2;
}

// We render a trapezoid when a join or continuation event happens on it, or when a split happens
// inside a trapezoid.



void visit_join(triangulator* self, vector_event_extra* ev) {
	trap* left_trap = ev->seg[0]->edge_trap;
	trap* right_trap = ev->seg[1]->edge_trap;

	assert(left_trap && right_trap);
	if(!left_trap || !right_trap) return; // One of the segments not visited, bail out

	if(left_trap == right_trap)
	{
		//TRI_ASSERT(ev->seg[0] == left_trap->left_seg);
		//TRI_ASSERT(ev->seg[1] == left_trap->right_seg);

		render_trap(self, left_trap, ev->y, ev->x, ev->x);

		DEBUG_PRINT(("(%f, %f) Ended trap %d\n",
			ev->x, ev->y, trap_id(self, left_trap)));
	}
	else
	{
		DEBUG_PRINT(("(%f, %f) Joined the two traps %d and %d, seg %d and %d\n",
			ev->x, ev->y, trap_id(self, left_trap), trap_id(self, right_trap),
			segment_id(self, ev->seg[0]),
			segment_id(self, ev->seg[1])));

		TRI_ASSERT(ev->seg[1] == right_trap->left_seg);
		TRI_ASSERT(ev->seg[0] == left_trap->right_seg);

		render_trap(self, left_trap, ev->y, find_x(left_trap->left_seg, ev->y), ev->x);
		render_trap(self, right_trap, ev->y, ev->x, find_x(right_trap->right_seg, ev->y));

		left_trap->right_seg = right_trap->right_seg;
		left_trap->prev_x2 = right_trap->prev_x2;
		right_trap->right_seg->edge_trap = left_trap;

		
	}

	tl_list_unlink(&right_trap->node);
}

void visit_continuation(triangulator* self, vector_event_extra* ev) {
	trap* t = ev->seg[0]->edge_trap;

	assert(t);
	if(!t) return; // Previous segment not visited, bail out

	if(t->right_seg == ev->seg[0])
	{
		t->right_seg = ev->seg[1];

		render_trap(self, t, ev->y, find_x(t->left_seg, ev->y), ev->x);

		DEBUG_PRINT(("(%f, %f) Continued trap %d on right side, from seg %d to %d\n",
			ev->x, ev->y, trap_id(self, t),
			segment_id(self, ev->seg[0]),
			segment_id(self, ev->seg[1])));
	}
	else
	{
		TRI_ASSERT(t->left_seg == ev->seg[0]);
		t->left_seg = ev->seg[1];

		render_trap(self, t, ev->y, ev->x, find_x(t->right_seg, ev->y));

		DEBUG_PRINT(("(%f, %f) Continued trap %d on left side, from seg %d to %d\n",
			ev->x, ev->y, trap_id(self, t),
			segment_id(self, ev->seg[0]),
			segment_id(self, ev->seg[1])));
	}
	ev->seg[1]->edge_trap = t;
}

void visit_begin(triangulator* self, vector_event_extra* ev) {
	//trap* new_t = new trap(ev.x, ev.x, ev.y);
	trap* new_t = self->next_trap++;
	new_t->prev_x1 = ev->x;
	new_t->prev_x2 = ev->x;
	new_t->prev_y = ev->y;

	new_t->left_seg = ev->seg[0];
	ev->seg[0]->edge_trap = new_t;
	new_t->right_seg = ev->seg[1];
	ev->seg[1]->edge_trap = new_t;
	//traps.push_back(new_t);

	DEBUG_PRINT(("(%f, %f) Begun trap %d, seg %d and %d\n",
		ev->x, ev->y, trap_id(self, new_t),
		segment_id(self, ev->seg[0]),
		segment_id(self, ev->seg[1])));

	tl_list_add(&self->active_traps, &new_t->node);
}

void visit_split(triangulator* self, vector_event_extra* ev) {
	// TODO: This is the only case where we need to search the trapezoid
	// set. Lower time complexity.
	tl_list_foreach(&self->active_traps, trap, node, t, {
		double left = find_x(t->left_seg, ev->y);
		double right = find_x(t->right_seg, ev->y);

		if(left <= ev->x && right > ev->x)
		{
			trap* right_t;
			render_trap(self, t, ev->y, left, right);

			//trap* right_t = new trap(ev.x, right, ev.y);
			right_t = self->next_trap++;
			right_t->prev_x1 = ev->x;
			right_t->prev_x2 = right;
			right_t->prev_y = ev->y;

			right_t->left_seg = ev->seg[1];
			ev->seg[1]->edge_trap = right_t;
			right_t->right_seg = t->right_seg;
			t->right_seg->edge_trap = right_t;

			t->prev_x2 = ev->x;
			t->right_seg = ev->seg[0];
			ev->seg[0]->edge_trap = t;

			DEBUG_PRINT(("(%f, %f) Split trap %d, added trap %d, seg %d and %d\n",
				ev->x, ev->y, trap_id(self, t), trap_id(self, right_t),
				segment_id(self, ev->seg[0]),
				segment_id(self, ev->seg[1])));

			//traps.push_back(right_t);
			tl_list_add(&self->active_traps, &right_t->node);
			return;
		}
	});

	visit_begin(self, ev);
}

triangulator* triangulator_create(double const* vertices, int* vertex_count, int ring_count) {
	int total_count = 0, i, j;
	int trap_count = 0;
	int triangle_bound = 0;
	int event_count = 0;
	segment* segments = NULL;
	segment* next_segment;
	double const* next_vertex;

	vector_event* events;
	vector_event* ev;

	vector_event_extra* events_extra;
	vector_event_extra* ev_extra;

	triangulator* self = malloc(sizeof(triangulator));
	tl_list_init(&self->active_traps);

	for(i = 0; i < ring_count; ++i)
		total_count += vertex_count[i];

	segments = malloc(total_count * sizeof(segment));
	next_segment = segments;
	//self->segments = segments;

	events = malloc(total_count * sizeof(vector_event));
	ev = events;

	events_extra = malloc(total_count * sizeof(vector_event_extra));
	ev_extra = events_extra;

	next_vertex = vertices;

	for(i = 0; i < ring_count; ++i) {
		int count = vertex_count[i];
		segment*   first_seg = next_segment;
		vector_event_extra* first_ev_extra = ev_extra;
		double const* first_vert = next_vertex;

		for(j = 0; j < count; ++j) {
			// segment j -> next
			int prev = (j + count - 1) % count;
			int next = (j + 1) % count;

			double xp = first_vert[prev*2];
			double yp = first_vert[prev*2+1];
			double x = next_vertex[0];
			double y = next_vertex[1];
			double xn = first_vert[next*2];
			double yn = first_vert[next*2+1];
			int prev_first = 0;
			int dup = 0;

			int cpc = vector_event_compare_p(xp, yp, first_ev_extra + prev, x, y, ev_extra);
			int ccn = vector_event_compare_p(x, y, ev_extra, xn, yn, first_ev_extra + next);

			if(x == xn && y == yn)
				DEBUG_PRINT(("Duplicate point %d @ (%f, %f)\n", j, x, y));
			
			//++event_count;

			if((next_segment - segments) == 27)
				dup = dup;

			if((y == yn) || (y == yp))
				dup = 1;

			//if((yp < y) != (yn < y) || dup)
			//if(((yp < y) && (yn < y)) || ((yp > y) && (yn > y)))
			if(((cpc < 0) && (ccn > 0)) || ((cpc > 0) && (ccn < 0)))
			{
				// Both on one side
				int w = orient2d_fast(xp, yp, x, y, xn, yn);
				
				if(ccn < 0)
				{
					prev_first = w > 0;

					//if(!prev_first)
						triangle_bound += 2; // begin does not produce triangles

					//ev_extra->type = (w > 0) ? begin : split;
					ev_extra->type = split;
					++trap_count;
				}
				else
				{
					prev_first = w < 0;
					triangle_bound += 4; // Join can produce 4!
					ev_extra->type = join;
				}
			}
			else
			{
				triangle_bound += 2;
				ev_extra->type = continuation;
				/*
				if(yp < y || (yp == y && (xp < x || (xp == x && prev < j))))
					prev_first = 1; // prev will be visited first*/

				if(yp < y || (yp == y && (xp < x || (xp == x && prev < j))))
					prev_first = 1; // prev will be visited first*/
			}

			if(prev_first)
			{
				ev_extra->seg[0] = first_seg + prev;
				ev_extra->seg[1] = first_seg + j;
			}
			else
			{
				ev_extra->seg[0] = first_seg + j;
				ev_extra->seg[1] = first_seg + prev;
			}

			if(y <= yn)
			{
				next_segment->from.x = x;
				next_segment->from.y = y;
				next_segment->dir.x = xn; // - x;
				next_segment->dir.y = yn; // - y;
			}
			else
			{
				next_segment->from.x = xn;
				next_segment->from.y = yn;
				next_segment->dir.x = x; // - xn;
				next_segment->dir.y = y; // - yn;
			}

			next_segment->edge_trap = NULL;
#ifndef NDEBUG
			next_segment->id = (int)(next_segment - segments);
#endif

			ev_extra->x = x;
			ev_extra->y = y;
			ev->y = y;
#ifndef NDEBUG
			ev_extra->id = (int)(next_segment - segments);
#endif
			ev->extra = ev_extra;

			++ev_extra;
			++next_segment;
			++ev;
			next_vertex += 2;
		}
	}

	qsort(events, total_count, sizeof(vector_event), vector_event_compare);

	self->triangles = malloc(triangle_bound * sizeof(triangle));
	self->next_triangle = self->triangles;

	self->traps = malloc(trap_count * sizeof(trap));
	self->next_trap = self->traps;

	for(i = 0; i < total_count; ++i)
	{
		vector_event_extra* ev_extra = events[i].extra;

		switch(ev_extra->type)
		{
			case begin: visit_begin(self, ev_extra); break;
			case split: visit_split(self, ev_extra); break;
			case join:  visit_join(self, ev_extra); break;
			default: /*case continuation:*/ visit_continuation(self, ev_extra); break;
		}
	}

	free(segments);
	free(events);
	free(events_extra);

	return self;
}

triangle* triangulator_get_triangles(triangulator* self, int* count) {
	*count = (int)(self->next_triangle - self->triangles);
	return self->triangles;
}

void triangulator_free(triangulator* self) {
	free(self->traps);
	free(self->triangles);
	free(self);
}
