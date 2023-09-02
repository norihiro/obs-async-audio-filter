#include <inttypes.h>
#include <obs-module.h>
#include <util/platform.h>
#include "plugin-macros.generated.h"
#include "lag_lead_filter.h"

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
	enum state_e state;

	struct lag_lead_filter lf;

	uint64_t audio_ns;
	int audio_ns_rem;

	int64_t error_sum_ns;
	int error_cnt;

	uint64_t locked_ns;
	double phase;

	int add_remove_prev;

	uint32_t cnt_untouched_frames;

	struct obs_audio_data audio_buffer;
	float last_audio[MAX_AV_PLANES];
	size_t audio_buffer_size;
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

static void store_last_audio(struct source_s *s, const struct obs_audio_data *audio)
{
	if (audio->frames < 1)
		return;

	for (int i = 0; i < MAX_AV_PLANES && audio->data[i]; i++)
		s->last_audio[i] = FLTP(audio->data, i, audio->frames - 1);
}

static inline struct obs_audio_data *add_frame(struct source_s *s, struct obs_audio_data *audio)
{
	if (s->audio_buffer_size < audio->frames + 1) {
		for (int i = 0; i < MAX_AV_PLANES && audio->data[i]; i++) {
			bfree(s->audio_buffer.data[i]);
			s->audio_buffer.data[i] = NULL;
		}
		s->audio_buffer_size = audio->frames + 1;
	}

	for (int i = 0; i < MAX_AV_PLANES && audio->data[i]; i++) {
		if (!s->audio_buffer.data[i])
			s->audio_buffer.data[i] = bmalloc(s->audio_buffer_size * sizeof(float));

		const float *src = (const float *)audio->data[i];
		float *dst = (float *)s->audio_buffer.data[i];

		dst[0] = (s->last_audio[i] + src[0]) * 0.5f;

		for (size_t j = 0; j < audio->frames; j++)
			dst[j + 1] = src[j];
	}

	s->audio_buffer.frames = audio->frames + 1;
	s->audio_buffer.timestamp = audio->timestamp;
	// If there is a new member, this assertion will fail at compile time.
	// If failed, need to copy the new member, then update the condition below.
	CASSERT(sizeof(*audio) <= sizeof(void *) * MAX_AV_PLANES + 4 + 8 + 4);

	return &s->audio_buffer;
}

static inline struct obs_audio_data *del_frame(struct source_s *s, struct obs_audio_data *audio)
{
	UNUSED_PARAMETER(s);

	if (audio->frames <= 1)
		return NULL;

	for (int i = 0; i < MAX_AV_PLANES && audio->data[i]; i++) {
		float f = (FLTP(audio->data, i, audio->frames - 2) + FLTP(audio->data, i, audio->frames - 1)) * 0.5f;
		FLTP(audio->data, i, audio->frames - 2) = f;
	}
	audio->frames--;

	return audio;
}

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
	s->locked_ns = s->audio_ns;
	lag_lead_filter_reset(&s->lf);
	s->phase = 0.0;
	s->cnt_untouched_frames = 0;
	s->add_remove_prev = 0;
}

static inline int64_t abs_s64(int64_t x)
{
	return x < 0 ? -x : x;
}

static struct obs_audio_data *async_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct source_s *s = data;

	const uint32_t fs = audio_output_get_sample_rate(obs_get_audio());
	uint64_t ts = s->use_obs_time ? os_gettime_ns() - audio->frames * 1000000000LL / fs : audio->timestamp;

	if (s->state == state_start) {
		if (s->audio_ns >= STARTUP_TIMEOUT_NS)
			enter_state_locking(s);
	}
	else if (s->state == state_locking) {
		int64_t e = (int64_t)ts - (int64_t)s->audio_ns;
		s->error_sum_ns += e;
		s->error_cnt += 1;

		if (s->audio_ns >= LOCKING_TIME_NS)
			enter_state_locked(s, ts);
	}
	else if (s->state == state_locked) {
		int64_t e = (int64_t)ts - (int64_t)s->audio_ns;

		if (abs_s64(e) >= TS_SMOOTHING_THRESHOLD) {
			blog(LOG_WARNING, "lock failed due to large error, %f us", e * 1e-3);
			s->audio_ns = 0;
			s->audio_ns_rem = 0;
			s->state = state_start;
			goto end;
		}

		int64_t e_max = audio->frames * 1000000000LL / fs;
		if (abs_s64(e) > e_max) {
			if (e < 0)
				e = -e_max;
			else
				e = e_max;
		}

		lag_lead_filter_set_error_ns(&s->lf, e);

		lag_lead_filter_tick(&s->lf, fs, audio->frames);

		s->phase += lag_lead_filter_get_drift(&s->lf) * audio->frames / fs;

		double threshold = 1.0; // [sample]

		int add_remove = 0;

		if (s->phase > threshold) {
			add_remove = 1;
			audio = add_frame(s, audio);
			s->phase -= threshold;
		}
		else if (-s->phase > threshold) {
			add_remove = -1;
			audio = del_frame(s, audio);
			s->phase += threshold;
		}

		if (add_remove) {
			if (s->verbosity >= 1)
				blog(LOG_INFO, "%s one frame, identified error is %.1f ppm, e=%.2f ms vc1=%f vc2=%f",
				     add_remove > 0 ? "adding" : "removing",
				     1e6 / (s->cnt_untouched_frames + audio->frames), e * 1e-6, s->lf.vc1, s->lf.vc2);
			s->cnt_untouched_frames = 0;
			s->add_remove_prev = add_remove;
		}
		else {
			s->cnt_untouched_frames += audio->frames;
		}

		if (s->verbosity >= 2 && (add_remove || (s->cnt_untouched_frames / audio->frames % 128) == 0))
			blog(LOG_INFO, "vc1: %f vc2: %f phase: %f", s->lf.vc1, s->lf.vc2, s->phase);
	}

	if (audio) {
		uint64_t x = 1000000000LL * audio->frames + s->audio_ns_rem;
		s->audio_ns += x / fs;
		s->audio_ns_rem = x % fs;
	}

end:
	if (audio)
		store_last_audio(s, audio);
	return audio;
}

static void *create(obs_data_t *settings, obs_source_t *source)
{
	struct source_s *s = bzalloc(sizeof(struct source_s));
	s->context = source;

	update(s, settings);

	return s;
}

static void destroy(void *data)
{
	struct source_s *s = data;

	for (int i = 0; i < MAX_AV_PLANES; i++)
		bfree(s->audio_buffer.data[i]);

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
