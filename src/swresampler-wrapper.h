#pragma once
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(4, 5, 100)
static inline uint64_t convert_speaker_layout(enum speaker_layout layout)
{
	switch (layout) {
	case SPEAKERS_UNKNOWN:
		return 0;
	case SPEAKERS_MONO:
		return AV_CH_LAYOUT_MONO;
	case SPEAKERS_STEREO:
		return AV_CH_LAYOUT_STEREO;
	case SPEAKERS_2POINT1:
		return AV_CH_LAYOUT_SURROUND;
	case SPEAKERS_4POINT0:
		return AV_CH_LAYOUT_4POINT0;
	case SPEAKERS_4POINT1:
		return AV_CH_LAYOUT_4POINT1;
	case SPEAKERS_5POINT1:
		return AV_CH_LAYOUT_5POINT1_BACK;
	case SPEAKERS_7POINT1:
		return AV_CH_LAYOUT_7POINT1;
	}

	/* shouldn't get here */
	return 0;
}

struct SwrContext *create_swr(uint32_t samples_per_sec, enum speaker_layout obs_layout)
{
	uint64_t ff_layout = convert_speaker_layout(obs_layout);
	return swr_alloc_set_opts(NULL, ff_layout, AV_SAMPLE_FMT_FLTP, samples_per_sec, ff_layout, AV_SAMPLE_FMT_FLTP,
				  samples_per_sec, 0, NULL);
}

#else // LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 100)

struct SwrContext *create_swr(uint32_t samples_per_sec, enum speaker_layout obs_layout)
{
	AVChannelLayout ff_layout;
	av_channel_layout_default(&ff_layout, get_audio_channels(obs_layout));
	struct SwrContext *context = NULL;
	if (swr_alloc_set_opts2(&context, &ff_layout, AV_SAMPLE_FMT_FLTP, samples_per_sec, &ff_layout,
				AV_SAMPLE_FMT_FLTP, samples_per_sec, 0, NULL) == 0)
		return context;
	return NULL;
}

#endif
