
.subckt model_openloop out_ns error_ns

* charge pump
Gcp n_c1 0 error_ns 0 'gain * 1e-9'

* lag-lead filter
Clf1 n_c1 0 'c1 / fs'
Clf2 n_c2 0 'c2 / fs'
Rlf1 n_c1 n_c2 'fs'

* VCO
Gvco phase 0 n_c1 0 '1'
Cvco phase 0 '1'

* phase [rad] -> time [s]
Eout out_ns 0 phase 0 '1e9/fs'

.ends
