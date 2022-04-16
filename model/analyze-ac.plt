#! /usr/bin/gnuplot

set terminal svg
set output "obs-async-audio-filter-ac.svg"
set multiplot layout 2,1
set logscale x
set xlabel 'Frequency [Hz]'

set ylabel 'Gain [dB]'
plot '< grep "^[0-9]" analyze-ac.out' u 2:3 w l t '' ,\
0 t '' lc black

set ylabel 'Phase [deg]'
plot '< grep "^[0-9]" analyze-ac.out' u 2:($4*180/3.14159265359) w l t ''

