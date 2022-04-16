#! /bin/awk -f

BEGIN {
	OFS = "\t"
}
/^[0-9]/ {
	if ($3 > 0) {
		fpos = $2
		gpos = $3
		ppos = $4
	} else if (fpos) {
		fneg = $2
		gneg = $3
		pneg = $4
		f0 = (fneg * gpos - fpos * gneg) / (gpos - gneg)
		p0 = (pneg * gpos - ppos * gneg) / (gpos - gneg)
		print f0, 1.0 / f0, p0 * 180 / 3.14159265359 + 180
		fpos = 0
	}
}
