Async Audio Filter

.include params.sp

* phase error [sample]
* 'icp' in the code
Verr n_err 0 DC 0 AC 1

.include model.sp

Xmodel n_out n_err model_openloop

.control
ac dec 10 0.11m 9
print vdb(n_out) vp(n_out) > analyze-ac.out
.endc

.end
