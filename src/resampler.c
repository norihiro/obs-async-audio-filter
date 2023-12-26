#include <inttypes.h>
#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "resampler.h"

extern const struct resampler_type erikd_resampler_type;
extern const struct resampler_type ffmpeg_resampler_type;

static const struct resampler_type *types[] = {
#ifdef USE_ERIKD_LIBSAMPLERATE
	&erikd_resampler_type,
#endif
#ifdef USE_FFMPEG_SWRESAMPLE
	&ffmpeg_resampler_type,
#endif
	NULL,
};

static inline const struct resampler_type *find_type(const char *type_name)
{
	static_assert(sizeof(types) > sizeof(types[0]), "Need at least one valid entry.");

	if (!type_name)
		return types[0];

	for (int i = 0; types[i]; i++) {
		if (strcmp(type_name, types[i]->type_name) == 0)
			return types[i];
	}

	blog(LOG_WARNING, "resampler_create: Specified type '%s' was not found. Falling back to '%s'", type_name,
	     types[0]->type_name);
	return types[0];
}

resampler_t *resampler_create(const char *type_name)
{
	const struct resampler_type *t = find_type(type_name);

	resampler_t *rs = bzalloc(sizeof(resampler_t));
	rs->type_data = t;
	rs->ctx = t->create(rs);
	return rs;
}

void resampler_destroy(resampler_t *rs)
{
	rs->type_data->destroy(rs);
	bfree(rs);
}

uint32_t resampler_audio(resampler_t *rs, float *data[], uint32_t frames, uint64_t *pts)
{
	return rs->type_data->audio(rs, data, frames, pts);
}

void resampler_compensate(resampler_t *rs, double compensation)
{
	rs->type_data->compensate(rs, compensation);
}
