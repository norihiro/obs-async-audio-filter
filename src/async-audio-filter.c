#include <inttypes.h>
#include <obs-module.h>
#include <util/platform.h>
#include "plugin-macros.generated.h"
#include "lag_lead_filter.h"
#include "resampler.h"

#define STARTUP_TIMEOUT_NS (2 * 1000LL * 1000LL * 1000LL)
#define LOCKING_TIME_NS (1 * 1000LL * 1000LL * 1000LL)
#define TS_SMOOTHING_THRESHOLD 70000000LL

#define CASSERT(predicate)                                    \
	do {                                                  \
		typedef char assertion_[2 * !!(predicate)-1]; \
		assertion_ *unused;                           \
		(void)unused;                                 \
	} while (0)

#define FLTP(audio_data, plane, frame) (((float *)(audio_data)[plane])[frame])

enum state_e {
	state_start = 0,
	state_locking,
	state_locked,
};

struct source_s
{
	obs_source_t *context;

	// properties
	bool use_obs_time;
	int verbosity;

	// internal data
#ifndef USE_NEXT_AUDIO_TS
	enum state_e state;
	uint64_t audio_ns;
	int audio_ns_rem;
#endif

	struct lag_lead_filter lf;

	int64_t error_sum_ns;
	int error_cnt;

	uint32_t cnt_untouched_frames;

	struct obs_audio_data audio_buffer;

	resampler_t *rs;

#ifdef USE_NEXT_AUDIO_TS
	bool callback_added;
	obs_source_t *parent;
	uint64_t expected_audio_ts;
	int64_t last_error_ns;
#endif // USE_NEXT_AUDIO_TS
};

static const char *get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("Asynchronous Audio Filter");
}

static obs_properties_t *get_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_bool(props, "use_obs_time", obs_module_text("Use OBS time instead of source time"));
	obs_properties_add_int(props, "verbosity", obs_module_text("Verbosity"), 0, 2, 1);

	return props;
}

static void get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "verbosity", 1);
}

static void update(void *data, obs_data_t *settings)
{
	struct source_s *s = data;

	s->use_obs_time = obs_data_get_bool(settings, "use_obs_time");
	s->verbosity = (int)obs_data_get_int(settings, "verbosity");

	lag_lead_filter_update(&s->lf);
}

#ifndef USE_NEXT_AUDIO_TS
static void enter_state_locking(struct source_s *s)
{
	blog(LOG_INFO, "locking the audio...");
	s->audio_ns = 0;
	s->audio_ns_rem = 0;
	s->error_sum_ns = 0;
	s->error_cnt = 0;
	s->state = state_locking;
}

static void enter_state_locked(struct source_s *s, uint64_t audio_ts)
{
	int64_t offset = s->error_sum_ns / s->error_cnt;
	blog(LOG_INFO, "locked offset=%f s, ts=%f s, audio_ns=%f s", offset * 1e-9, audio_ts * 1e-9,
	     s->audio_ns * 1e-9);
	s->audio_ns += offset;
	s->error_sum_ns = 0;
	s->error_cnt = 0;
	s->state = state_locked;
	lag_lead_filter_reset(&s->lf);
	s->cnt_untouched_frames = 0;
}
#endif

static inline int64_t abs_s64(int64_t x)
{
	return x < 0 ? -x : x;
}

static inline int64_t clip_s64(int64_t x, int64_t limit)
{
	if (x < -limit)
		return -limit;
	if (x > limit)
		return limit;
	return x;
}

#ifdef USE_NEXT_AUDIO_TS
static void audio_capture_callback(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	UNUSED_PARAMETER(muted);
	struct source_s *s = param;

	/*
	 * When there is no more filters, `audio_data->timestamp` should be
	 * same as the `timestamp` returned by `async_filter_audio`.
	 * However, the plugin cannot block more filters at the later stage,
	 * use this callback to get the final timestamp.
	 */

	if (audio_data->timestamp && s->expected_audio_ts) {
		s->last_error_ns = (int64_t)audio_data->timestamp - (int64_t)s->expected_audio_ts;
	}
	s->expected_audio_ts = obs_source_get_next_audio_timestamp(source);
}

static void add_callback(struct source_s *s)
{
	obs_source_t *parent = obs_filter_get_parent(s->context);
	if (!parent)
		return;

	obs_source_add_audio_capture_callback(parent, audio_capture_callback, s);
	s->parent = parent;
	s->callback_added = true;
}
#endif // USE_NEXT_AUDIO_TS

static struct obs_audio_data *async_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct source_s *s = data;

	if (!audio)
		return NULL;

	if (!s->rs)
		s->rs = resampler_create(NULL);

#ifdef USE_NEXT_AUDIO_TS
	if (!s->callback_added)
		add_callback(s);
#endif // USE_NEXT_AUDIO_TS

	const uint32_t fs = audio_output_get_sample_rate(obs_get_audio());
	uint64_t ts = s->use_obs_time ? os_gettime_ns() - audio->frames * 1000000000LL / fs : audio->timestamp;

	s->audio_buffer = *audio;
	s->audio_buffer.frames = resampler_audio(s->rs, (float **)s->audio_buffer.data, audio->frames, &ts);
	s->audio_buffer.timestamp = ts;

#ifndef USE_NEXT_AUDIO_TS
	int64_t e = (int64_t)ts - (int64_t)s->audio_ns;
#else
	int64_t e = s->last_error_ns;
#endif

#ifndef USE_NEXT_AUDIO_TS
	if (s->state == state_start) {
		if (s->audio_ns >= STARTUP_TIMEOUT_NS)
			enter_state_locking(s);
	}
	else if (s->state == state_locking) {
		s->error_sum_ns += e;
		s->error_cnt += 1;

		if (s->audio_ns >= LOCKING_TIME_NS)
			enter_state_locked(s, ts);
	}
	else if (s->state == state_locked) {
#endif
		if (abs_s64(e) >= TS_SMOOTHING_THRESHOLD) {
			blog(LOG_WARNING, "lock failed due to large error, %f us", e * 1e-3);
#ifndef USE_NEXT_AUDIO_TS
			s->audio_ns = 0;
			s->audio_ns_rem = 0;
			s->state = state_start;
#else
		lag_lead_filter_reset(&s->lf);
#endif
			goto end;
		}

		int64_t e_clip = clip_s64(e, audio->frames * 1000000000LL / fs);

		lag_lead_filter_set_error_ns(&s->lf, e_clip);

		lag_lead_filter_tick(&s->lf, fs, audio->frames);

		double comp = lag_lead_filter_get_drift(&s->lf);
		// Add `comp` frames for every seconds

		resampler_compensate(s->rs, comp);
#ifndef USE_NEXT_AUDIO_TS
	}
#endif

	int add_remove = (int)s->audio_buffer.frames - (int)audio->frames;

#ifndef USE_NEXT_AUDIO_TS
	if (add_remove && s->state == state_locked) {
#else
	if (add_remove) {
#endif
		double comp = lag_lead_filter_get_drift(&s->lf);
		if (s->verbosity >= 1)
			blog(LOG_INFO,
			     "%s %d frame(s), identified error is %.1f ppm, e=%.2f ms vc1=%f vc2=%f output-ts=%.06f input-ts=%.06f",
			     add_remove > 0 ? "added" : "removed", add_remove > 0 ? add_remove : -add_remove,
			     comp * 1e6 / fs, e * 1e-6, s->lf.vc1, s->lf.vc2, ts * 1e6, audio->timestamp * 1e6);
		s->cnt_untouched_frames = 0;
	}
	else {
		s->cnt_untouched_frames += audio->frames;
	}

	if (s->verbosity >= 2 && (add_remove || (s->cnt_untouched_frames / audio->frames % 128) == 0))
		blog(LOG_INFO, "vc1: %f vc2: %f", s->lf.vc1, s->lf.vc2);

#ifndef USE_NEXT_AUDIO_TS
	if (s->audio_buffer.frames) {
		uint64_t x = 1000000000LL * s->audio_buffer.frames + s->audio_ns_rem;
		s->audio_ns += x / fs;
		s->audio_ns_rem = x % fs;
	}
#endif

end:
	return &s->audio_buffer;
}

static void *create(obs_data_t *settings, obs_source_t *source)
{
	struct source_s *s = bzalloc(sizeof(struct source_s));
	s->context = source;

	lag_lead_filter_reset(&s->lf);

	update(s, settings);

	return s;
}

static void destroy(void *data)
{
	struct source_s *s = data;

#ifdef USE_NEXT_AUDIO_TS
	if (s->callback_added)
		obs_source_remove_audio_capture_callback(s->parent, audio_capture_callback, s);
#endif

	if (s->rs)
		resampler_destroy(s->rs);

	bfree(s);
}

const struct obs_source_info asynchronous_audio_info = {
	.id = ID_PREFIX "asynchronous-audio-filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = get_name,
	.create = create,
	.destroy = destroy,
	.update = update,
	.get_properties = get_properties,
	.get_defaults = get_defaults,
	.filter_audio = async_filter_audio,
};
