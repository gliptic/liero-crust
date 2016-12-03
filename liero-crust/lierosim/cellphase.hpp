#ifndef LIERO_CELLPHASE_HPP
#define LIERO_CELLPHASE_HPP

#include <tl/std.h>
#include <tl/bits.h>
#include <tl/vec.hpp>
#include <tl/vector.hpp>

#define INTERLEAVED_REFS 0

namespace liero {

using tl::narrow;

struct CellNode {
	typedef i16 Index;

	CellNode(Index n, Index p) : next(n), prev(p) {
	}

	Index next, prev;
};

struct Cellphase {
	static u32 const CellShift = 4; // 16
	static u32 const WorldShift = 9; // 512
	static u32 const GridShift = WorldShift - CellShift;
	static u32 const GridWidth = 1 << GridShift;
	static u32 const GridSize = GridWidth * GridWidth;
	static u32 const CellMask = GridWidth - 1;

	struct AreaRange {

#if INTERLEAVED_REFS
		CellNode const* base;
#else
		CellNode::Index const* base;
#endif
		i32 pitch_minus_w;
		i32 xy_h_end, xy_v_end;
		i32 n;

		bool next(u16& ret) {
			do {
#if INTERLEAVED_REFS
				n = base[n].next;
#else
				n = base[n];
#endif
				if (n >= 0) {
					ret = (u16)n;
					return true;
				}

				i32 skip = i32(n & CellMask) == xy_h_end ? pitch_minus_w : 0;

				n = mask_neg(n + skip);
			} while (n != xy_v_end);

			return false;
		}
	};

	// TODO: If max_count is limited to (2^16 - GridSize) anyway, it's probably better to allocate inline
#if INTERLEAVED_REFS
	CellNode* base;
#else
	CellNode::Index* nexts;
	CellNode::Index* prevs;
#endif
	u32 max_count;

	Cellphase(u32 max_count_init)
		: max_count(max_count_init) {

#if INTERLEAVED_REFS
		auto cells = (CellNode *)malloc(sizeof(CellNode) * (GridSize + max_count_init));
		this->base = cells + GridSize;
#else
		auto cells = (CellNode::Index *)malloc(sizeof(CellNode::Index) * (GridSize + max_count_init) * 2);
		this->nexts = cells + GridSize;
		this->prevs = this->nexts + max_count_init + GridSize;
#endif

		for (u32 i = 0; i < GridSize + max_count_init; ++i) {
			auto prev_index = mask_neg(i - GridSize - 1);
			auto next_index = mask_neg(i - GridSize + 1);
			set(i - GridSize, next_index, prev_index);
		}

	}

	static CellNode::Index mask_neg(i32 n) {
		return CellNode::Index(n | ~(CellMask | (CellMask << GridShift)));
	}

	static CellNode::Index mask(i32 n) {
		return CellNode::Index(n & (CellMask | (CellMask << GridShift)));
	}

#if INTERLEAVED_REFS
	inline CellNode* cells() {
		return base - GridSize;
	}

	inline CellNode const* cells() const {
		return base - GridSize;
	}
#else
	inline CellNode::Index* cells() {
		return this->nexts - GridSize;
	}

	inline CellNode::Index const* cells() const {
		return this->nexts - GridSize;
	}
#endif

	void copy_from(Cellphase const& other, usize count) {
		assert(max_count == other.max_count && count <= max_count);

#if INTERLEAVED_REFS
		memcpy(this->cells(), other.cells(), sizeof(CellNode) * (GridSize + count));
#else
		memcpy(this->nexts - GridSize, other.nexts - GridSize, sizeof(CellNode::Index) * (GridSize + count));
		memcpy(this->prevs - GridSize, other.prevs - GridSize, sizeof(CellNode::Index) * (GridSize + count));
#endif
	}

	~Cellphase() {
		free(cells());
	}

	void validate_idx(i32 idx) {
		TL_UNUSED(idx);
		assert(idx >= -(i32)GridSize && idx < (i32)max_count);
	}

#if INTERLEAVED_REFS
	void insert_outside_grid(CellNode::Index idx) {
		this->base[idx] = CellNode(idx, idx);
	}

	void set(i32 idx, CellNode::Index next, CellNode::Index prev) {
		this->base[idx] = CellNode(next, prev);
	}

	void set_next(i32 idx, CellNode::Index next) {
		this->base[idx].next = next;
	}

	void set_prev(i32 idx, CellNode::Index prev) {
		this->base[idx].prev = prev;
	}

	CellNode get(i32 idx) {
		return this->base[idx];
	}

	CellNode::Index get_next(i32 idx) {
		return this->base[idx].next;
	}

	CellNode::Index get_prev(i32 idx) {
		return this->base[idx].prev;
	}
#else

	void set(i32 idx, CellNode::Index next, CellNode::Index prev) {
		this->nexts[idx] = next;
		this->prevs[idx] = prev;
	}

	void set_next(i32 idx, CellNode::Index next) {
		this->nexts[idx] = next;
	}

	void set_prev(i32 idx, CellNode::Index prev) {
		this->prevs[idx] = prev;
	}

	CellNode get(i32 idx) {
		return CellNode(this->nexts[idx], this->prevs[idx]);
	}

	CellNode::Index get_next(i32 idx) {
		return this->nexts[idx];
	}

	CellNode::Index get_prev(i32 idx) {
		return this->prevs[idx];
	}
#endif

	void validate(i32 idx) {
		validate_idx(idx);
		assert(get_next(get_prev(idx)) == idx);
		assert(get_prev(get_next(idx)) == idx);
	}

	CellNode::Index insert(CellNode::Index idx, Vector2 pos) {
		assert((i32)idx < (i32)max_count);
		u32 cx = (u32)pos.x.raw() >> (16 + CellShift);
		u32 cy = ((u32)pos.y.raw() >> (16 + CellShift)) << GridShift;

		auto new_cell = mask_neg(cx + cy);

		// sp <-> new_cell  ---->  sp <-> idx <-> new_cell
		auto sp = get_next(new_cell);
		set_prev(sp, idx);
		set(idx, sp, new_cell);
		set_next(new_cell, idx);

		assert(get_prev(idx) == new_cell);
		assert(get_next(idx) == sp);
		assert(get_prev(sp) == idx);

		return new_cell;
	}

	CellNode::Index update(CellNode::Index idx, tl::VectorI2 pos, Vector2 old_pos) {

		u32 old_cx = (u32)old_pos.x.raw() >> (16 + CellShift);
		u32 old_cy = ((u32)old_pos.y.raw() >> (16 + CellShift)) << GridShift;
		auto old_cell = mask_neg(old_cx + old_cy);

		u32 cx = (u32)pos.x >> CellShift;
		u32 cy = ((u32)pos.y >> CellShift) << GridShift;
		auto new_cell = mask_neg(cx + cy);

		if (old_cell != new_cell) {

			// Unlink
			this->remove(idx);

			// Relink
			auto sp = get_next(new_cell);
			set_prev(sp, idx);
			set(idx, sp, new_cell);
			set_next(new_cell, idx);

			assert(get_prev(idx) == new_cell);
			assert(get_next(idx) == sp);
			assert(get_prev(sp) == idx);
		}

		return new_cell;
	}

	CellNode::Index update(CellNode::Index idx, Vector2 pos, CellNode::Index cur_cell) {
		u32 cx = (u32)pos.x.raw() >> (16 + CellShift);
		u32 cy = ((u32)pos.y.raw() >> (16 + CellShift)) << GridShift;
		auto new_cell = mask_neg(cx + cy);

		if (cur_cell != new_cell) {

			// Unlink
			this->remove(idx);

			// Relink
			auto sp = get_next(new_cell);
			set_prev(sp, idx);
			set(idx, sp, new_cell);
			set_next(new_cell, idx);

			assert(get_prev(idx) == new_cell);
			assert(get_next(idx) == sp);
			assert(get_prev(sp) == idx);
		}

		return new_cell;
	}

	void remove(CellNode::Index idx) {
		// Unlink
		auto np = get(idx);
		assert(get_next(np.prev) == idx);
		assert(get_prev(np.next) == idx);

		set_next(np.prev, np.next);
		set_prev(np.next, np.prev);
	}

	AreaRange area(i32 x1, i32 y1, i32 x2, i32 y2) {
		AreaRange ar;

		u32 cx1 = (u32(x1) >> CellShift);
		u32 cy1 = (u32(y1) >> CellShift) << GridShift;
		u32 cx2 = (u32(x2) >> CellShift);
		u32 cy2 = (u32(y2) >> CellShift) << GridShift;

		auto first = mask_neg(cx1 + cy1);

		ar.n = first;
#if INTERLEAVED_REFS
		ar.base = base;
#else
		ar.base = nexts;
#endif

		ar.pitch_minus_w = i32(GridWidth + cx1 - cx2 - 1);
		ar.xy_h_end = (cx2 + 1) & CellMask;
		ar.xy_v_end = mask_neg(cx1 + cy2 + GridWidth);

		return ar;
	}

	void move_last(CellNode::Index last_idx, CellNode::Index move_to_idx) {

		if (last_idx != move_to_idx) {
			// Fix up neighbours of moved node
			auto last = get(last_idx);

			set_next(move_to_idx, last.next);
			set_prev(move_to_idx, last.prev);

			assert(get_next(last.prev) == last_idx);
			assert(get_prev(last.next) == last_idx);

			set_next(last.prev, move_to_idx);
			set_prev(last.next, move_to_idx);
		}

#if !defined(NDEBUG) && 0
		for (u32 i = 0; i < GridSize; ++i) {
			//assert(this->base[i].next != last_idx);
			//assert(this->base[i].prev != last_idx);
		}
#endif
	}

	void swap_remove(CellNode::Index idx, CellNode::Index last_idx) {

		this->remove(idx);

		this->move_last(last_idx, idx);
	}
};

}

#endif // LIERO_CELLPHASE_HPP
