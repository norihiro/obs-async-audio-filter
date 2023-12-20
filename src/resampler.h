#pragma once

#include <inttypes.h>

typedef struct resampler_s resampler_t;

/**
 * Create a resampler context
 */
resampler_t *resampler_create();

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
 * @param rs        Context
 * @param delta     Number of frames to add
 * @param distance  Number of frames the compensation will continue
 */
void resampler_compensate(resampler_t *rs, int delta, int distance);
