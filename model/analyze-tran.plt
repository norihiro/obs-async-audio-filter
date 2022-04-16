#! /usr/bin/gnuplot

set terminal svg
set output "obs-async-audio-filter-tran-ramp.svg"
set key right bottom
set xlabel 'Time [s]'
set ylabel 'Closed loop response [a.u.]'
plot '< grep "^[0-9]" analyze-tran-ramp.out' u 2:(-$3) w l t 'ramp response' , x*1e-3 t 'input'

set output "obs-async-audio-filter-tran-step.svg"
plot '< grep "^[0-9]" analyze-tran-step.out' u 2:3 w l t 'step response'

