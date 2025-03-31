rm ./page/table.html

function run_sable () {
# ./sable --count-det --output=table.html --report-table --input-tlsf=$1
filename=$(basename $1)
echo
echo
echo $filename ...
timeout 1h /usr/bin/time -o script_logs/$filename.time.log -v ./sable --output=table.html --report-table --input-tlsf=$1 > script_logs/$filename.output.log
cat script_logs/$filename.time.log
}

# find benchmarks_ulb -name "*.tlsf" | while read line; do run_sable $line; done
find ../benchmarks/tlsf/generated_tlsf -name "*.tlsf" | sort | while read line; do run_sable $line; done