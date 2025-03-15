rm ./page/table.html

function run_sable () {
./sable --count-det --output=table.html --report-table --input-tlsf=$1
}

find ../benchmarks_ulb -name "*.tlsf" | while read line; do run_sable $line; done