#ifndef LIERO_CELLPHASE_HPP
#define LIERO_CELLPHASE_HPP

#include <tl/std.h>
#include <tl/bits.h>
#include <tl/vec.hpp>
#include <tl/vector.hpp>

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

		CellNode const* base;
		i32 pitch_minus_w;
		i32 xy_h_end, xy_v_end;
		i32 n;

		bool next(u16& ret) {
			do {
				n = base[n].next;
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
	//CellNode* cells;
	CellNode* base;
	u32 max_count;

	Cellphase(u32 max_count_init)
		: max_count(max_count_init) {

		auto cells = (CellNode *)malloc(sizeof(CellNode) * (GridSize + max_count_init));

		for (u32 i = 0; i < GridSize + max_count_init; ++i) {
			auto prev_index = mask_neg(i - GridSize - 1);
			auto next_index = mask_neg(i - GridSize + 1);
			cells[i] = CellNode(next_index, prev_index);
		}

		this->base = cells + GridSize;
	}

	static CellNode::Index mask_neg(i32 n) {
		return CellNode::Index(n | ~(CellMask | (CellMask << GridShift)));
	}

	static CellNode::Index mask(i32 n) {
		return CellNode::Index(n & (CellMask | (CellMask << GridShift)));
	}

	inline CellNode* cells() {
		return base - GridSize;
	}

	inline CellNode const* cells() const {
		return base - GridSize;
	}

	void copy_from(Cellphase const& other, usize count) {
		assert(max_count == other.max_count && count <= max_count);

		memcpy(this->cells(), other.cells(), sizeof(CellNode) * (GridSize + count));
	}

	~Cellphase() {
		free(cells());
	}

	void insert_outside_grid(CellNode::Index idx) {
		this->base[idx] = CellNode(idx, idx);
	}

	CellNode::Index insert(CellNode::Index idx, tl::VectorI2 pos) {
		assert((i32)idx < (i32)max_count);
		u32 cx = (u32)pos.x >> CellShift;
		u32 cy = ((u32)pos.y >> CellShift) << GridShift;

		auto new_cell = mask_neg(cx + cy);

		// sp <-> new_cell  ---->  sp <-> idx <-> new_cell
		auto sp = this->base[new_cell].next;
		this->base[sp].prev = idx;
		this->base[idx] = CellNode(sp, new_cell);
		this->base[new_cell].next = idx;
		
		return new_cell;
	}

	CellNode::Index update(CellNode::Index idx, tl::VectorI2 pos, CellNode::Index cur_cell) {
		u32 cx = (u32)pos.x >> CellShift;
		u32 cy = ((u32)pos.y >> CellShift) << GridShift;
		auto new_cell = mask_neg(cx + cy);

		if (cur_cell != new_cell) {

			// Unlink
			this->remove(idx);

			// Relink
			auto sp = this->base[new_cell].next;
			this->base[sp].prev = idx;
			this->base[idx] = CellNode(sp, new_cell);
			this->base[new_cell].next = idx;
		}

		return new_cell;
	}

	void remove(CellNode::Index idx) {
		// Unlink
		auto np = this->base[idx];
		assert(this->base[np.prev].next == idx);
		assert(this->base[np.next].prev == idx);
		this->base[np.prev].next = np.next;
		this->base[np.next].prev = np.prev;
	}

	AreaRange area(i32 x1, i32 y1, i32 x2, i32 y2) {
		AreaRange ar;

		u32 cx1 = (u32(x1) >> CellShift);
		u32 cy1 = (u32(y1) >> CellShift) << GridShift;
		u32 cx2 = (u32(x2) >> CellShift);
		u32 cy2 = (u32(y2) >> CellShift) << GridShift;

		auto first = mask_neg(cx1 + cy1);

		ar.n = first;
		ar.base = base;

		ar.pitch_minus_w = i32(GridWidth + cx1 - cx2 - 1);
		ar.xy_h_end = (cx2 + 1) & CellMask;
		ar.xy_v_end = mask_neg(cx1 + cy2 + GridWidth);

		return ar;
	}

	void swap_remove(CellNode::Index idx, CellNode::Index last_idx) {

		this->remove(idx);

		// There are two options. Either we do the self-link of [idx], or we do the (last_idx != idx) check.
		// The (last_idx != idx) branch should be well-predicted.
#if 0
		this->base[idx].next = idx;
		this->base[idx].prev = idx;

		if (true) {
#else
		if (last_idx != idx) {
#endif
			// Fix up neighbours of moved node
			auto last = this->base[last_idx];
			if (last.next == last_idx) {
			//if (false) {
				this->base[idx] = CellNode(idx, idx);
			} else {
				this->base[idx] = last;
			}

			assert(this->base[last.prev].next == last_idx);
			assert(this->base[last.next].prev == last_idx);
			this->base[last.prev].next = idx;
			this->base[last.next].prev = idx;
		}

#if !defined(NDEBUG) && 0
		for (u32 i = 0; i < GridSize; ++i) {
			assert(this->base[i].next != last_idx);
			assert(this->base[i].prev != last_idx);
		}
#endif
	}
};

}

#endif // LIERO_CELLPHASE_HPP
