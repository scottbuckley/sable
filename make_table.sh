rm ./page/table.html

function run_sable () {
./sable --count-det --output=table.html --report-table --input-tlsf=$1
}

run_sable ../benchmarks_ulb/strix_detector_1.tlsf
run_sable ../benchmarks_ulb/strix_detector_2.tlsf
run_sable ../benchmarks_ulb/strix_detector_3.tlsf
run_sable ../benchmarks_ulb/strix_detector_4.tlsf

# run_sable ../benchmarks_ulb/narylatch_4.tlsf
# run_sable ../benchmarks_ulb/narylatch_5.tlsf
# run_sable ../benchmarks_ulb/narylatch_6.tlsf

run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_2.tlsf
run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_3.tlsf
run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_4.tlsf
# run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_5.tlsf
# run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_6.tlsf
# run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_7.tlsf
# run_sable ../benchmarks_ulb/arbiter/parametric/simple_arbiter_8.tlsf