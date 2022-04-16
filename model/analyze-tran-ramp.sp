Async Audio Filter - Transition analysis

.include params.sp

* phase error [sample]
* 'icp' in the code
* Verr n_err 0 DC 0 pwl((0 0) (0.01 0) (0.02 1))
Verr n_err 0 DC 0 pwl((0 0) (1000 -1))

Ediff n_diff 0 n_err n_out '1'

.include model.sp

Xmodel n_out n_diff model_openloop

.control
tran 0.1 390
print v(n_out) v(xmodel.n_c1), v(Xmodel.n_c2) > analyze-tran-ramp.out
.endc

.end

