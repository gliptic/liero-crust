#include "local_set.hpp"

namespace hyp {

static u32 const default_local_set_loglen = 8;

local_set::local_set()
	: hshift(32 - default_local_set_loglen), keycount(0) {

	u32 len = 1 << default_local_set_loglen;

	//this->prof_hprobes = 0;
	//this->prof_getcount = 0;

	this->tab = (slot *)calloc(len, sizeof(slot));
}

inline u32 hash_key(u64 x) {
	u32 v = (u32)((x >> 32) ^ x);
	return v * (v * 2 + 1);
}

#define HASH2IDX(h) ((h) >> hshift)
#define KTAB(x) (tab[x].key)
#define HTAB(x) (hash_key(KTAB(p)))
#define VTAB(x) (tab[x].value)
#define IS_EMPTY() (p_key == 0)

u32 local_set::max_dtb() {
	u32 hmask = ((u32)-1) >> this->hshift;
	u32 max_dist = 0;

	for (u32 p = 0; p <= hmask; ++p) {
		if (KTAB(p) != 0) {

			u32 p_hash = HTAB(p);
			u32 p_dist = (p - HASH2IDX(p_hash)) & hmask;

			if (p_dist > max_dist) {
				max_dist = p_dist;
			}
		}
	}

	return max_dist;
}

double local_set::avg_dtb() {
	u32 hmask = ((u32)-1) >> this->hshift;
	u32 count = 0;
	u64 dtb_sum = 0;

	for (u32 p = 0; p <= hmask; ++p) {
		if (KTAB(p) != 0) {
			++count;

			u32 p_hash = HTAB(p);
			u32 p_dist = (p - HASH2IDX(p_hash)) & hmask;

			dtb_sum += p_dist;
		}
	}

	return (double)dtb_sum / count;
}

static void resize(local_set* self);

static bool insert(
	local_set* self,
	u32 hash, u64 key, u64 value,
	u32 hshift, local_set::slot* tab,
	u64** ret) {

	TL_UNUSED(self);

	u32 hmask = ((u32)-1) >> hshift;
	u32 p = HASH2IDX(hash);
	u32 dist = 0;
	
	u64* r = 0;

	//++self->prof_getcount;

	for (;;) {
		
		//++self->prof_hprobes;

		auto p_key = KTAB(p);

		if (p_key == key) {
			// Exists
			*ret = &VTAB(p);
			return true;
		}

		u32 p_hash = hash_key(p_key);
		u32 p_dist = (p - HASH2IDX(p_hash)) & hmask;

		if (IS_EMPTY() || dist > p_dist) {
			if (!r) {
				r = &VTAB(p);
			}

			u64 p_value = VTAB(p);

			KTAB(p) = key;
			VTAB(p) = value;

			if (IS_EMPTY()) {
				// Slot was empty. We're done.
				*ret = r;
				return false;
			}

			// Not empty so we need to push it down
			key = p_key;
			value = p_value;
			dist = p_dist;
			hash = p_hash;
		}

		p = (p + 1) & hmask;
		++dist;
	}
}

static void resize(local_set* self) {
	u32 newhshift = self->hshift - 1;
	u32 newloglen = 32 - newhshift;
	u32 newlen = 1 << newloglen;

	local_set::slot *newtab = (local_set::slot *)calloc(newlen, sizeof(local_set::slot));
	local_set::slot *oldtab = self->tab;

	u32 oldhmask = ((u32)-1) >> self->hshift;

	for (u32 p = 0; p <= oldhmask; ++p) {
		u64 p_key = oldtab[p].key;

		if (!IS_EMPTY()) {
			u64 oldvalue = oldtab[p].value;
			u64* dummy;

			insert(self, hash_key(p_key), p_key, oldvalue, newhshift, newtab, &dummy);
		}
	}

	self->tab = newtab;
	self->hshift = newhshift;
	free(oldtab);
}

u64* local_set::get(u64 key) {
	u64* v;

	u32 hmask = ((u32)-1) >> hshift;
		
	if (!insert(this, hash_key(key), key, 0, this->hshift, this->tab, &v)) {
		if (++this->keycount > hmask - (hmask >> 2)) {
			resize(this);
			insert(this, hash_key(key), key, 0, this->hshift, this->tab, &v);
		}
	}

	return v;
}

u64 local_set::remove(u64 key) {
	u32 hash = hash_key(key);
	u32 hmask = ((u32)-1) >> hshift;
	u32 p = HASH2IDX(hash);
	u32 dist = 0;

	//++self->prof_getcount;

	for (;;) {

		//++self->prof_hprobes;

		auto p_key = KTAB(p);

		if (p_key == key) {
			u64 value = VTAB(p);
			for (;;) {
				u32 prev_p = p;
				p = (p + 1) & hmask;
				++dist;

				p_key = KTAB(p);
				u32 p_hash = hash_key(p_key);
				
				if (IS_EMPTY() || p == HASH2IDX(p_hash)) {
					// Chain is bridged. Just make sure previous slot is empty.
					KTAB(prev_p) = 0;
					--this->keycount;
					return value;
				}

				KTAB(prev_p) = p_key;
				VTAB(prev_p) = VTAB(p);
			}
		}

#if 1
		// TODO: This should never happen, so we should skip it.
		// However, the performance impact seems to be insignificant.
		u32 p_hash = hash_key(p_key);
		u32 p_dist = (p - HASH2IDX(p_hash)) & hmask;

		if (IS_EMPTY() || dist > p_dist) {
			return 0;
		}
#endif

		p = (p + 1) & hmask;
		++dist;
	}
}

#undef HASH2IDX
#undef KTAB
#undef HTAB
#undef VTAB
#undef IS_EMPTY

}