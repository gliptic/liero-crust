#include "sfx.hpp"

#include <windows.h>
#include <objbase.h>
#include <MMReg.h>
#include <tl/memory.h>

#define DIRECTSOUND_VERSION 0x0300
#include <dsound.h>

#if TL_MSVCPP
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "dsound.lib" )
#endif

#define CHECK(r) do { if(r) goto fail; } while(0)
#define CHECKB(r) do { if(!(r)) goto fail; } while(0)
#define CHECKR(r) do { err = (r); if(err) goto fail; } while(0)

namespace sfx {

HINSTANCE dsound_dll = NULL;
HRESULT (WINAPI *direct_sound_create)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN) = NULL;
HRESULT (WINAPI *direct_sound_enumerate_a)(LPDSENUMCALLBACKA, LPVOID) = NULL;

struct StreamWin32 : Stream {
	StreamWin32() {
		memset(this, 0, sizeof(StreamWin32));
	}

	IDirectSound* dsound;
	IDirectSoundBuffer* dsound_output_buffer;
	u32 frames_per_ds_buffer;
	i32 output_buffer_size_bytes; // signed on purpose
	DWORD output_buffer_write_offset_bytes;     /* last write position */
	MMRESULT timer_id;

	LARGE_INTEGER  perf_counter_ticks_per_buffer; /* counter ticks it should take to play a full buffer */
	LARGE_INTEGER  previous_play_time;
	UINT           previous_play_cursor;
	double         dsw_frames_written;
	//double         frames_played;
	i32            bytes_per_output_frame;
};

static Device device_create(LPGUID lpGUID, LPCTSTR name) {
	Device dev(tl::StringSlice((u8 const*)name, (u8 const*)name + strlen(name)));

	dev.guid_is_null = lpGUID == NULL;
	if (lpGUID != NULL) {
		*(LPGUID)&dev.guid = *lpGUID;
	}

	return std::move(dev);
}

static HRESULT gather_output_device_info(Device& dev) {
	IDirectSound* dsound = NULL;
	HRESULT err;
	GUID* guid = dev.guid_is_null ? NULL : (GUID *)&dev.guid;
	CHECKR(direct_sound_create(guid, &dsound, NULL));

	{
		DSCAPS caps;
		memset(&caps, 0, sizeof(caps));
		caps.dwSize = sizeof(caps);
		CHECKR(dsound->GetCaps(&caps));

		if (caps.dwFlags & DSCAPS_PRIMARYSTEREO)
			dev.max_out_channels = 2; // TODO: More channels may be supported
		else
			dev.max_out_channels = 1;

		if (caps.dwFlags & DSCAPS_CONTINUOUSRATE) {
			dev.default_sample_rate = caps.dwMaxSecondarySampleRate;

			// TODO: Normalize to a default sample rate
		} else if (caps.dwMinSecondarySampleRate == caps.dwMaxSecondarySampleRate) {
			if (caps.dwMinSecondarySampleRate == 0)
				dev.default_sample_rate = 44100;
			else
				dev.default_sample_rate = caps.dwMaxSecondarySampleRate;
		} else if ((caps.dwMinSecondarySampleRate < 1000) && (caps.dwMaxSecondarySampleRate > 50000)) {
			dev.default_sample_rate = 44100;
		} else {
			dev.default_sample_rate = caps.dwMaxSecondarySampleRate;
		}
	}

	return 0;

fail:
	if (dsound) dsound->Release();
	return err;
}

static BOOL CALLBACK collect_guids(LPGUID lpGUID, LPCSTR lpszDesc, LPCSTR /*lpszDrvName*/, LPVOID lpContext) {
	Context& ctx = *(Context *)lpContext;

	Device dev = device_create(lpGUID, lpszDesc);
	if (!gather_output_device_info(dev)) {
		ctx.devices.push_back(std::move(dev));
	}

	return 1;
}

Context Context::create() {
	HRESULT hr;
	hr = CoInitialize(NULL);

	if (!dsound_dll) {
		dsound_dll = LoadLibrary("dsound.dll");
		if (dsound_dll) {
			direct_sound_create = (HRESULT (WINAPI *)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN))GetProcAddress(dsound_dll, "DirectSoundCreate");
			direct_sound_enumerate_a = (HRESULT (WINAPI *)(LPDSENUMCALLBACKA, LPVOID))GetProcAddress(dsound_dll, "DirectSoundEnumerateA");
		}
	}

	Context ctx;

	hr = direct_sound_enumerate_a(collect_guids, &ctx);

	return std::move(ctx);
}


static GUID dataFormatSubtypePcm = 
	{ (USHORT)(WAVE_FORMAT_PCM), 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };

static void waveformatex_init(
	WAVEFORMATEXTENSIBLE* waveFormat,
	int numChannels,
	double sampleRate,
	DWORD channelMask,
	int extensible) {

	WAVEFORMATEX *waveFormatEx = (WAVEFORMATEX*)waveFormat;
	u32 bytesPerSample = 2;
	u32 bytesPerFrame = numChannels * bytesPerSample;

	waveFormatEx->wFormatTag = extensible ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
	waveFormatEx->nChannels = (WORD)numChannels;
	waveFormatEx->nSamplesPerSec = (DWORD)sampleRate;
	waveFormatEx->nAvgBytesPerSec = waveFormatEx->nSamplesPerSec * bytesPerFrame;
	waveFormatEx->nBlockAlign = (WORD)bytesPerFrame;
	waveFormatEx->wBitsPerSample = (WORD)(bytesPerSample * 8);
	waveFormatEx->cbSize = extensible ? 22 : 0;

	if(extensible)
	{
		waveFormat->Samples.wValidBitsPerSample = waveFormatEx->wBitsPerSample;
		waveFormat->dwChannelMask = channelMask;
		waveFormat->SubFormat = dataFormatSubtypePcm;
	}
}

#define PAWIN_SPEAKER_FRONT_LEFT			((u32)0x1)
#define PAWIN_SPEAKER_FRONT_RIGHT			((u32)0x2)
#define PAWIN_SPEAKER_FRONT_CENTER			((u32)0x4)
#define PAWIN_SPEAKER_LOW_FREQUENCY			((u32)0x8)
#define PAWIN_SPEAKER_BACK_LEFT				((u32)0x10)
#define PAWIN_SPEAKER_BACK_RIGHT			((u32)0x20)
#define PAWIN_SPEAKER_FRONT_LEFT_OF_CENTER	((u32)0x40)
#define PAWIN_SPEAKER_FRONT_RIGHT_OF_CENTER	((u32)0x80)
#define PAWIN_SPEAKER_BACK_CENTER			((u32)0x100)
#define PAWIN_SPEAKER_SIDE_LEFT				((u32)0x200)
#define PAWIN_SPEAKER_SIDE_RIGHT			((u32)0x400)
#define PAWIN_SPEAKER_TOP_CENTER			((u32)0x800)
#define PAWIN_SPEAKER_TOP_FRONT_LEFT		((u32)0x1000)
#define PAWIN_SPEAKER_TOP_FRONT_CENTER		((u32)0x2000)
#define PAWIN_SPEAKER_TOP_FRONT_RIGHT		((u32)0x4000)
#define PAWIN_SPEAKER_TOP_BACK_LEFT			((u32)0x8000)
#define PAWIN_SPEAKER_TOP_BACK_CENTER		((u32)0x10000)
#define PAWIN_SPEAKER_TOP_BACK_RIGHT		((u32)0x20000)

/* Bit mask locations reserved for future use */
#define PAWIN_SPEAKER_RESERVED					((u32)0x7FFC0000)

/* Used to specify that any possible permutation of speaker configurations */
#define PAWIN_SPEAKER_ALL						((u32)0x80000000)

/* DirectSound Speaker Config */
#define PAWIN_SPEAKER_DIRECTOUT					0
#define PAWIN_SPEAKER_MONO				(PAWIN_SPEAKER_FRONT_CENTER)
#define PAWIN_SPEAKER_STEREO			(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT)
#define PAWIN_SPEAKER_QUAD				(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT | \
										PAWIN_SPEAKER_BACK_LEFT  | PAWIN_SPEAKER_BACK_RIGHT)
#define PAWIN_SPEAKER_SURROUND			(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT | \
										PAWIN_SPEAKER_FRONT_CENTER | PAWIN_SPEAKER_BACK_CENTER)
#define PAWIN_SPEAKER_5POINT1			(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT | \
										PAWIN_SPEAKER_FRONT_CENTER | PAWIN_SPEAKER_LOW_FREQUENCY | \
										PAWIN_SPEAKER_BACK_LEFT  | PAWIN_SPEAKER_BACK_RIGHT)
#define PAWIN_SPEAKER_7POINT1			(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT | \
										PAWIN_SPEAKER_FRONT_CENTER | PAWIN_SPEAKER_LOW_FREQUENCY | \
										PAWIN_SPEAKER_BACK_LEFT | PAWIN_SPEAKER_BACK_RIGHT | \
										PAWIN_SPEAKER_FRONT_LEFT_OF_CENTER | PAWIN_SPEAKER_FRONT_RIGHT_OF_CENTER)
#define PAWIN_SPEAKER_5POINT1_SURROUND	(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT | \
										PAWIN_SPEAKER_FRONT_CENTER | PAWIN_SPEAKER_LOW_FREQUENCY | \
										PAWIN_SPEAKER_SIDE_LEFT  | PAWIN_SPEAKER_SIDE_RIGHT)
#define PAWIN_SPEAKER_7POINT1_SURROUND	(PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_RIGHT | \
										PAWIN_SPEAKER_FRONT_CENTER | PAWIN_SPEAKER_LOW_FREQUENCY | \
										PAWIN_SPEAKER_BACK_LEFT | PAWIN_SPEAKER_BACK_RIGHT | \
										PAWIN_SPEAKER_SIDE_LEFT | PAWIN_SPEAKER_SIDE_RIGHT)

static u32 default_channel_mask(int numChannels) {

	switch( numChannels ){
		case 1: return PAWIN_SPEAKER_MONO;
		case 2: return PAWIN_SPEAKER_STEREO; 
		case 3: return PAWIN_SPEAKER_FRONT_LEFT | PAWIN_SPEAKER_FRONT_CENTER | PAWIN_SPEAKER_FRONT_RIGHT;
		case 4: return PAWIN_SPEAKER_QUAD;
		case 5: return PAWIN_SPEAKER_QUAD | PAWIN_SPEAKER_FRONT_CENTER;
		case 6: return PAWIN_SPEAKER_5POINT1; 
		/* case 7: */
		case 8: return PAWIN_SPEAKER_7POINT1;
	}

	return PAWIN_SPEAKER_DIRECTOUT;
}

//#define PA_LATENCY_ENV_NAME  ("PA_MIN_LATENCY_MSEC")
//#define PA_ENV_BUF_SIZE  (32)

DWORD ds_get_min_latency_frames(DWORD sampleRate) {
#if 0
	char      envbuf[PA_ENV_BUF_SIZE];
	DWORD     hresult;
	int       minLatencyMsec = 0;

	/* Let user determine minimal latency by setting environment variable. */
	hresult = GetEnvironmentVariable( PA_LATENCY_ENV_NAME, envbuf, PA_ENV_BUF_SIZE );
	if( (hresult > 0) && (hresult < PA_ENV_BUF_SIZE) )
	{
		minLatencyMsec = atoi( envbuf );
	}
	else
#endif

	int minLatencyMsec = 50; // TODO: Decide based on OS version
	return minLatencyMsec * sampleRate / 1000;
}

std::unique_ptr<Stream> Context::open(FillFunc fill, void* user_data) {

	HRESULT err;
	IDirectSoundBuffer* primary_buffer = NULL;
	IDirectSoundBuffer* dsound_output_buffer = NULL;

	std::unique_ptr<StreamWin32> str(new StreamWin32());
	Device& dev = this->devices[0];

	GUID * guid = dev.guid_is_null ? NULL : (GUID *)&dev.guid;
	IDirectSound* dsound = NULL;

	CHECKR(direct_sound_create(guid, &dsound, NULL));
	str->dsound = dsound;
	
	{
		// Buffer
		DWORD bytesPerSample = 2;
		DWORD nChannels = 2;
		DWORD nFrameRate = 44100;
		DWORD frames_per_ds_buffer = ds_get_min_latency_frames(nFrameRate);
		DWORD bytes_per_output_frame = nChannels * bytesPerSample;
		DWORD bytes_per_direct_sound_buffer = frames_per_ds_buffer * bytes_per_output_frame;
		DWORD channelMask = default_channel_mask(nChannels);
		
		DWORD bytesPerBuffer = bytes_per_direct_sound_buffer;
		DWORD outputBufferSizeBytes = bytesPerBuffer;
		
		LARGE_INTEGER counterFrequency;

		str->owner = this;
		str->sample_rate = nFrameRate;
		str->stream_pos = 0;
		str->buffer = (i16 *)malloc(outputBufferSizeBytes); // TODO: Make sure it is freed
		str->fill = fill;
		str->ud = user_data;
		str->frames_per_ds_buffer = frames_per_ds_buffer;
		str->output_buffer_size_bytes = outputBufferSizeBytes;
		str->bytes_per_output_frame = bytes_per_output_frame;

		HWND wnd = GetDesktopWindow();
		CHECKR(dsound->SetCooperativeLevel(wnd, DSSCL_EXCLUSIVE));

		DSBUFFERDESC primaryDesc, secondaryDesc;
		memset(&primaryDesc, 0, sizeof(DSBUFFERDESC));
		primaryDesc.dwSize        = sizeof(DSBUFFERDESC);
		primaryDesc.dwFlags       = DSBCAPS_PRIMARYBUFFER; // all panning, mixing, etc done by synth
		primaryDesc.dwBufferBytes = 0;
		primaryDesc.lpwfxFormat   = NULL;

		// Create the buffer
		CHECKR(dsound->CreateSoundBuffer(&primaryDesc, &primary_buffer, NULL));

		WAVEFORMATEXTENSIBLE waveFormat;
		waveformatex_init(&waveFormat, nChannels, nFrameRate, channelMask, 1);

		if (primary_buffer->SetFormat((WAVEFORMATEX *)&waveFormat) != DS_OK) {
			waveformatex_init(&waveFormat, nChannels, nFrameRate, channelMask, 0);
			CHECKR(primary_buffer->SetFormat((WAVEFORMATEX *)&waveFormat));
		}

		// ----------------------------------------------------------------------
		// Setup the secondary buffer description
		memset(&secondaryDesc, 0, sizeof(DSBUFFERDESC));
		secondaryDesc.dwSize = sizeof(DSBUFFERDESC);
		secondaryDesc.dwFlags =  DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
		secondaryDesc.dwBufferBytes = bytes_per_direct_sound_buffer;
		secondaryDesc.lpwfxFormat = (WAVEFORMATEX*)&waveFormat;

		// Create the secondary buffer
		CHECKR(dsound->CreateSoundBuffer(&secondaryDesc, &dsound_output_buffer, NULL));
		str->dsound_output_buffer = dsound_output_buffer;

		u8* dsBuffData;
		DWORD dwDataLen;
		CHECKR(dsound_output_buffer->Lock(0, outputBufferSizeBytes, (void **)&dsBuffData, &dwDataLen, NULL, 0, 0));

		memset(dsBuffData, 0, dwDataLen);

		CHECKR(dsound_output_buffer->Unlock(dsBuffData, dwDataLen, NULL, 0));

		QueryPerformanceFrequency(&counterFrequency);
		int framesInBuffer = bytesPerBuffer / (nChannels * bytesPerSample);
		str->perf_counter_ticks_per_buffer.QuadPart = (counterFrequency.QuadPart * framesInBuffer) / nFrameRate;

		// Let DSound set the starting write position because if we set it to zero, it looks like the
		// buffer is full to begin with. This causes a long pause before sound starts when using large buffers.
		DWORD play_cursor;
		CHECKR(dsound_output_buffer->GetCurrentPosition(&play_cursor, &str->output_buffer_write_offset_bytes));
		str->dsw_frames_written = str->output_buffer_write_offset_bytes / bytes_per_output_frame;
	}

	return std::move(str);

fail:
	return std::unique_ptr<Stream>();
}

u32 Stream::stop() {
	HRESULT err;
	StreamWin32* self = (StreamWin32 *)this;

	if (self->output_is_running) {
		CHECKR(self->dsound_output_buffer->Stop());

		self->output_is_running = false;
		TL_WRITE_SYNC(); // Make sure outputIsRunning is written before event is killed.
		timeKillEvent(self->timer_id);

		// TODO: Wait until processing is actually done. If we kill the timer while
		// an event is being processed we must wait until it's done to be safe.
	}

	return 0;

fail:
	return 0xffffffff;
}

Stream::~Stream() {

	StreamWin32* self = (StreamWin32 *)this;

	self->stop();

	free(self->buffer);
	if (self->dsound_output_buffer) self->dsound_output_buffer->Release();
	if (self->dsound) self->dsound->Release();
}

static i32 query_output_space(StreamWin32* str) {
	HRESULT err = DS_OK;
	DWORD play_cursor;
	DWORD write_cursor;

	// Query to see how much room is in buffer.
	CHECKR(str->dsound_output_buffer->GetCurrentPosition(&play_cursor, &write_cursor));

	// Determine size of gap between playIndex and WriteIndex that we cannot write into.
	i32 play_write_gap = write_cursor - play_cursor;
	if (play_write_gap < 0)
		play_write_gap += str->output_buffer_size_bytes; // unwrap

	/* DirectSound doesn't have a large enough play_cursor so we cannot detect wrap-around. */
	/* Attempt to detect play_cursor wrap-around and correct it. */

	if (str->output_is_running) {
		/* How much time has elapsed since last check. */
		LARGE_INTEGER currentTime;
		//LARGE_INTEGER elapsedTime;

		QueryPerformanceCounter(&currentTime);
		i64 elapsedTime = currentTime.QuadPart - str->previous_play_time.QuadPart;
		str->previous_play_time = currentTime;
		/* How many bytes does DirectSound say have been played. */
		i32 bytes_played = play_cursor - str->previous_play_cursor;
		if (bytes_played < 0) bytes_played += str->output_buffer_size_bytes; // unwrap
		str->previous_play_cursor = play_cursor;
		/* Calculate how many bytes we would have expected to been played by now. */
		i32 bytes_expected = i32((elapsedTime * str->output_buffer_size_bytes) / str->perf_counter_ticks_per_buffer.QuadPart);
		i32 buffers_wrapped = (bytes_expected - bytes_played) / str->output_buffer_size_bytes;

		if (buffers_wrapped > 0) {
			play_cursor += buffers_wrapped * str->output_buffer_size_bytes;
			bytes_played += buffers_wrapped * str->output_buffer_size_bytes;
		}
		/* Maintain frame output cursor. */
		//str->frames_played += (bytes_played / str->bytes_per_output_frame);
	}

	i32 num_bytes_empty = play_cursor - str->output_buffer_write_offset_bytes;
	if (num_bytes_empty < 0)
		num_bytes_empty += str->output_buffer_size_bytes; // unwrap offset

	/* Have we underflowed? */
	if (num_bytes_empty > str->output_buffer_size_bytes - play_write_gap) {
		if (str->output_is_running)
			str->output_underflow_count += 1;
		str->output_buffer_write_offset_bytes = write_cursor;
		num_bytes_empty = str->output_buffer_size_bytes - play_write_gap;
	}

	return num_bytes_empty;
fail:
	return 0;
}


u64 Context::get_time() {
	return tl_get_ticks();
}

static void CALLBACK process_stream(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD dw1, DWORD dw2) {

	HRESULT err;

	// TODO double outputBufferDacTime;
	// TODO double currentTime;
	// TODO double outputLatency = 0.0;
	
	LPBYTE lpOutBuf1 = NULL;
	LPBYTE lpOutBuf2 = NULL;
	DWORD  dwOutSize1 = 0;
	DWORD  dwOutSize2 = 0;
	StreamWin32* str = (StreamWin32 *)dwUser;

	if (!str->output_is_running)
		return;

	(void)uID;
	(void)uMsg;
	(void)dw1;
	(void)dw2;

	i32 bytes_empty = query_output_space(str);

	i32 frames_to_xfer = bytes_empty / str->bytes_per_output_frame;
	// i32 num_out_frames_ready = frames_to_xfer;
	i32 bytes_to_xfer = frames_to_xfer * str->bytes_per_output_frame;

	CHECKR(str->dsound_output_buffer->Lock(str->output_buffer_write_offset_bytes, bytes_to_xfer,
		(void **)&lpOutBuf1, &dwOutSize1,
		(void **)&lpOutBuf2, &dwOutSize2, 0));

	i32 num_frames = (dwOutSize1 + dwOutSize2) / str->bytes_per_output_frame;
	// TODO currentTime = str->owner->get_time();
	// TODO outputBufferDacTime = currentTime + outputLatency;

	// TODO: Make sure underflows are counted even if no data are produced for them.
	
	if(str->fill) {
		str->fill(*str, str->stream_pos, num_frames);
		memcpy(lpOutBuf1, (char*)str->buffer, dwOutSize1);
		memcpy(lpOutBuf2, (char*)str->buffer + dwOutSize1, dwOutSize2);
	} else {
		memset(lpOutBuf1, 0, dwOutSize1);
		memset(lpOutBuf2, 0, dwOutSize2);
	}

	str->dsound_output_buffer->Unlock(lpOutBuf1, dwOutSize1, lpOutBuf2, dwOutSize2);

	str->stream_pos += num_frames;
	TL_WRITE_SYNC();

	i32 bytes_processed = num_frames * str->bytes_per_output_frame;
	str->output_buffer_write_offset_bytes = (str->output_buffer_write_offset_bytes + bytes_processed) % str->output_buffer_size_bytes;
	str->dsw_frames_written += num_frames;

	str->successful_calls += 1;

fail: ;
	// TODO: Remember error?
	
}

u32 Stream::start() {
	HRESULT err;
	StreamWin32* self = (StreamWin32 *)this;
	if(self->output_is_running)
		return 0; // Already running

	QueryPerformanceCounter(&self->previous_play_time);
	//self->frames_played = 0.0;
	CHECKR(self->dsound_output_buffer->SetCurrentPosition(0));
	
	{
		u32 framesPerWakeup = self->frames_per_ds_buffer / 4;
		u32 msecPerWakeup = 1000 * framesPerWakeup / self->sample_rate;
		if (msecPerWakeup < 10) msecPerWakeup = 10;
		else if (msecPerWakeup > 100) msecPerWakeup = 100;
		u32 resolution = msecPerWakeup / 4;

		self->timer_id = timeSetEvent(msecPerWakeup, resolution, (LPTIMECALLBACK)process_stream,
											 (DWORD_PTR)self, TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
	}

	CHECKR(self->dsound_output_buffer->Play(0, 0, DSBPLAY_LOOPING));
	self->output_is_running = true;

	return 0;

fail:
	return 0xffffffff;
}

}
