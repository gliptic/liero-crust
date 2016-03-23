#include "avi.h"

#include "tl/riff.h"
#include <string.h>

static void avi_begin_avi(avi_file_sink* self, u32 riff_sign)
{
	self->orgriff = riff_push_riff_hdr(&self->sink, riff_sign, 0);
}

static u64 avi_begin_hdrl(avi_file_sink* self)
{
	return riff_push_list_hdr(&self->sink, RIFF_SIGN('h','d','r','l'), 0);
}

void avi_end_avi(avi_file_sink* self)
{
	riff_patch_hdr_len(&self->sink, self->orgriff);
}

typedef struct Struct_Avih {
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
} Struct_Avih;

typedef struct Struct_Strh {
     uint32_t fccType;
     uint32_t fccHandler;
     uint32_t dwFlags;
     uint16_t wPriority;
     uint16_t wLanguage;
     uint32_t dwInitialFrames;
     uint32_t dwScale;
     uint32_t dwRate;
     uint32_t dwStart;
     uint32_t dwLength;
     uint32_t dwSuggestedBufferSize;
     uint32_t dwQuality;
     uint32_t dwSampleSize;
     struct {
         int16_t left;
         int16_t top;
         int16_t right;
         int16_t bottom;
     } rcFrame;
} Struct_Strh;

typedef struct Struct_BmpHeader
{
  uint32_t biSize;
  int32_t  biWidth;
  int32_t  biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t  biXPelsPerMeter;
  int32_t  biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
  //uint32_t bmiColors[1];
} Struct_BmpHeader;

#define	AVIF_HASINDEX   0x00000010
#define	AVIF_MUSTUSEINDEX   0x00000020
#define	AVIF_ISINTERLEAVED   0x00000100
#define	AVIF_TRUSTCKTYPE   0x00000800
#define	AVIF_WASCAPTUREFILE   0x00010000
#define	AVIF_COPYRIGHTED   0x00020000
#define	AVI_MAX_RIFF_SIZE   0x40000000LL
#define	AVI_MASTER_INDEX_SIZE   256
#define	AVI_MAX_STREAM_COUNT   100
#define	AVIIF_INDEX   0x10

#define AVIIF_KEYFRAME 0x10

TL_STATIC_ASSERT(sizeof(Struct_Avih) == 14*4);
TL_STATIC_ASSERT(sizeof(Struct_Strh) == 14*4);
TL_STATIC_ASSERT(sizeof(Struct_BmpHeader) == 10*4);
TL_STATIC_ASSERT(sizeof(avi_ientry) == 3*4);

static void write_header(avi_file_sink* self)
{
	u64 orghdrl;
	Struct_Avih avih;

	int width, height;
	// TODO: Per-stream codec
	int framerate = 30;
	int codec_tag = 0;
	int bitrate;
	int buffer_size = 1024*1024 * 10;

	tl_vector_foreach(self->streams, avi_stream, s, {
		self->width = s->width;
		self->height = s->height;
		break;
	});

	width = self->width;
	height = self->height;
	bitrate = width * height * framerate * 24;

	avi_begin_avi(self, RIFF_SIGN('A','V','I',' '));
	orghdrl = avi_begin_hdrl(self);

	// TODO: Add appropiate tl_le32/tl_le16 calls

	self->orgavih = riff_push_hdr(&self->sink, RIFF_SIGN('a','v','i','h'), sizeof(avih));
	memset(&avih, 0, sizeof(avih));
	avih.dwMicroSecPerFrame = (uint32_t)(1000000ull * 1 / framerate); // TODO: Custom
	avih.dwMaxBytesPerSec = (bitrate / 8);
	//avih.dwPaddingGranularity = 0;
	avih.dwFlags = (AVIF_TRUSTCKTYPE | AVIF_HASINDEX | AVIF_ISINTERLEAVED);
	//avih.dwTotalFrames // Filled later
	//avih.dwInitialFrames = 0;
	avih.dwStreams = (u32)tl_vector_size(self->streams);
	avih.dwSuggestedBufferSize = buffer_size;
	avih.dwWidth = width;
	avih.dwHeight = height;

	tl_bs_pushn(&self->sink, (u8 const*)&avih, sizeof(avih));

	// Streams
	tl_vector_foreach(self->streams, avi_stream, s, {
		Struct_Strh strh;
		u64 orgstrl = riff_push_list_hdr(&self->sink, RIFF_SIGN('s','t','r','l'), 0);

		{
			memset(&strh, 0, sizeof(strh));

			strh.fccType = RIFF_SIGN('v','i','d','s');
			strh.fccHandler = codec_tag; // TODO: codec_tag
			//strl.dwFlags = 0;
			//strl.wPriority = 0;
			//strl.wLanguage = 0;
			//strl.dwInitialFrames = 0;
			strh.dwScale = 1; // TODO: Custom
			strh.dwRate = framerate;
			//strl.dwStart = 0;
			//strl.dwLength = 0; // TODO: Filled later, remember orgstrh for each stream
			strh.dwSuggestedBufferSize = buffer_size; // TODO: Smaller for audio?
			strh.dwQuality = 0xffffffff;
			//strh.dwSampleSize = 0; // TODO: For audio
			//strl.rcFrame.left = 0;
			//strl.rcFrame.top = 0;
			strh.rcFrame.right = width;
			strh.rcFrame.bottom = height;

			s->orgstrh = riff_push_hdr(&self->sink, RIFF_SIGN('s','t','r','h'), sizeof(strh));
			tl_bs_pushn(&self->sink, (u8 const*)&strh, sizeof(strh));
		}

		{
			Struct_BmpHeader bmph;

			memset(&bmph, 0, sizeof(bmph));
			bmph.biSize = sizeof(bmph); // TODO: Extra data?
			bmph.biWidth = width;
			bmph.biHeight = -height; // TODO: Positive when codec is set
			bmph.biPlanes = 1;
			bmph.biBitCount = 24; // TODO: Custom
			bmph.biCompression = codec_tag; // TODO
			bmph.biSizeImage = width * height * 3; // TODO
			//bmph.biXPelsPerMeter = 0;
			//bmph.biYPelsPerMeter = 0;
			//bmph.biClrUsed = 0;
			//bmph.biClrImportant = 0;

			riff_push_hdr(&self->sink, RIFF_SIGN('s','t','r','f'), sizeof(bmph));
			tl_bs_pushn(&self->sink, (u8 const*)&bmph, sizeof(bmph));
		}

		riff_patch_hdr_len(&self->sink, orgstrl);
	});

	riff_patch_hdr_len(&self->sink, orghdrl);

	{
		//u64 orginfo = riff_push_list_hdr(&self->sink, RIFF_SIGN('I','N','F','O'), 0);
		//riff_patch_hdr_len(&self->sink, orginfo);
	}

	self->orgmovi = riff_push_list_hdr(&self->sink, RIFF_SIGN('m','o','v','i'), 0);
}

static u32 tag_for_stream(avi_file_sink* self, int stream_index)
{
	uint32_t tag = RIFF_SIGN('0' + (stream_index/10), '0' + (stream_index%10), 'd', 'b'); // TODO: Depends on stream

	return tag;
}

static void write_packet(avi_file_sink* self, int stream_index, u8 const* data, u32 size)
{
	u32 tag = tag_for_stream(self, stream_index);
	avi_stream* s = tl_vector_idx(self->streams, avi_stream, stream_index);

	int flags = AVIIF_KEYFRAME; // TODO: Only put this on keyframes.
	// TODO: If size exceeds max RIFF size, begin an AVIX/movi chunk

	
	{
		avi_ientry entry;
		uint64_t offset = tl_bs_tell_sink(&self->sink) - self->orgmovi;
		assert(offset <= 0xffffffffull);
		// TODO: tl_le*
		entry.flags = flags;
		entry.pos = (uint32_t)offset;
		entry.len = size;
		tl_vector_pushback(s->index_entries, avi_ientry, entry);
	}

	tl_bs_push32_le(&self->sink, tag);
	tl_bs_push32_le(&self->sink, size);
	tl_bs_pushn(&self->sink, data, size);
	if(size & 1)
		tl_bs_push(&self->sink, 0);

	++s->packet_count;
}

static void write_index(avi_file_sink* self)
{
	u64 orgidx1 = riff_push_hdr(&self->sink, RIFF_SIGN('i','d','x','1'), 0);
	u32 entry_index[100];
	avi_ientry* next_entry;
	int next_stream;

	memset(entry_index, 0, sizeof(entry_index));

	while(1)
	{
		int index = 0;
		u32 tag;

		next_entry = NULL;
		
		tl_vector_foreach(self->streams, avi_stream, s, {
			avi_ientry* cand_entry;
			if(tl_vector_size(s->index_entries) <= entry_index[index])
				continue;

			cand_entry = tl_vector_idx(s->index_entries, avi_ientry, entry_index[index]);
			if(!next_entry || cand_entry->pos < next_entry->pos)
			{
				next_entry = cand_entry;
				next_stream = index;
			}
			++index;
		});

		if(!next_entry) break;

		tag = tag_for_stream(self, next_stream);

		tl_bs_push32_le(&self->sink, tag);
		tl_bs_pushn(&self->sink, (u8 const*)next_entry, sizeof(*next_entry));
		++entry_index[next_stream];
	}

	riff_patch_hdr_len(&self->sink, orgidx1);
}

static void write_counters(avi_file_sink* self)
{
	u64 org = tl_bs_tell_sink(&self->sink);
	int max_packet_count = 0;
	
	tl_vector_foreach(self->streams, avi_stream, s, {
		tl_bs_seek_sink(&self->sink, s->orgstrh + offsetof(Struct_Strh, dwLength));

		// TODO for audio: audio_strm_length / stream->block_align
		// For video, it's just the frame count
		tl_bs_push32_le(&self->sink, s->packet_count);
		if(s->packet_count > max_packet_count)
			max_packet_count = s->packet_count;
	});

	tl_bs_seek_sink(&self->sink, self->orgavih + offsetof(Struct_Avih, dwTotalFrames));
	tl_bs_push32_le(&self->sink, max_packet_count);

	tl_bs_seek_sink(&self->sink, org);
}

static void write_tail(avi_file_sink* self)
{
	riff_patch_hdr_len(&self->sink, self->orgmovi);

	write_index(self);
	write_counters(self);

	avi_end_avi(self);
}

void avi_test()
{
	avi_file_sink avi;
	uint8_t data[320 * 240 * 3];
	int i;

	int framerate = 30;
	int time = 5;

	tl_vector_new_empty(avi.streams);

	{
		avi_stream str;
		str.packet_count = 0;
		str.width = 320;
		str.height = 240;
		tl_vector_new_empty(str.index_entries);
		tl_vector_pushback(avi.streams, avi_stream, str);
	}

	tl_bs_file_sink(&avi.sink, "E:/test_write.avi");

	write_header(&avi);

	memset(data, 0, sizeof(data));

	for(i = 0; i < time*framerate; ++i)
	{
		memset(data, 0x7f, sizeof(data) * i / (time*framerate));
		write_packet(&avi, 0, data, sizeof(data));
	}
	
	write_tail(&avi);
	tl_bs_free_sink(&avi.sink);
}