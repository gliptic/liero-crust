#ifndef LIERO_CELLPHASE_SLL_HPP
#define LIERO_CELLPHASE_SLL_HPP

#include <tl/std.h>
#include <tl/bits.h>
#include <tl/vec.hpp>
#include <tl/vector.hpp>

#define INTERLEAVED_REFS 0

namespace liero {

	using tl::narrow;

	struct CellNodeSll {
		typedef i16 Index;

		CellNodeSll(Index n) : next(n) {
		}

		Index next;
	};

	struct CellphaseSll {
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
			CellNodeSll::Index const* base;
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
		CellNodeSll::Index* nexts;
#endif
		u32 max_count;

		CellphaseSll(u32 max_count_init)
			: max_count(max_count_init) {

#if INTERLEAVED_REFS
			auto cells = (CellNode *)malloc(sizeof(CellNode) * (GridSize + max_count_init));
			this->base = cells + GridSize;
#else
			auto cells = (CellNodeSll::Index *)malloc(sizeof(CellNodeSll::Index) * (GridSize + max_count_init));
			this->nexts = cells + GridSize;
#endif

			this->clear();
		}

		static CellNodeSll::Index mask_neg(i32 n) {
			return CellNodeSll::Index(n | ~(CellMask | (CellMask << GridShift)));
		}

		static CellNodeSll::Index mask(i32 n) {
			return CellNodeSll::Index(n & (CellMask | (CellMask << GridShift)));
		}

#if INTERLEAVED_REFS
		inline CellNode* cells() {
			return base - GridSize;
		}

		inline CellNode const* cells() const {
			return base - GridSize;
		}
#else
		inline CellNodeSll::Index* cells() {
			return this->nexts - GridSize;
		}

		inline CellNodeSll::Index const* cells() const {
			return this->nexts - GridSize;
		}
#endif

		void copy_from(CellphaseSll const& other, usize count) {
			assert(max_count == other.max_count && count <= max_count);

#if INTERLEAVED_REFS
			memcpy(this->cells(), other.cells(), sizeof(CellNode) * (GridSize + count));
#else
			memcpy(this->nexts - GridSize, other.nexts - GridSize, sizeof(CellNodeSll::Index) * (GridSize + count));
#endif
		}

		~CellphaseSll() {
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

		void set(i32 idx, CellNodeSll::Index next) {
			this->nexts[idx] = next;
		}

		void set_next(i32 idx, CellNodeSll::Index next) {
			this->nexts[idx] = next;
		}

		CellNodeSll get(i32 idx) {
			return CellNodeSll(this->nexts[idx]);
		}

		CellNodeSll::Index get_next(i32 idx) {
			return this->nexts[idx];
		}

#endif

		void validate(i32 idx) {
			validate_idx(idx);
		}

		CellNodeSll::Index insert(CellNodeSll::Index idx, Vector2 pos) {
			assert((i32)idx < (i32)max_count);
			u32 cx = (u32)pos.x.raw() >> (16 + CellShift);
			u32 cy = ((u32)pos.y.raw() >> (16 + CellShift)) << GridShift;

			auto new_cell = mask_neg(cx + cy);

			auto sp = get_next(new_cell);
			set(idx, sp);
			set_next(new_cell, idx);

			assert(get_next(idx) == sp);

			return new_cell;
		}

		void clear() {
			for (u32 i = 0; i < GridSize; ++i) {
				auto next_index = mask_neg(i - GridSize + 1);
				set(i - GridSize, next_index);
			}
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
	};

}

#endif // LIERO_CELLPHASE_SLL_HPP
