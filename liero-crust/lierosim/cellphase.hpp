#ifndef LIERO_CELLPHASE_HPP
#define LIERO_CELLPHASE_HPP

#include <tl/std.h>
#include <tl/bits.h>
#include <tl/vec.hpp>
#include <tl/vector.hpp>

namespace liero {

using tl::narrow;

struct CellNode {
	typedef u16 Index;

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
		u32 xbeg;
		u32 x, y;
		u32 xend, yend;
		CellNode::Index n;

		bool next(CellNode::Index& ret) {
			while (true) {
				n = base[n].next;
				if (n >= GridSize) {
					ret = n - GridSize;
					return true;
				}

				if (x == xend) {
					if (y == yend)
						return false;
					y += GridWidth;
					x = xbeg;
				}
				
				++x;

				n = CellNode::Index((x + y) & (CellMask | (CellMask << GridShift)));
			}
		}
	};

	// TODO: If max_count is limited to (2^16 - GridSize) anyway, it's probably better to allocate inline
	CellNode* cells;

	Cellphase(u32 max_count) {
		this->cells = (CellNode *)malloc(sizeof(CellNode) * (GridSize + max_count));
		for (u32 i = 0; i < GridSize + max_count; ++i) {
			cells[i] = CellNode(narrow<CellNode::Index>(i), narrow<CellNode::Index>(i));
		}
	}

	~Cellphase() {
		free(cells);
	}

	CellNode::Index insert(CellNode::Index idx_, tl::VectorI2 pos) {
		u32 cx = (u32)pos.x >> CellShift;
		u32 cy = ((u32)pos.y >> CellShift) << GridShift;
		auto new_cell = CellNode::Index((cx + cy) & (CellMask | (CellMask << GridShift)));
		auto idx = CellNode::Index(idx_ + GridSize);

		auto sp = this->cells[new_cell].prev;

		// sp <-> new_cell  ---->  sp <-> idx <-> new_cell
		this->cells[sp].next = idx;
		this->cells[idx] = CellNode(new_cell, sp);
		this->cells[new_cell].prev = idx;
		return new_cell;
	}

	CellNode::Index update(CellNode::Index idx_, tl::VectorI2 pos, CellNode::Index cur_cell) {
		u32 cx = (u32)pos.x >> CellShift;
		u32 cy = ((u32)pos.y >> CellShift) << GridShift;
		auto new_cell = CellNode::Index((cx + cy) & (CellMask | (CellMask << GridShift)));
		auto idx = CellNode::Index(idx_ + GridSize);

		if (cur_cell != new_cell) {
			// Unlink
			CellNode np = this->cells[idx];
			this->cells[np.prev].next = np.next;
			this->cells[np.next].prev = np.prev;

			auto sp = this->cells[new_cell].prev;

			// Relink
			this->cells[sp].next = idx;
			this->cells[idx] = CellNode(new_cell, sp);
			this->cells[new_cell].prev = idx;
		}

		return new_cell;
	}

	AreaRange area(i32 x1, i32 y1, i32 x2, i32 y2) {
		AreaRange ar;

		u32 cx1 = u32(x1 >> CellShift);
		u32 cy1 = u32(y1 >> CellShift) << GridShift;
		u32 cx2 = u32(x2 >> CellShift);
		u32 cy2 = u32(y2 >> CellShift) << GridShift;

		auto first = CellNode::Index((cx1 + cy1) & (CellMask | (CellMask << GridShift)));

		ar.n = first;
		ar.base = cells;
		ar.x = cx1;
		ar.xbeg = cx1 - 1;
		ar.y = cy1;
		ar.xend = cx2;
		ar.yend = cy2;
		return ar;
	}

	void swap_remove(CellNode::Index idx_, CellNode::Index last_idx_) {
		auto idx = CellNode::Index(idx_ + GridSize);
		auto last_idx = CellNode::Index(last_idx_ + GridSize);

		{
			auto np = this->cells[idx];
			assert(this->cells[np.prev].next == idx);
			assert(this->cells[np.next].prev == idx);
			this->cells[np.prev].next = np.next;
			this->cells[np.next].prev = np.prev;
		}

		// There are two options, either we do the self-link of [idx], or we do the (last_idx != idx) check.
		// The (last_idx != idx) branch should be well-predicted.
#if 1
		this->cells[idx].next = idx;
		this->cells[idx].prev = idx;

		if (true) {
#else
		if (last_idx != idx) {
#endif
			// Fix up neighbours of moved node
			auto last = this->cells[last_idx];
			this->cells[idx] = last;

			assert(this->cells[last.prev].next == last_idx);
			assert(this->cells[last.next].prev == last_idx);
			this->cells[last.prev].next = idx;
			this->cells[last.next].prev = idx;
		}


#if !defined(NDEBUG) && 0
		for (u32 i = 0; i < GridSize; ++i) {
			assert(this->cells[i].next != last_idx);
			assert(this->cells[i].prev != last_idx);
		}
#endif
	}
};

}

#endif // LIERO_CELLPHASE_HPP
