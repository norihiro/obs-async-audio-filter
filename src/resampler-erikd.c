#include <inttypes.h>
#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "resampler.h"
#include "samplerate.h"

#define BUFFER_SIZE_MARGIN 8
#define BUFFER_SIZE_ALIGN 8

typedef DARRAY(float) float_array_t;

struct ctx_s
{
	double compensation;

	// resampler context
	SRC_STATE *src_state;
	uint32_t pts_offset;

	// Buffer
	float_array_t buffer1;
	float_array_t buffer2;
};

static void resampler_deinit(struct ctx_s *ctx);

static void *erikd_resampler_create(resampler_t *rs)
{
	UNUSED_PARAMETER(rs);
	struct ctx_s *ctx = bzalloc(sizeof(struct ctx_s));
	return ctx;
}

static void erikd_resampler_destroy(resampler_t *rs)
{
	struct ctx_s *ctx = rs->ctx;
	resampler_deinit(ctx);
	da_free(ctx->buffer1);
	da_free(ctx->buffer2);
	bfree(ctx);
}

static inline void resampler_init_if_necessary(resampler_t *rs, uint32_t channels)
{
	struct ctx_s *ctx = rs->ctx;

	if (ctx->src_state && (uint32_t)src_get_channels(ctx->src_state) == channels)
		return;

	resampler_deinit(ctx);

	static bool version_printed = false;
	if (!version_printed) {
		blog(LOG_INFO, "Initializing SRC instance of %s", src_get_version());
		version_printed = true;
	}

	int error = 0;
	ctx->src_state = src_new(SRC_SINC_BEST_QUALITY, channels, &error);
	ctx->pts_offset = 0;
	if (!ctx->src_state) {
		blog(LOG_ERROR, "Failed to initialize resampler: %s", src_strerror(error));
		return;
	}
}

static void resampler_deinit(struct ctx_s *ctx)
{
	if (ctx->src_state)
		src_delete(ctx->src_state);
	ctx->src_state = NULL;
}

static void planar_to_packed(float *dst, const float *src[], uint32_t channels, uint32_t frames)
{
	for (uint32_t c = 0; c < channels; c++) {
		float *ptr = dst + c;
		for (uint32_t f = 0; f < frames; f++) {
			*ptr = src[c][f];
			ptr += channels;
		}
	}
}

static void packed_to_planar(float *dst[], const float *src, uint32_t channels, uint32_t frames)
{
	for (uint32_t c = 0; c < channels; c++) {
		const float *ptr = src + c;
		for (uint32_t f = 0; f < frames; f++) {
			dst[c][f] = *ptr;
			ptr += channels;
		}
	}
}

static uint32_t erikd_resampler_audio(resampler_t *rs, float *data[], uint32_t frames, uint64_t *pts)
{
	struct ctx_s *ctx = rs->ctx;
	struct obs_audio_info oai;
	obs_get_audio_info(&oai);

	uint32_t channels = get_audio_channels(oai.speakers);

	resampler_init_if_necessary(rs, channels);

	uint32_t buffer_frames = (frames + BUFFER_SIZE_MARGIN + BUFFER_SIZE_ALIGN - 1) & ~(BUFFER_SIZE_ALIGN - 1);
	da_resize(ctx->buffer1, buffer_frames * channels);
	da_resize(ctx->buffer2, buffer_frames * channels);

	planar_to_packed(ctx->buffer1.array, (const float **)data, channels, frames);

	SRC_DATA src_data = {
		.data_in = ctx->buffer1.array,
		.data_out = ctx->buffer2.array,
		.input_frames = frames,
		.output_frames = frames + BUFFER_SIZE_MARGIN,
		.end_of_input = 0,
		.src_ratio = 1.0 + ctx->compensation / oai.samples_per_sec,
	};

	int ret = src_process(ctx->src_state, &src_data);
	if (ret) {
		blog(LOG_ERROR, "src_process returns an error %d", ret);
	}

	*pts -= ctx->pts_offset;

	if (!ctx->pts_offset) {
		long diff = src_data.input_frames - src_data.output_frames_gen;
		uint64_t ns = (uint64_t)diff * 1000000000ULL / oai.samples_per_sec;
		ctx->pts_offset = (uint32_t)ns;
	}

	for (uint32_t c = 0; c < channels; c++)
		data[c] = ctx->buffer1.array + c * buffer_frames;
	packed_to_planar(data, ctx->buffer2.array, channels, src_data.output_frames_gen);

	return src_data.output_frames_gen;
}

static void erikd_resampler_compensate(resampler_t *rs, double compensation)
{
	struct ctx_s *ctx = rs->ctx;
	ctx->compensation = compensation;
}

const struct resampler_type erikd_resampler_type = {
	.type_name = "libsamplerate",
	.create = erikd_resampler_create,
	.destroy = erikd_resampler_destroy,
	.audio = erikd_resampler_audio,
	.compensate = erikd_resampler_compensate,
};
