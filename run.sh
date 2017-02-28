#! /bin/bash -eu
cleanup() {
	exit 1
}
trap cleanup SIGTERM SIGINT

sleep_time=5000000000

duration=0
tot_nb_iter=0
nb_events=0
max_mem=0
discard_events=0

run_baseline() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
        #run the testcase twice to warm the cache
	output=$(taskset -c 0 ./$testcase $cpu_affinity $nbThreads $sleepTime)
	output=$(taskset -c 0 ./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo "$output" | cut -f1 -d ' ')
	tot_nb_iter=$(echo "$output" | cut -f2 -d ' ')
	nb_events="na"
	discard_events="na"
}

run_lttng() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	discard_events=0
	LOCK=1

	trap 'LOCK=0' SIGUSR1

	lttng-sessiond --sig-parent &
	while [[ $LOCK -eq 1 ]]
	 do
		sleep 1
	done

	lttng create --snapshot --output="$(mktemp -d --tmpdir=/tmp/)"
	lttng enable-channel --num-subbuf 512 --subbuf-size 64k --kernel my_channel
	lttng enable-event -k sched_process_exit,sched_switch,signal_deliver --channel my_channel
	lttng enable-event -k --syscall --all --channel my_channel
	lttng start
	# we run the testcase twice to ensure the tracer's and testcase's pages are warm
	output=$(taskset -c 0 ./$testcase $cpu_affinity $nbThreads $sleepTime)

	output=$(taskset -c 0 ./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo "$output" | cut -f1 -d ' ')
	tot_nb_iter=$(echo "$output" | cut -f2 -d ' ')
	discard_events=$(lttng stop | grep 'warning' | awk '{print $2}')
	discard_events="na"
	sleep 1

#Dont count the number of event to reduce runtime
	#nb_events=$(lttng view | wc -l)
	nb_events="na"
	lttng destroy -a
	killall lttng-sessiond
	wait
}

drop_caches() {
	echo 3 > /proc/sys/vm/drop_caches
}


file_output=./results.csv
if [ -z ${1+x} ]; then
	testcase_to_run="failing-open-enoent"
else
	testcase_to_run=$1
fi

echo 'testcase,tracer,run,sleeptime,cpu_affinity,nbthreads,duration,nbiter,nbevents,discarded,maxmem' > $file_output
for nthreads in 1 2 4 8 16; do
	for cpuaffinity in 1; do
		for tcase in $(echo $testcase_to_run); do
			for tracer in baseline lttng ; do
			        # Run the testcase once to warm the caches
			        # and discard the results
				for i in $(seq 1 5); do
					run_$tracer $tcase $cpuaffinity $nthreads $sleep_time
					echo "$tcase,$tracer,$i,$sleep_time,$cpuaffinity,$nthreads,$duration,$tot_nb_iter,$nb_events,$discard_events,$max_mem" >> $file_output
					echo "$tcase,$tracer,$i,$sleep_time,$cpuaffinity,$nthreads,$duration,$tot_nb_iter,$nb_events,$discard_events,$max_mem"
					sleep 1
				done
			done
		done
	done
done
sync

