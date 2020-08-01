#include "../test_cellphase.hpp"

namespace test_cellphase {

struct Obj1 {

	Vector2 pos, vel;
	Token token;

	u16 ty_idx;

	i16 cell;

	Obj1() {
	}
};

typedef FixedObjectList<Obj1, ObjLimit, true> ObjList;

struct State1 : BaseState {

	
	ObjList objs;
	Cellphase obj_broadphase;
	

	State1()
		: obj_broadphase(ObjLimit) {
	}

	void create(Vector2 pos, Vector2 vel, u16 ty_idx) {
		auto* o = objs.new_object_reuse_queue();
		o->pos = pos;
		o->vel = vel;
		o->ty_idx = ty_idx;
		o->cell = 0;
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

			for (Obj1* n; (n = r.next()) != 0; ) {
				auto ipos = n->pos.cast<i32>();

				if (clipped.is_inside(ipos)) {
					clipped.unsafe_pixel32(ipos.x, ipos.y) = 0xffffffff;
				}
			}
		}
	}

	void process() {
		{
			auto r = objs.all();

			for (Obj1* b; (b = r.next()) != 0; ) {
				//NObject::ObjState new_state = liero::update(*b, *this, r, transient_state);
				auto new_state = update(*b, *this);

				if (new_state != ObjState::Alive) {

					//auto owner = b->owner;
					auto lpos = b->pos, lvel = b->vel;
					auto token = b->token;
					ObjType const& ty = this->obj_types[b->ty_idx];

					obj_broadphase.swap_remove(CellNode::Index(objs.index_of(b)), CellNode::Index(objs.size() - 1));
					objs.free(r);

					if (new_state == ObjState::Exploded) {
						//NObject::explode_obj(ty, lpos, lvel, owner, *this, transient_state);
						explode_obj(ty, lpos, lvel, *this, token);
					}
				} else {
					b->cell = obj_broadphase.update(narrow<CellNode::Index>(objs.index_of(b)), b->pos, b->cell);
				}
			}
		}

		auto new_nobjs = objs.flush_new_queue([&](Obj1*, u32 index) {
			//if (nobj->cell < 0) // TEMP DISABLE
			obj_broadphase.remove(narrow<CellNode::Index>(index));
			});

		for (auto& nobj : new_nobjs) {
			assert(nobj.cell >= 0);
			i16 obj_index = narrow<CellNode::Index>(objs.index_of(&nobj));
#if !IMPLICIT_NOBJ_CELL
			nobj.cell = obj_broadphase.insert(obj_index, nobj.pos);
#else
			this->nobject_broadphase.insert(obj_index, nobj.pos);
#endif
		}

		++this->frame;
	}

	inline bool obj_timed_out(Obj1& self);
};

inline bool State1::obj_timed_out(Obj1& self) {
	auto r = self.token.get_seed(0x8223190f);
	auto& ty = this->obj_types[self.ty_idx];

	u32 ttl = ty.time_to_live + r.get_u32_fast_(ty.time_to_live_v);
	return self.token.elapsed(this->frame) >= ttl;
}

}