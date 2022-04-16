
MODEL_FILES=model.sp params.sp

all: \
	analyze-ac.out \
	analyze-tran-ramp.out \
	analyze-ac-unity.out \
	obs-async-audio-filter-ac.svg \
	obs-async-audio-filter-tran-ramp.svg \
	obs-async-audio-filter-tran-step.svg

.SUFFIXES: .out .sp

%.out: %.sp ${MODEL_FILES}
	ngspice -p $< < /dev/null

analyze-ac-unity.out: analyze-ac.out analyze-ac-unity.awk
	./analyze-ac-unity.awk analyze-ac.out > $@

obs-async-audio-filter-ac.svg: analyze-ac.plt analyze-ac.out
	gnuplot analyze-ac.plt

obs-async-audio-filter-tran-ramp.svg obs-async-audio-filter-tran-step.svg: analyze-tran.plt analyze-tran-ramp.out analyze-tran-step.out
	gnuplot analyze-tran.plt
