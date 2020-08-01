#ifndef PHYS_SPACE_HPP
#define PHYS_SPACE_HPP 1

#include "tl/vec.hpp"
#include "tl/vector.hpp"

namespace phys {

struct Body {
	tl::VectorF2 rot;
	tl::VectorF2 pos, vel;
	f32 mass, inv_mass;
	f32 i, inv_i;
};

struct Space {
	tl::Vec<Body> bodies;
	tl::Vec<u32> body_indexes;
	u32 first_free_body_index;

	void add_body_index(u32 index, u32 pos) {
		while (body_indexes.size() <= index) {
			body_indexes.push_back(0);
		}

		body_indexes[index] = pos;
	}

	void body_index_swap_delete(u32 at, u32 top) {
		auto at_pos = body_indexes[at];
		body_indexes[top] = at_pos;
	}

	u32 add_body(Body body) {
		u32 index = (u32)body_indexes.size();
		u32 pos = (u32)bodies.size();
		bodies.push_back(body);
		body_indexes.push_back(pos);
		return index;
	}

	void update() {
		for (Body& body : this->bodies) {
			body.pos += body.vel;

		}
	}
};

}

#endif
