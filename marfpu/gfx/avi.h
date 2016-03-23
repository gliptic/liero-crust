#ifndef UUID_76601971700C40DEC6596DB6552D1354
#define UUID_76601971700C40DEC6596DB6552D1354

#include "gfx.h"
#include "tl/std.h"
#include "tl/stream.h"
#include "tl/vector.h"

typedef struct avi_ientry {
	u32 flags, pos, len;
} avi_ientry;

typedef struct avi_stream
{
	//int audio_strm_length;
	int packet_count;
	int width, height;

	u64 orgstrh;
	
	u64 index_start;
	tl_vector index_entries; // avi_ientry
} avi_stream;

typedef struct avi_file_sink
{
	tl_byte_sink_pushable sink;

	u64 orgriff, orgmovi, orgavih;

	int width, height;
	
	tl_vector streams; // avi_stream
	
} avi_file_sink;

GFX_API void avi_test();

#endif // UUID_76601971700C40DEC6596DB6552D1354
