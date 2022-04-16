
.param fs = 48k
* .param c1=2 c2=190.957466 gain=0.02

.param q = '3.74'
.param fu = '1 / 180'

.param wu = 'fu * 6.283'
.param gain = 'fu * 6.76'
.param wz = 'wu / q'
.param wp = 'wu * q'
.param c2 = '1 / wz'
.param c1 = 'c2 / (wp * c2 - 1)'
