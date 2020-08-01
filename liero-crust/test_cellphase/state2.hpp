#include "../test_cellphase.hpp"

namespace test_cellphase {

struct Obj2 {

	Vector2 pos, vel;
	Token token;

	u16 ty_idx;

	u16 key;

	Obj2() {
	}
};

typedef FixedObjectList<Obj2, ObjLimit, true> ObjList2;

/*
static const unsigned int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF};
static const unsigned int S[] = {1, 2, 4, 8};

unsigned int x; // Interleave lower 16 bits of x and y, so the bits of x
unsigned int y; // are in the even positions and bits from y in the odd;
unsigned int z; // z gets the resulting 32-bit Morton Number.  
// x and y must initially be less than 65536.

x = (x | (x << S[3])) & B[3];
x = (x | (x << S[2])) & B[2];
x = (x | (x << S[1])) & B[1];
x = (x | (x << S[0])) & B[0];

y = (y | (y << S[3])) & B[3];
y = (y | (y << S[2])) & B[2];
y = (y | (y << S[1])) & B[1];
y = (y | (y << S[0])) & B[0];

z = x | (y << 1);
*/

u16 interleave_bytes(u8 x, u8 y) {
	u32 x0 = (x | (x << 4)) & 0x0F0F;
	x0 = (x0 | (x0 << 2)) & 0x3333;
	x0 = (x0 | (x0 << 1)) & 0x5555;

	u32 y0 = (y | (y << 4)) & 0x0F0F;
	y0 = (y0 | (y0 << 2)) & 0x3333;
	y0 = (y0 | (y0 << 1)) & 0x5555;
	return x0 | (y0 << 1);
}

u16 interleave_bytes2(u8 x, u8 y) {
	u32 x0 = (y << 16) | x;
	x0 = (x0 | (x0 << 4)) & 0x0F0F;
	x0 = (x0 | (x0 << 2)) & 0x3333;
	x0 = (x0 | (x0 << 1)) & 0x5555;
	return u16((x0 >> 15) | x0);
}

struct State2 : BaseState {

	ObjList2 objs;
	
	State2() {
	}

	u16 sort_key(Vector2 pos) {
		auto x = (u8)(pos.x.raw() >> (16 + 1));
		auto y = (u8)(pos.y.raw() >> (16 + 1));

		return interleave_bytes(x, y);
	}

	u16 sort_key(tl::VectorI2 pos) {
		auto x = (u8)(pos.x >> 1);
		auto y = (u8)(pos.y >> 1);

		return interleave_bytes(x, y);
	}

	void create(Vector2 pos, Vector2 vel, u16 ty_idx) {
		auto* o = objs.new_object_reuse_queue();
		o->pos = pos;
		o->vel = vel;
		o->ty_idx = ty_idx;
		o->key = sort_key(pos);
		o->token = Token(this->frame, objs.index_of(o));
	}

	void start() {
		this->create(
			Vector2(Scalar(20), Scalar(20)),
			Vector2(Scalar(1), Scalar(1)), 0);
	}

	void draw(liero::DrawTarget target) {
		auto clipped = target.image.crop(tl::RectU(0, 0, 504, 350));

		clipped.blit(level.graphics.slice(), &target.pal, 0, 0);

		{
			auto r = objs.all();

			Vector2 ul = Vector2(285, 160);
			Vector2 lr = Vector2(290, 165);

			u16 ul_k = sort_key(ul);
			u16 ur_k = sort_key(Vector2(lr.x, ul.y));
			u16 ll_k = sort_key(Vector2(ul.x, lr.y));
			u16 lr_k = sort_key(lr);
			u16 low = tl::min(tl::min(ul_k, ur_k), tl::min(ll_k, lr_k));
			u16 high = tl::max(tl::max(ul_k, ur_k), tl::max(ll_k, lr_k));

			for (i32 y = 0; y < 350; ++y) {
				for (i32 x = 0; x < 504; ++x) {
					auto i = sort_key(tl::VectorI2(x, y));
					if (low <= i && i <= high) {
						clipped.set_pixel32(tl::VectorI2(x, y), 0x00007f00);
					}
				}

			}

			auto col = 0x0000ff00;
			for (i32 x = (i32)ul.x; x <= (i32)lr.x; ++x) {
				clipped.set_pixel32(tl::VectorI2(x, (i32)ul.y), col);
				clipped.set_pixel32(tl::VectorI2(x, (i32)lr.y), col);
			}

			for (i32 y = (i32)ul.y; y <= (i32)lr.y; ++y) {
				clipped.set_pixel32(tl::VectorI2((i32)ul.x, y), col);
				clipped.set_pixel32(tl::VectorI2((i32)lr.x, y), col);
			}

			for (Obj2* n; (n = r.next()) != 0; ) {
				auto ipos = n->pos.cast<i32>();

				clipped.set_pixel32(ipos, n->key >= low && n->key <= high ?
					0x0000ff00 :
					0xffffffff);
			}

			
		}
	}

	void process() {
		{
			auto r = objs.all_ordered();

			for (; r.has_more(); ) {
				Obj2* b = r.current();

				//NObject::ObjState new_state = liero::update(*b, *this, r, transient_state);
				auto new_state = update(*b, *this);

				if (new_state != ObjState::Alive) {

					//auto owner = b->owner;
					auto lpos = b->pos, lvel = b->vel;
					auto token = b->token;
					ObjType const& ty = this->obj_types[b->ty_idx];

					//obj_broadphase.swap_remove(CellNode::Index(objs.index_of(b)), CellNode::Index(objs.size() - 1));
					//objs.free(r);
					objs.next_free(r);

					if (new_state == ObjState::Exploded) {
						//NObject::explode_obj(ty, lpos, lvel, owner, *this, transient_state);
						explode_obj(ty, lpos, lvel, *this, token);
					}
				} else {
					b->key = sort_key(b->pos);
					r.next();
					//b->cell = obj_broadphase.update(narrow<CellNode::Index>(objs.index_of(b)), b->pos, b->cell);
				}
			}
		}

		auto new_nobjs = objs.flush_new_queue([&](Obj2*, u32 index) {
			//if (nobj->cell < 0) // TEMP DISABLE
			//obj_broadphase.remove(narrow<CellNode::Index>(index));
			});

		for (auto& nobj : new_nobjs) {
		}

		++this->frame;
	}

	inline bool obj_timed_out(Obj2& self);
};

inline bool State2::obj_timed_out(Obj2& self) {
	auto r = self.token.get_seed(0x8223190f);
	auto& ty = this->obj_types[self.ty_idx];

	u32 ttl = ty.time_to_live + r.get_u32_fast_(ty.time_to_live_v);
	return self.token.elapsed(this->frame) >= ttl;
}

}