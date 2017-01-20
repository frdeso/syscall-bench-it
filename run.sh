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
tracker_pid=0
discard_events=0
run_mem_tracker=0

MEM_USAGE_FILE=$(mktemp)

start_mem_tracker() {
	cp /dev/null $MEM_USAGE_FILE
	#top -b  | grep 'KiB Swap' | awk '{print $9}' >> $MEM_USAGE_FILE &
	bash -c "while true; do cat /proc/meminfo | grep -E '^Cached:'|awk '{print \$2}' >> $MEM_USAGE_FILE; sleep 1; done" &
	tracker_pid=$!
}

update_max_mem_usage() {
	kill -9 $tracker_pid
	wait $tracker_pid

	max_mem=0
	for tmp in $(cat $MEM_USAGE_FILE); do
		(($tmp > $max_mem)) && max_mem=$tmp
	done
}

run_baseline() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
#	start_mem_tracker
        #run the testcase twice to warm the cache
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
#	update_max_mem_usage
	duration=$(echo $output | cut -f1 -d ' ')
	tot_nb_iter=$(echo $output | cut -f2 -d ' ')
	nb_events="na"
	discard_events="na"
}
run_strace() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	start_mem_tracker
	output=$(strace -f ./$testcase $cpu_affinity $nbThreads $sleepTime 2> /dev/null)
	update_max_mem_usage
	duration=$(echo $output | cut -f1 -d ' ')
	tot_nb_iter=$(echo $output | cut -f2 -d ' ')
	nb_events=-1
}

run_lttng() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	discard_events=0
	LOCK=1

	trap 'LOCK=0' SIGUSR1

# Removed the memory tracking to limit unneeded interferences
#	start_mem_tracker
	lttng-sessiond --sig-parent &
	while [[ $LOCK -eq 1 ]]
	 do
		sleep 1
	done

	lttng create --snapshot --output=$(mktemp -d --tmpdir=/tmp/)
	lttng enable-channel --num-subbuf 512 --subbuf-size 64k --kernel my_channel
	lttng enable-event -k sched_process_exit,sched_switch,signal_deliver --channel my_channel
	lttng enable-event -k --syscall --all --channel my_channel
	lttng start
	# we run the testcase twice to ensure the tracer's and testcase's pages are warm
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)

	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo $output | cut -f1 -d ' ')
	tot_nb_iter=$(echo $output | cut -f2 -d ' ')
	discard_events=$(lttng stop | grep 'warning' | awk '{print $2}')
	discard_events="na"
	sleep 1
#	update_max_mem_usage

#Dont count the number of event to reduce runtime
	#nb_events=$(lttng view | wc -l)
	nb_events="na"
	lttng destroy -a
	killall lttng-sessiond
	wait
}
run_sysdig() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	tmp_file=$(mktemp --tmpdir=/root/tmp/)

	start_mem_tracker
	sysdig -w $tmp_file &
	#save sysdig's pid
	p=$!
	sleep 2
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo $output | cut -f1 -d ' ')
	tot_nb_iter=$(echo $output | cut -f2 -d ' ')
	sleep 1
	update_max_mem_usage

	# Send sigint to sysdig and wait for it to exit
	kill -2 $p
	wait $p

	#sync to make sure the entire trace is written to disk
	sync

	# extract the number of events from the read verbose output
	sysdig_output=$(sysdig -r $tmp_file -v 2>&1 >/dev/null)

	nb_events=$(echo $sysdig_output |grep 'Elapsed'|awk '{print $10}'|sed 's/,//g')
	discard_events=$(echo $sysdig_output |grep 'Drops' | awk '{print $4}' | cut -f2 -d:)
	echo $sysdig_output | grep 'Driver'
	rm $tmp_file
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
					echo $tcase,$tracer,$i,$sleep_time,$cpuaffinity,$nthreads,$duration,$tot_nb_iter,$nb_events,$discard_events,$max_mem >> $file_output
					echo $tcase,$tracer,$i,$sleep_time,$cpuaffinity,$nthreads,$duration,$tot_nb_iter,$nb_events,$discard_events,$max_mem
					sleep 1
				done
			done
		done
	done
done
sync

