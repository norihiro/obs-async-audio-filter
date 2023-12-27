#pragma once

#include <inttypes.h>

typedef struct resampler_s resampler_t;

struct resampler_type
{
	const char *type_name;
	void *(*create)(resampler_t *rs);
	void (*destroy)(resampler_t *rs);
	uint32_t (*audio)(resampler_t *rs, float *data[], uint32_t frames, uint64_t *pts);
	void (*compensate)(resampler_t *rs, double compensation);
};

struct resampler_s
{
	const struct resampler_type *type_data;
	void *ctx;
};

/**
 * Create a resampler context
 */
resampler_t *resampler_create(const char *type_name);

/**
 * Destroy the resampler context
 *
 * @param rs  Context
 */
void resampler_destroy(resampler_t *rs);

/**
 * Process audio
 *
 * @param rs      Context
 * @param data    Pointer to input and output FLTP audio data
 * @param frames  Number of input frames
 * @param pts     Pointer to the timestamp, will be modified by this function.
 * @return Number of output frames
 */
uint32_t resampler_audio(resampler_t *rs, float *data[], uint32_t frames, uint64_t *pts);

/**
 * Set asynchronous compensation ratio
 *
 * @param rs            Context
 * @param compensation  Number of frames to add in a second
 */
void resampler_compensate(resampler_t *rs, double compensation);
