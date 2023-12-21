#include <inttypes.h>
#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "swresampler-wrapper.h"
#include "resampler.h"

// TODO: libsndfile/libsamplerate also looks good.

#define BUFFER_SIZE_MARGIN 4

typedef DARRAY(float) float_array_t;

struct resampler_s
{
	uint32_t samples_per_sec;
	uint32_t channels;

	// FFmpeg resampler
	struct SwrContext *swr;

	// Buffer
	float_array_t raw_buffer;
};

static void resampler_deinit(resampler_t *rs);

resampler_t *resampler_create()
{
	resampler_t *rs = bzalloc(sizeof(resampler_t));
	return rs;
}

void resampler_destroy(resampler_t *rs)
{
	resampler_deinit(rs);
	da_free(rs->raw_buffer);
	bfree(rs);
}

static inline bool check_formats(resampler_t *rs, struct obs_audio_info *oai)
{
	if (rs->samples_per_sec != oai->samples_per_sec)
		return false;

	if (rs->channels != get_audio_channels(oai->speakers))
		return false;

	return true;
}

static void resampler_init_if_necessary(resampler_t *rs)
{
	struct obs_audio_info oai;
	obs_get_audio_info(&oai);

	if (check_formats(rs, &oai))
		return;

	resampler_deinit(rs);

	rs->samples_per_sec = oai.samples_per_sec;
	rs->channels = get_audio_channels(oai.speakers);

	rs->swr = create_swr(oai.samples_per_sec, oai.speakers);
	int err_init = swr_init(rs->swr);
	if (err_init) {
		blog(LOG_ERROR, "swr_init failed %d", err_init);
		resampler_deinit(rs);
	}

	/* Activate the compensation from the beginning. */
	resampler_compensate(rs, 0, 1);
}

static void resampler_deinit(resampler_t *rs)
{
	if (rs->swr)
		swr_free(&rs->swr);
	rs->swr = NULL;
}

uint32_t resampler_audio(resampler_t *rs, float *data[], uint32_t frames, uint64_t *pts)
{
	resampler_init_if_necessary(rs);

	struct SwrContext *swr = rs->swr;

	int32_t delay0_ns = (int32_t)swr_get_delay(swr, 1000000000LL);
	uint32_t out_max_samples = frames + BUFFER_SIZE_MARGIN;

	da_resize(rs->raw_buffer, out_max_samples * rs->channels);
	float *out_data[MAX_AUDIO_CHANNELS] = {NULL};
	for (uint32_t i = 0; i < rs->channels; i++)
		out_data[i] = rs->raw_buffer.array + out_max_samples * i;
	int ret = swr_convert(swr, (uint8_t **)out_data, out_max_samples, (const uint8_t **)data, frames);

	if (ret < 0) {
		blog(LOG_ERROR, "swr_convert failed: %d", ret);
		return 0;
	}

	for (uint32_t i = 0; i < rs->channels; i++)
		data[i] = out_data[i];

	if (*pts >= (uint64_t)delay0_ns)
		*pts -= delay0_ns;
	else
		*pts = 0;

	return ret;
}

void resampler_compensate(resampler_t *rs, int delta, int distance)
{
	swr_set_compensation(rs->swr, delta, distance);
}
