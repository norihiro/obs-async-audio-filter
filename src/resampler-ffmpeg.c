#include <inttypes.h>
#include <obs-module.h>
#include "plugin-macros.generated.h"
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#include "resampler.h"

#define BUFFER_SIZE_MARGIN 4

typedef DARRAY(float) float_array_t;

struct ff_resampler_s
{
	uint32_t samples_per_sec;
	uint32_t channels;

	// FFmpeg resampler
	struct SwrContext *swr;

	// Buffer
	float_array_t raw_buffer;
};

static void resampler_deinit(struct ff_resampler_s *rs);
static void ff_resampler_compensate(resampler_t *rs, double compensation);

static void *ff_resampler_create(resampler_t *rs)
{
	UNUSED_PARAMETER(rs);
	struct ff_resampler_s *ctx = bzalloc(sizeof(struct ff_resampler_s));
	return ctx;
}

static void ff_resampler_destroy(resampler_t *rs)
{
	struct ff_resampler_s *ctx = rs->ctx;
	resampler_deinit(ctx);
	da_free(ctx->raw_buffer);
	bfree(ctx);
}

static inline bool check_formats(struct ff_resampler_s *ctx, struct obs_audio_info *oai)
{
	if (ctx->samples_per_sec != oai->samples_per_sec)
		return false;

	if (ctx->channels != get_audio_channels(oai->speakers))
		return false;

	return true;
}

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

static struct SwrContext *create_swr(uint32_t samples_per_sec, enum speaker_layout obs_layout)
{
	uint64_t ff_layout = convert_speaker_layout(obs_layout);
	return swr_alloc_set_opts(NULL, ff_layout, AV_SAMPLE_FMT_FLTP, samples_per_sec, ff_layout, AV_SAMPLE_FMT_FLTP,
				  samples_per_sec, 0, NULL);
}

#else // LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 100)

static struct SwrContext *create_swr(uint32_t samples_per_sec, enum speaker_layout obs_layout)
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

static inline void resampler_init_if_necessary(resampler_t *rs)
{
	struct ff_resampler_s *ctx = rs->ctx;
	struct obs_audio_info oai;
	obs_get_audio_info(&oai);

	if (check_formats(ctx, &oai))
		return;

	resampler_deinit(ctx);

	ctx->samples_per_sec = oai.samples_per_sec;
	ctx->channels = get_audio_channels(oai.speakers);

	ctx->swr = create_swr(oai.samples_per_sec, oai.speakers);
	int err_init = swr_init(ctx->swr);
	if (err_init) {
		blog(LOG_ERROR, "swr_init failed %d", err_init);
		resampler_deinit(ctx);
	}

	/* Activate the compensation from the beginning. */
	ff_resampler_compensate(rs, 0);
}

static void resampler_deinit(struct ff_resampler_s *ctx)
{
	if (ctx->swr)
		swr_free(&ctx->swr);
	ctx->swr = NULL;
}

static uint32_t ff_resampler_audio(resampler_t *rs, float *data[], uint32_t frames, uint64_t *pts)
{
	struct ff_resampler_s *ctx = rs->ctx;
	resampler_init_if_necessary(rs);

	struct SwrContext *swr = ctx->swr;

	int64_t delay0_ns = swr_get_delay(swr, 1000000000LL);
	uint32_t out_max_samples = frames + BUFFER_SIZE_MARGIN;

	da_resize(ctx->raw_buffer, out_max_samples * ctx->channels);
	float *out_data[MAX_AUDIO_CHANNELS] = {NULL};
	for (uint32_t i = 0; i < ctx->channels; i++)
		out_data[i] = ctx->raw_buffer.array + out_max_samples * i;
	int ret = swr_convert(swr, (uint8_t **)out_data, out_max_samples, (const uint8_t **)data, frames);

	if (ret < 0) {
		blog(LOG_ERROR, "swr_convert failed: %d", ret);
		return 0;
	}

	for (uint32_t i = 0; i < ctx->channels; i++)
		data[i] = out_data[i];

	if ((int64_t)*pts >= delay0_ns)
		*pts -= delay0_ns;
	else
		*pts = 0;

	return ret;
}

static void ff_resampler_compensate(resampler_t *rs, double compensation)
{
	struct ff_resampler_s *ctx = rs->ctx;
	swr_set_compensation(ctx->swr, (int)(compensation * 256), 256 * ctx->samples_per_sec);
}

const struct resampler_type ffmpeg_resampler_type = {
	.type_name = "ffmpeg",
	.create = ff_resampler_create,
	.destroy = ff_resampler_destroy,
	.audio = ff_resampler_audio,
	.compensate = ff_resampler_compensate,
};
