#include "string_set.hpp"
#include <tl/memory.h>

namespace hyp {

static u32 const default_string_set_loglen = 8;

string_set::string_set()
	: hshift(32 - default_string_set_loglen), keycount(0) {

	u32 len = 1 << default_string_set_loglen;

	//this->prof_hprobes = 0;
	//this->prof_getcount = 0;

	this->tab = (slot *)calloc(len, sizeof(slot));
}

string_set::~string_set() {
	free(this->tab);
}

u32 string_set::count() {
	u32 hmask = ((u32)-1) >> this->hshift;
	u32 count = 0;

	for (u32 p = 0; p <= hmask; ++p) {
		if (this->tab[p].h) {
			++count;
		}
	}

	return count;
}

#define HASH2IDX(h) ((h) >> hshift)
#define HTAB(x) (tab[x].h)
#define ITAB(x) (tab[x].i)
#define IS_EMPTY() (p_hash == 0)

u32 string_set::max_dtb() {
	u32 hmask = ((u32)-1) >> this->hshift;
	u32 max_dist = 0;

	for (u32 p = 0; p <= hmask; ++p) {
		if (this->tab[p].h) {
			
			u32 p_hash = HTAB(p);
			u32 p_dist = (p - HASH2IDX(p_hash)) & hmask;

			if (p_dist > max_dist) {
				max_dist = p_dist;
			}
		}
	}

	return max_dist;
}

static void resize(string_set* self);

static int insert(
	string_set* self,
	u32 hash, u32 str_offset, u8 const* str, u32 len,
	u32 hshift, string_set::slot* tab,
	u32* ret) {

	u32 hmask = ((u32)-1) >> hshift;
	u32 p = HASH2IDX(hash);
	u32 dist = 0;

	//++self->prof_getcount;

	for (;;) {
		u32 p_hash = HTAB(p);
		u32 p_dist = (p - HASH2IDX(p_hash)) & hmask;

		//++self->prof_hprobes;

		if (str && p_hash == hash) {
			u32 cand_offset = ITAB(p);

			u8 const* cand_ptr = self->source.begin() + cand_offset;
			u32 max_cand_len = (u32)(self->source.end() - cand_ptr);

			/*
			if (str_minlen <= 1 && len == 1) {
				if (max_cand_len >= 1 && *cand_ptr == *str) {
					*ret = cand_offset;
					return 1;
				}
			} else */
			if (max_cand_len >= len && !memcmp(cand_ptr, str, len)) {
				// Exists
				*ret = cand_offset;
				return 1;
			}
		}

		if (IS_EMPTY() || dist > p_dist) {
			u32 p_offset = ITAB(p);

			HTAB(p) = hash;
			ITAB(p) = str_offset;

			if (IS_EMPTY()) {
				// Slot was empty. We're done.
				return 0;
			}

			// Not empty so we need to push it down
			str_offset = p_offset;
			dist = p_dist;
			hash = p_hash;
			str = NULL; // Disable matching. No other entry in the table can be equal to the pushed out entry.
		}

		p = (p + 1) & hmask;
		++dist;
	}
}

static void resize(string_set* self) {
	u32 newhshift = self->hshift - 1;
	u32 newloglen = 32 - newhshift;
	u32 newlen = 1 << newloglen;

	string_set::slot *newtab = (string_set::slot *)calloc(newlen, sizeof(string_set::slot));
	string_set::slot *oldtab = self->tab;
	
	u32 oldhmask = ((u32)-1) >> self->hshift;

	for (u32 p = 0; p <= oldhmask; ++p) {
		u32 p_hash = oldtab[p].h;

		if (!IS_EMPTY()) {
			u32 oldi = oldtab[p].i;

			// len is not used when str == NULL
			insert(self, oldtab[p].h, oldi, NULL, 0, newhshift, newtab, NULL);
		}
	}

	self->tab = newtab;
	self->hshift = newhshift;
	free(oldtab);
}

bool string_set::get(u32 hash, u8 const* str, u32 len, u32* ret) {

	u32 prel_offset = (u32)(str - this->source.begin());

	int existing = insert(this, hash, prel_offset, str, len, this->hshift, this->tab, ret);

	if (existing) {
		return true;
	}

	u32 hmask = ((u32)-1) >> this->hshift;

	double log2 = 1.0 / log(2.0);

#if 1
	if (++this->keycount > hmask - (hmask >> 2)) {
		u32 dtb = max_dtb();
		double k = dtb / (log(this->keycount) * log2);
		double k2 = dtb / (log(hmask + 1) * log2);

		printf("Max: %d. dtb = %f * log2(keycount). dtb = %f * log2(bucketcount).\n", dtb, k, k2);
		resize(this);
	}
#endif

	*ret = prel_offset;
	return false;
}

#undef HASH2IDX
#undef ITAB
#undef HTAB
#undef IS_EMPTY

}